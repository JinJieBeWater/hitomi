#include "app/runtime_lifecycle_service.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_ota_ops.h>

#include <algorithm>
#include <atomic>
#include <cstdio>

#include "app/runtime_diagnostics.hpp"
#include "app/runtime_network_ops.hpp"
#include "app/runtime_state.hpp"
#include "app/runtime_storage_ops.hpp"
#include "board/app_config.hpp"
#include "board/pins.hpp"
#include "core/use_cases.hpp"
#include "infra/local_store.hpp"

namespace app {

namespace {

std::string buildDefaultDeviceSerial() {
  char serial[32] = {};
  std::snprintf(serial, sizeof(serial), "SZPI-%llX", static_cast<unsigned long long>(ESP.getEfuseMac()));
  return serial;
}

struct WifiDriverEventBridge {
  std::atomic<uint32_t> sequence{0};
  std::atomic<int> eventId{ARDUINO_EVENT_NONE};
  std::atomic<int> disconnectReason{0};
};

WifiDriverEventBridge gWifiDriverEventBridge;
wifi_event_id_t gWifiEventHandle = 0;

void onWifiDriverEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  int reason = 0;
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    reason = info.wifi_sta_disconnected.reason;
  }

  gWifiDriverEventBridge.disconnectReason.store(reason, std::memory_order_relaxed);
  gWifiDriverEventBridge.eventId.store(static_cast<int>(event), std::memory_order_relaxed);
  gWifiDriverEventBridge.sequence.fetch_add(1, std::memory_order_release);
}

bool isAuthFailureReason(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_802_1X_AUTH_FAILED:
      return true;
    default:
      return false;
  }
}

std::string formatBssid(const uint8_t* bssid) {
  if (bssid == nullptr) {
    return "";
  }

  char formatted[18] = {};
  std::snprintf(
      formatted,
      sizeof(formatted),
      "%02X:%02X:%02X:%02X:%02X:%02X",
      bssid[0],
      bssid[1],
      bssid[2],
      bssid[3],
      bssid[4],
      bssid[5]);
  return formatted;
}

std::optional<std::array<uint8_t, 6>> parseBssid(const std::string& value) {
  if (value.empty()) {
    return std::nullopt;
  }

  int parsed[6] = {};
  if (std::sscanf(
          value.c_str(),
          "%02x:%02x:%02x:%02x:%02x:%02x",
          &parsed[0],
          &parsed[1],
          &parsed[2],
          &parsed[3],
          &parsed[4],
          &parsed[5]) != 6) {
    return std::nullopt;
  }

  std::array<uint8_t, 6> bytes = {};
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    bytes[index] = static_cast<uint8_t>(parsed[index]);
  }
  return bytes;
}

void syncWifiRuntimeState(RuntimeState& state) {
  if (state.wifiProfileRuntime.size() != state.deviceConfig.wifiProfiles.size()) {
    state.wifiProfileRuntime.resize(state.deviceConfig.wifiProfiles.size());
  }
}

bool wifiProfileInCooldown(const RuntimeState& state, std::size_t profileIndex, uint32_t nowMs) {
  return profileIndex < state.wifiProfileRuntime.size() &&
         state.wifiProfileRuntime[profileIndex].cooldownUntilMs > nowMs;
}

void clearWifiCandidates(RuntimeState& state) {
  state.wifiCandidates.clear();
  state.wifiCandidateCursor = 0;
}

void resetWifiAttemptRound(RuntimeState& state) {
  clearWifiCandidates(state);
  state.wifiShouldTryLastKnownGood = true;
  state.wifiConfiguredFallbackAttempted = false;
  state.lastWifiRetryRoundMs = 0;
}

std::optional<std::size_t> findSuccessfulWifiProfile(const RuntimeState& state, uint32_t nowMs) {
  const std::vector<std::size_t> rankedProfiles = core::rankWifiProfiles(state.deviceConfig.wifiProfiles);
  for (const auto profileIndex : rankedProfiles) {
    const auto& profile = state.deviceConfig.wifiProfiles[profileIndex];
    if (profile.lastSuccessAt == 0) {
      continue;
    }
    if (wifiProfileInCooldown(state, profileIndex, nowMs)) {
      continue;
    }
    return profileIndex;
  }
  return std::nullopt;
}

