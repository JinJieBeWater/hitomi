#include "app/runtime_lifecycle_service.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_ota_ops.h>

#include <algorithm>
#include <cstdio>

#include "app/runtime_diagnostics.hpp"
#include "app/runtime_state.hpp"
#include "board/app_config.hpp"
#include "board/pins.hpp"
#include "core/use_cases.hpp"
#include "infra/device_api_client.hpp"
#include "infra/local_store.hpp"

namespace app {

namespace {

std::string buildDefaultDeviceSerial() {
  char serial[32] = {};
  std::snprintf(serial, sizeof(serial), "SZPI-%llX", static_cast<unsigned long long>(ESP.getEfuseMac()));
  return serial;
}

}  // namespace

void waitForSerial() {
  const unsigned long startedAt = millis();
  while (!Serial && millis() - startedAt < board::kSerialWaitTimeoutMs) {
    delay(10);
  }
}

void initWifi() {
  WiFi.mode(WIFI_STA);
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

void syncDeviceApiClientConfig(const RuntimeContext& context, const RuntimeState& state) {
  context.deviceApiClient.setBaseUrl(state.deviceConfig.backendLocator.origin);
}

void ensureWifiConnection(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  static_cast<void>(context);

  if (WiFi.status() == WL_CONNECTED) {
    if (state.wifiScanInProgress) {
      WiFi.scanDelete();
      state.wifiScanInProgress = false;
    }
    return;
  }
  if (!state.deviceConfig.wifiConfigured()) {
    return;
  }

  if (state.wifiScanInProgress) {
    const int scanResult = WiFi.scanComplete();
    if (scanResult == WIFI_SCAN_RUNNING) {
      return;
    }
    state.wifiScanInProgress = false;

    std::vector<std::string> availableSsids;
    if (scanResult > 0) {
      availableSsids.reserve(static_cast<std::size_t>(scanResult));
      for (int index = 0; index < scanResult; ++index) {
        availableSsids.emplace_back(WiFi.SSID(index).c_str());
      }
    }
    WiFi.scanDelete();

    auto selected = core::chooseWifiProfile(state.deviceConfig.wifiProfiles, availableSsids);
    if (!selected.has_value()) {
      // Fallback: try first configured profile even if not seen in scan (supports hidden SSIDs)
      auto fallback = std::find_if(
          state.deviceConfig.wifiProfiles.begin(),
          state.deviceConfig.wifiProfiles.end(),
          [](const core::WifiProfile& profile) { return profile.configured(); });
      if (fallback == state.deviceConfig.wifiProfiles.end()) {
        Serial.println("[APP] no known WiFi profile found in scan");
        return;
      }
      selected = static_cast<std::size_t>(std::distance(state.deviceConfig.wifiProfiles.begin(), fallback));
    }

    const auto& profile = state.deviceConfig.wifiProfiles[selected.value()];
    state.activeWifiSsid = profile.ssid;
    Serial.printf("[APP] connecting WiFi SSID=%s\n", profile.ssid.c_str());
    WiFi.begin(profile.ssid.c_str(), profile.password.c_str());
    return;
  }

  if (nowMs - state.lastWifiConnectAttemptMs < board::kWifiConnectRetryIntervalMs) {
    return;
  }
  state.lastWifiConnectAttemptMs = nowMs;
  WiFi.scanNetworks(true);  // async, non-blocking
  state.wifiScanInProgress = true;
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
  Serial.printf("CPU frequency: %lu MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash size: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("PSRAM detected: %s\n", psramFound() ? "yes" : "no");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
  Serial.printf("Credentials store ready: %s\n", state.credentialsReady ? "yes" : "no");
  Serial.printf("LittleFS ready: %s\n", state.filesystemReady ? "yes" : "no");
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

  if (partition != nullptr) {
    Serial.printf("Running partition: %s (%u bytes)\n", partition->label, partition->size);
  }

  Serial.println("==============================");
}

}  // namespace app