std::optional<std::size_t> findConfiguredFallbackProfile(const RuntimeState& state, uint32_t nowMs) {
  const std::vector<std::size_t> rankedProfiles = core::rankWifiProfiles(state.deviceConfig.wifiProfiles);
  for (const auto profileIndex : rankedProfiles) {
    if (!state.deviceConfig.wifiProfiles[profileIndex].configured()) {
      continue;
    }
    if (wifiProfileInCooldown(state, profileIndex, nowMs)) {
      continue;
    }
    return profileIndex;
  }
  return std::nullopt;
}

void registerWifiAttemptFailure(RuntimeState& state, uint32_t nowMs, std::optional<uint8_t> reason) {
  if (!state.activeWifiProfileIndex.has_value()) {
    return;
  }

  syncWifiRuntimeState(state);
  const std::size_t profileIndex = state.activeWifiProfileIndex.value();
  if (profileIndex < state.wifiProfileRuntime.size()) {
    state.wifiProfileRuntime[profileIndex].lastDisconnectReason = reason;
    if (reason.has_value() && isAuthFailureReason(reason.value())) {
      state.wifiProfileRuntime[profileIndex].cooldownUntilMs = nowMs + board::kWifiAuthFailureCooldownMs;
      Serial.printf(
          "[APP] profile SSID=%s cooling down for auth failure until=%lu\n",
          state.deviceConfig.wifiProfiles[profileIndex].ssid.c_str(),
          static_cast<unsigned long>(state.wifiProfileRuntime[profileIndex].cooldownUntilMs));
    }
  }

  state.activeWifiProfileIndex.reset();
}

bool isBetterScanCandidate(
    const core::WifiProfile& profile,
    const WifiScanCandidate& candidate,
    const WifiScanCandidate& current) {
  if (candidate.matchesLastKnownGood != current.matchesLastKnownGood) {
    return candidate.matchesLastKnownGood;
  }

  const bool candidateMatchesChannel =
      profile.lastSuccessChannel != 0 && candidate.channel == profile.lastSuccessChannel;
  const bool currentMatchesChannel =
      profile.lastSuccessChannel != 0 && current.channel == profile.lastSuccessChannel;
  if (candidateMatchesChannel != currentMatchesChannel) {
    return candidateMatchesChannel;
  }

  if (candidate.rssi != current.rssi) {
    return candidate.rssi > current.rssi;
  }

  return candidate.channel < current.channel;
}

void refreshWifiScanCandidates(RuntimeState& state, uint32_t nowMs) {
  state.wifiScanInProgress = false;
  state.lastWifiScanCompletedMs = nowMs;
  clearWifiCandidates(state);

  const int scanResult = WiFi.scanComplete();
  if (scanResult <= 0) {
    WiFi.scanDelete();
    return;
  }

  syncWifiRuntimeState(state);
  std::vector<std::optional<WifiScanCandidate>> bestByProfile(state.deviceConfig.wifiProfiles.size());

  for (int index = 0; index < scanResult; ++index) {
    String scannedSsid;
    uint8_t scannedAuthMode = 0;
    int32_t scannedRssi = 0;
    uint8_t* scannedBssid = nullptr;
    int32_t scannedChannel = 0;
    if (!WiFi.getNetworkInfo(index, scannedSsid, scannedAuthMode, scannedRssi, scannedBssid, scannedChannel)) {
      continue;
    }
    if (scannedSsid.isEmpty()) {
      continue;
    }

    const std::string ssid = scannedSsid.c_str();
    for (std::size_t profileIndex = 0; profileIndex < state.deviceConfig.wifiProfiles.size(); ++profileIndex) {
      const auto& profile = state.deviceConfig.wifiProfiles[profileIndex];
      if (!profile.configured() || profile.ssid != ssid || wifiProfileInCooldown(state, profileIndex, nowMs)) {
        continue;
      }
      if (scannedAuthMode == WIFI_AUTH_OPEN && !profile.password.empty()) {
        continue;
      }

      WifiScanCandidate candidate = {
          .profileIndex = profileIndex,
          .rssi = scannedRssi,
          .channel = 0,
          .authMode = scannedAuthMode,
          .bssid = {},
          .hasBssid = scannedBssid != nullptr,
          .matchesLastKnownGood = false,
      };
      if (scannedChannel > 0) {
        candidate.channel = static_cast<uint8_t>(scannedChannel);
      }

      if (candidate.hasBssid) {
        std::copy(scannedBssid, scannedBssid + candidate.bssid.size(), candidate.bssid.begin());
        candidate.matchesLastKnownGood =
            !profile.lastSuccessBssid.empty() && formatBssid(candidate.bssid.data()) == profile.lastSuccessBssid;
      }

      if (!bestByProfile[profileIndex].has_value() ||
          isBetterScanCandidate(profile, candidate, bestByProfile[profileIndex].value())) {
        bestByProfile[profileIndex] = candidate;
      }
    }
  }

  WiFi.scanDelete();

  for (const auto& candidate : bestByProfile) {
    if (candidate.has_value()) {
      state.wifiCandidates.push_back(candidate.value());
    }
  }

  std::sort(state.wifiCandidates.begin(), state.wifiCandidates.end(), [&](const WifiScanCandidate& left, const WifiScanCandidate& right) {
    const auto& leftProfile = state.deviceConfig.wifiProfiles[left.profileIndex];
    const auto& rightProfile = state.deviceConfig.wifiProfiles[right.profileIndex];
    if (leftProfile.priority != rightProfile.priority) {
      return leftProfile.priority > rightProfile.priority;
    }
    if (leftProfile.lastSuccessAt != rightProfile.lastSuccessAt) {
      return leftProfile.lastSuccessAt > rightProfile.lastSuccessAt;
    }
    if (left.matchesLastKnownGood != right.matchesLastKnownGood) {
      return left.matchesLastKnownGood;
    }
    if (left.rssi != right.rssi) {
      return left.rssi > right.rssi;
    }
    return leftProfile.ssid < rightProfile.ssid;
  });
}

void applyConnectedState(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  const bool wasConnected = state.connectivity == ConnectivityState::Connected;
  state.wifiConnectInProgress = false;
  state.wifiScanInProgress = false;
  state.wifiShouldTryLastKnownGood = true;
  state.wifiConfiguredFallbackAttempted = false;
  state.lastWifiRetryRoundMs = 0;
  state.lastWifiDisconnectReason.reset();

  const std::string ssid = std::string(WiFi.SSID().c_str());
  const std::string bssid = WiFi.BSSIDstr().c_str();
  const int32_t currentChannel = WiFi.channel();
  const uint8_t channel = currentChannel > 0 ? static_cast<uint8_t>(currentChannel) : 0;

  clearWifiCandidates(state);
  syncWifiRuntimeState(state);

  std::optional<std::size_t> matchedProfileIndex = state.activeWifiProfileIndex;
  if ((!matchedProfileIndex.has_value() || matchedProfileIndex.value() >= state.deviceConfig.wifiProfiles.size()) && !ssid.empty()) {
    for (std::size_t index = 0; index < state.deviceConfig.wifiProfiles.size(); ++index) {
      if (state.deviceConfig.wifiProfiles[index].ssid == ssid) {
        matchedProfileIndex = index;
        break;
      }
    }
  }

  bool profileMetadataChanged = false;
  if (matchedProfileIndex.has_value() && matchedProfileIndex.value() < state.deviceConfig.wifiProfiles.size()) {
    auto& profile = state.deviceConfig.wifiProfiles[matchedProfileIndex.value()];
    profileMetadataChanged =
        !wasConnected || profile.lastSuccessAt == 0 || profile.lastSuccessBssid != bssid ||
        profile.lastSuccessChannel != channel;
    if (profileMetadataChanged) {
      profile.lastSuccessAt = nowMs;
      profile.lastSuccessBssid = bssid;
      profile.lastSuccessChannel = channel;
      profile.disabled = false;
    }

    if (matchedProfileIndex.value() < state.wifiProfileRuntime.size()) {
      state.wifiProfileRuntime[matchedProfileIndex.value()].cooldownUntilMs = 0;
      state.wifiProfileRuntime[matchedProfileIndex.value()].lastDisconnectReason.reset();
    }
  } else if (!ssid.empty()) {
    profileMetadataChanged = !wasConnected;
    if (profileMetadataChanged) {
      core::markWifiProfileSuccess(state.deviceConfig.wifiProfiles, ssid, nowMs);
    }
  }

  state.activeWifiProfileIndex.reset();

  const bool ssidChanged = !state.activeWifiSsid.has_value() || state.activeWifiSsid.value() != ssid;
  if (!ssid.empty()) {
    state.activeWifiSsid = ssid;
    if (ssidChanged) {
      state.renderDirty = true;
    }
  }

  if (profileMetadataChanged) {
    persistDeviceConfig(context, state);
  }

  if (!wasConnected) {
    state.connectivity = ConnectivityState::Connected;
    state.renderDirty = true;
    resetNetworkTaskSchedule(state);
  }
}

void applyDisconnectedState(RuntimeState& state, uint32_t nowMs, std::optional<uint8_t> reason) {
  registerWifiAttemptFailure(state, nowMs, reason);
  invalidatePendingNetworkRequests(state);
  state.wifiConnectInProgress = false;
  state.lastWifiDisconnectReason = reason;

  if (state.activeWifiSsid.has_value()) {
    state.activeWifiSsid.reset();
    state.renderDirty = true;
  }
  if (state.connectivity != ConnectivityState::Disconnected) {
    state.connectivity = ConnectivityState::Disconnected;
    state.renderDirty = true;
  }
}

void startWifiConnectionAttempt(
    RuntimeState& state,
    std::size_t profileIndex,
    uint32_t nowMs,
    const char* strategy,
    uint8_t channel = 0,
    const uint8_t* bssid = nullptr) {
  const auto& profile = state.deviceConfig.wifiProfiles[profileIndex];
  state.lastWifiConnectAttemptMs = nowMs;
  state.wifiConnectInProgress = true;
  state.activeWifiProfileIndex = profileIndex;
  state.lastWifiDisconnectReason.reset();
  state.renderDirty = true;

  if (bssid != nullptr || channel != 0) {
    Serial.printf(
        "[APP] connecting WiFi strategy=%s SSID=%s channel=%u bssid=%s\n",
        strategy,
        profile.ssid.c_str(),
        static_cast<unsigned>(channel),
        formatBssid(bssid).c_str());
    WiFi.begin(profile.ssid.c_str(), profile.password.c_str(), channel, bssid);
    return;
  }

  Serial.printf("[APP] connecting WiFi strategy=%s SSID=%s\n", strategy, profile.ssid.c_str());
  WiFi.begin(profile.ssid.c_str(), profile.password.c_str());
}

bool tryLastKnownGoodConnection(RuntimeState& state, uint32_t nowMs) {
  if (!state.wifiShouldTryLastKnownGood) {
    return false;
  }

  const auto profileIndex = findSuccessfulWifiProfile(state, nowMs);
  state.wifiShouldTryLastKnownGood = false;
  if (!profileIndex.has_value()) {
    return false;
  }

  const auto& profile = state.deviceConfig.wifiProfiles[profileIndex.value()];
  const auto bssid = parseBssid(profile.lastSuccessBssid);
  startWifiConnectionAttempt(
      state,
      profileIndex.value(),
      nowMs,
      "last-known-good",
      profile.lastSuccessChannel,
      bssid.has_value() ? bssid->data() : nullptr);
  return true;
}

bool tryNextWifiCandidate(RuntimeState& state, uint32_t nowMs) {
  while (state.wifiCandidateCursor < state.wifiCandidates.size()) {
    const WifiScanCandidate candidate = state.wifiCandidates[state.wifiCandidateCursor++];
    if (!state.deviceConfig.wifiProfiles[candidate.profileIndex].configured() ||
        wifiProfileInCooldown(state, candidate.profileIndex, nowMs)) {
      continue;
    }

    startWifiConnectionAttempt(
        state,
        candidate.profileIndex,
        nowMs,
        "scan-candidate",
        candidate.channel,
        candidate.hasBssid ? candidate.bssid.data() : nullptr);
    return true;
  }

  return false;
}

bool tryConfiguredFallbackConnection(RuntimeState& state, uint32_t nowMs) {
  if (state.wifiConfiguredFallbackAttempted) {
    return false;
  }

  state.wifiConfiguredFallbackAttempted = true;
  const auto profileIndex = findConfiguredFallbackProfile(state, nowMs);
  if (!profileIndex.has_value()) {
    return false;
  }

  startWifiConnectionAttempt(state, profileIndex.value(), nowMs, "configured-fallback");
  return true;
}

bool requestWifiScan(RuntimeState& state, uint32_t nowMs) {
  if (state.wifiScanInProgress) {
    return true;
  }
  if (state.lastWifiScanRequestMs != 0 &&
      nowMs - state.lastWifiScanRequestMs < board::kWifiScanRetryIntervalMs) {
    return false;
  }

  const int scanResult = WiFi.scanNetworks(true, false);
  state.lastWifiScanRequestMs = nowMs;
  if (scanResult == WIFI_SCAN_FAILED) {
    Serial.println("[APP] WiFi scan request failed");
    return false;
  }

  Serial.println("[APP] WiFi scan requested");
  state.wifiScanInProgress = true;
  return true;
}

}  // namespace

void waitForSerial() {
  const unsigned long startedAt = millis();
  while (!Serial && millis() - startedAt < board::kSerialWaitTimeoutMs) {
    delay(10);
  }
}

void initWifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  if (gWifiEventHandle == 0) {
    gWifiEventHandle = WiFi.onEvent(onWifiDriverEvent);
  }
}

uint32_t currentWifiDriverEventSequence() {
  return gWifiDriverEventBridge.sequence.load(std::memory_order_acquire);
}

void processWifiDriverEvents(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  const uint32_t nextSequence = gWifiDriverEventBridge.sequence.load(std::memory_order_acquire);
  if (nextSequence == state.lastWifiEventSequence) {
    return;
  }
  state.lastWifiEventSequence = nextSequence;

  const WiFiEvent_t event = static_cast<WiFiEvent_t>(gWifiDriverEventBridge.eventId.load(std::memory_order_relaxed));
  const int reason = gWifiDriverEventBridge.disconnectReason.load(std::memory_order_relaxed);

  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      state.wifiConnectInProgress = true;
      return;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      applyConnectedState(context, state, nowMs);
      return;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (reason != 0) {
        Serial.printf(
            "[APP] WiFi disconnected reason=%d (%s)\n",
            reason,
            WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)));
      }
      applyDisconnectedState(
          state,
          nowMs,
          reason == 0 ? std::nullopt : std::optional<uint8_t>(static_cast<uint8_t>(reason)));
      return;
    default:
      return;
  }
}

void seedDeviceConfigFromBoardDefaults(const RuntimeContext& context, RuntimeState& state) {
  bool changed = false;

  if (state.deviceConfig.bootstrapIdentity.deviceSerial.empty()) {
    state.deviceConfig.bootstrapIdentity.deviceSerial = buildDefaultDeviceSerial();
    changed = true;
  }

  if (state.deviceConfig.wifiProfiles.empty() && board::wifiConfigured()) {
    core::upsertWifiProfile(
        state.deviceConfig.wifiProfiles,
        core::WifiProfile{
            .ssid = board::wifiSsid(),
            .password = board::wifiPassword(),
            .priority = 100,
            .lastSuccessAt = 0,
            .lastSuccessBssid = "",
            .lastSuccessChannel = 0,
            .disabled = false,
        });
    changed = true;
  }

  if (!state.deviceConfig.backendLocator.configured() && board::apiConfigured()) {
    state.deviceConfig.backendLocator.origin = board::apiBaseUrl();
    changed = true;
  }

  if (changed) {
    context.localStore.saveDeviceConfig(state.deviceConfig);
  }
}

void ensureWifiConnection(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  static_cast<void>(context);
  syncWifiRuntimeState(state);

  if (WiFi.status() == WL_CONNECTED) {
    applyConnectedState(context, state, nowMs);
    return;
  }
  if (!state.deviceConfig.wifiConfigured()) {
    return;
  }

  if (state.wifiConnectInProgress) {
    if (nowMs - state.lastWifiConnectAttemptMs < board::kWifiConnectAttemptTimeoutMs) {
      return;
    }

    Serial.println("[APP] WiFi connect attempt timed out");
    WiFi.disconnect(false, false);
    state.wifiConnectInProgress = false;
    applyDisconnectedState(state, nowMs, std::nullopt);
  }

  if (state.wifiScanInProgress) {
    const int scanResult = WiFi.scanComplete();
    if (scanResult == WIFI_SCAN_RUNNING) {
      return;
    }
    refreshWifiScanCandidates(state, nowMs);
  }

  if (tryLastKnownGoodConnection(state, nowMs)) {
    return;
  }

  if (tryNextWifiCandidate(state, nowMs)) {
    return;
  }

  if (state.wifiCandidates.empty() && requestWifiScan(state, nowMs)) {
    return;
  }

  if (tryConfiguredFallbackConnection(state, nowMs)) {
    return;
  }

  if (state.lastWifiRetryRoundMs == 0) {
    state.lastWifiRetryRoundMs = nowMs;
    return;
  }
  if (nowMs - state.lastWifiRetryRoundMs < board::kWifiConnectRetryIntervalMs) {
    return;
  }

  resetWifiAttemptRound(state);
}

void pollBootButton(RuntimeState& state, uint32_t nowMs) {
  if (nowMs - state.lastButtonPollMs < board::kButtonPollIntervalMs) {
    return;
  }

  state.lastButtonPollMs = nowMs;
  const bool pressed = digitalRead(board::kBootKeyPin) == LOW;
  if (pressed == state.lastButtonPressed) {
    return;
  }

  state.lastButtonPressed = pressed;
  Serial.println(pressed ? "BOOT pressed" : "BOOT released");
}

void printRuntimeCheck(const RuntimeContext& context, const RuntimeState& state) {
  const esp_partition_t* partition = esp_ota_get_running_partition();
  const RuntimeDiagnostics diagnostics = buildRuntimeDiagnostics(buildRuntimeStatus(context, state));

  Serial.println();
  Serial.println("=== Hitomi Device Runtime ===");
  Serial.printf("FW tag: %s\n", board::kFirmwareTag);
  Serial.printf("Chip model: %s\n", ESP.getChipModel());
  Serial.printf("Chip revision: %d\n", ESP.getChipRevision());
  Serial.printf("SDK version: %s\n", ESP.getSdkVersion());
  Serial.printf("CPU frequency: %lu MHz\n", static_cast<unsigned long>(ESP.getCpuFreqMHz()));
  Serial.printf("Flash size: %lu MB\n", static_cast<unsigned long>(ESP.getFlashChipSize() / (1024 * 1024)));
  Serial.printf("PSRAM detected: %s\n", psramFound() ? "yes" : "no");
  Serial.printf("Free heap: %lu bytes\n", static_cast<unsigned long>(ESP.getFreeHeap()));
  Serial.printf("Free PSRAM: %lu bytes\n", static_cast<unsigned long>(ESP.getFreePsram()));
  Serial.printf("Credentials store ready: %s\n", state.credentialsReady ? "yes" : "no");
  Serial.printf("LittleFS ready: %s\n", state.filesystemReady ? "yes" : "no");
  Serial.printf("Template store enabled: %s\n", board::kEnableTemplateStore ? "yes" : "no");
  Serial.printf("Template store ready: %s\n", state.templateStoreReady ? "yes" : "no");
  Serial.printf("Display ready: %s\n", state.displayReady ? "yes" : "no");
  Serial.printf("WiFi configured: %s\n", state.deviceConfig.wifiConfigured() ? "yes" : "no");
  Serial.printf("API configured: %s\n", state.deviceConfig.backendLocator.configured() ? "yes" : "no");
  Serial.printf("Device credentials configured: %s\n", state.credentials.configured() ? "yes" : "no");
  Serial.println(diagnostics.credentialsLine.c_str());
  Serial.println(diagnostics.snapshotLine.c_str());
  Serial.println(diagnostics.storageLine.c_str());
  Serial.println(diagnostics.queueLine.c_str());
  Serial.println(diagnostics.faceLine.c_str());
  Serial.println(diagnostics.faceDetectLine.c_str());

  if (partition != nullptr) {
    Serial.printf("Running partition: %s (%lu bytes)\n", partition->label, static_cast<unsigned long>(partition->size));
  }

  Serial.println("==============================");
}

}  // namespace app
