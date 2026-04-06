#include "app/runtime_network_ops.hpp"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>
#include <utility>

#include "app/api_probe_policy.hpp"
#include "app/runtime_storage_ops.hpp"
#include "board/app_config.hpp"
#include "core/use_cases.hpp"
#include "infra/device_api_client.hpp"

namespace app {

void resetNetworkTaskSchedule(RuntimeState& state) {
  state.lastApiProbeAttemptMs = 0;
  state.lastActivationAttemptMs = 0;
  state.lastSyncAttemptMs = 0;
  state.lastUploadAttemptMs = 0;
}

void probeConnectivity(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  state.lastNetworkProbeMs = nowMs;
  const ConnectivityState next =
      WiFi.status() == WL_CONNECTED ? ConnectivityState::Connected : ConnectivityState::Disconnected;
  if (next != state.connectivity) {
    state.connectivity = next;
    state.renderDirty = true;

    if (state.connectivity == ConnectivityState::Connected) {
      state.activeWifiSsid = std::string(WiFi.SSID().c_str());
      core::markWifiProfileSuccess(state.deviceConfig.wifiProfiles, state.activeWifiSsid.value(), nowMs);
      persistDeviceConfig(context, state);
      resetNetworkTaskSchedule(state);
    } else {
      state.activeWifiSsid = std::nullopt;
      state.apiProbeInFlight = false;
      state.activationInFlight = false;
    }
  }
}

bool shouldProbeApi(const RuntimeContext& context, const RuntimeState& state, uint32_t nowMs) {
  return app::shouldProbeApi(
      state.deviceConfig.backendLocator.configured() && context.deviceApiClient.configured(),
      state.apiProbeInFlight,
      state.connectivity,
      state.lastApiProbeAttemptMs,
      nowMs);
}

void performApiProbe(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  state.apiProbeInFlight = true;
  state.lastApiProbeAttemptMs = nowMs;
  state.renderDirty = true;

  const auto result = context.deviceApiClient.probeServer();

  state.apiProbeInFlight = false;
  const ApiProbeOutcome outcome = toApiProbeOutcome(result);
  state.apiProbeSucceeded = outcome.succeeded;
  state.apiProbeStatusCode = outcome.statusCode;

  if (result.success && result.data.has_value()) {
    Serial.printf(
        "[APP] API reachable service=%s now=%llu\n",
        result.data->service.c_str(),
        static_cast<unsigned long long>(result.data->now));
  } else if (result.error.has_value()) {
    Serial.printf(
        "[APP] API probe failed code=%s message=%s\n",
        result.error->code.c_str(),
        result.error->message.c_str());
  } else {
    Serial.println("[APP] API probe failed without error details");
  }

  state.renderDirty = true;
}

bool shouldActivate(const RuntimeContext& context, const RuntimeState& state, uint32_t nowMs) {
  if (state.activationInFlight || state.connectivity != ConnectivityState::Connected) {
    return false;
  }
  if (state.credentials.configured()) {
    return false;
  }
  if (!state.deviceConfig.backendLocator.configured() || !state.deviceConfig.bootstrapIdentity.configured()) {
    return false;
  }
  if (!context.deviceApiClient.configured()) {
    return false;
  }
  if (state.lastActivationAttemptMs == 0) {
    return true;
  }
  return nowMs - state.lastActivationAttemptMs >= board::kActivationRetryIntervalMs;
}

void performActivation(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  state.activationInFlight = true;
  state.lastActivationAttemptMs = nowMs;
  state.renderDirty = true;

  infra::ApiResult<infra::BootstrapActivationResponse> activationResult = {};
  std::string apiPath = "/api/device/bootstrap/claim-status";

  if (!state.activationRegistrationId.has_value()) {
    apiPath = "/api/device/bootstrap/hello";
    activationResult = context.deviceApiClient.bootstrapHello({
        .deviceSerial = state.deviceConfig.bootstrapIdentity.deviceSerial,
        .bootstrapSecret = state.deviceConfig.bootstrapIdentity.bootstrapSecret,
        .firmwareTag = board::kFirmwareTag,
        .wifiSsid = state.activeWifiSsid,
    });
  } else {
    activationResult = context.deviceApiClient.pollActivation(
        state.deviceConfig.bootstrapIdentity, state.activationRegistrationId.value());
  }

  state.activationInFlight = false;

  if (!activationResult.success || !activationResult.data.has_value()) {
    setLastError(state, activationResult.error);
    if (activationResult.error.has_value()) {
      appendApiFailureLog(context, state, apiPath.c_str(), nowMs, activationResult.error.value(), std::nullopt);
    }
    state.renderDirty = true;
    return;
  }

  const auto& data = activationResult.data.value();
  if (!data.registrationId.empty()) {
    state.activationRegistrationId = data.registrationId;
  }

  if (data.state == "activated" && data.deviceCode.has_value() && data.apiKey.has_value()) {
    state.deviceConfig.runtimeCredentials.deviceCode = data.deviceCode.value();
    state.deviceConfig.runtimeCredentials.apiKey = data.apiKey.value();
    state.credentials = state.deviceConfig.runtimeCredentials;
    persistDeviceConfig(context, state);
    context.localStore.saveCredentials(state.credentials);
    state.activationRegistrationId = std::nullopt;
    state.faceModuleEnabled =
        state.templateStoreReady && facePortsReady(context) && state.credentials.configured();
  }

  setLastError(state, std::nullopt);
  state.renderDirty = true;
}

bool shouldSync(const RuntimeContext& context, const RuntimeState& state, uint32_t nowMs) {
  if (state.syncInFlight || state.connectivity != ConnectivityState::Connected) {
    return false;
  }
  if (!state.credentials.configured() || !context.deviceApiClient.configured()) {
    return false;
  }
  if (state.lastSyncAttemptMs == 0) {
    return true;
  }
  return nowMs - state.lastSyncAttemptMs >= board::kSyncIntervalMs;
}

bool shouldUpload(const RuntimeContext& context, const RuntimeState& state, uint32_t nowMs) {
  if (state.uploadInFlight || state.connectivity != ConnectivityState::Connected) {
    return false;
  }
  if (state.pendingAttendanceRecords.empty()) {
    return false;
  }
  if (!state.credentials.configured() || !context.deviceApiClient.configured()) {
    return false;
  }
  if (!state.snapshots.attendanceConfig.has_value()) {
    return false;
  }
  if (state.lastUploadAttemptMs == 0) {
    return true;
  }
  return nowMs - state.lastUploadAttemptMs >= board::kUploadRetryIntervalMs;
}

void performSync(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  state.syncInFlight = true;
  state.lastSyncAttemptMs = nowMs;
  state.renderDirty = true;

  auto result = context.deviceApiClient.sync(state.credentials);

  state.syncInFlight = false;
  if (!result.success || !result.data.has_value()) {
    setLastError(state, result.error);
    if (result.error.has_value()) {
      appendApiFailureLog(context, state, "/api/device/sync", nowMs, result.error.value(), std::nullopt);
    }
    state.renderDirty = true;
    return;
  }

  state.snapshots = core::applySyncSnapshot(state.snapshots, result.data.value(), nowMs);
  state.apiProbeSucceeded = true;
  state.apiProbeStatusCode = std::nullopt;
  persistSnapshots(context, state);
  setLastError(state, std::nullopt);
  state.renderDirty = true;
}

void performUpload(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  state.uploadInFlight = true;
  state.lastUploadAttemptMs = nowMs;
  state.renderDirty = true;

  const std::size_t batchSize =
      std::min<std::size_t>(state.pendingAttendanceRecords.size(), board::kUploadBatchSize);
  std::vector<core::PendingAttendanceRecord> batch(
      state.pendingAttendanceRecords.begin(), state.pendingAttendanceRecords.begin() + batchSize);

  auto result = context.deviceApiClient.uploadAttendance(state.credentials, batch);

  state.uploadInFlight = false;
  if (!result.success || !result.data.has_value()) {
    setLastError(state, result.error);
    if (result.error.has_value()) {
      markUploadAttemptFailure(context, state, batch, nowMs, result.error->code);
      appendApiFailureLog(
          context, state, "/api/device/attendance/upload", nowMs, result.error.value(), std::nullopt);
    }
    state.renderDirty = true;
    return;
  }

  auto uploadState =
      core::applyUploadResults(state.pendingAttendanceRecords, state.failureLogs, result.data->results, nowMs);
  state.pendingAttendanceRecords = std::move(uploadState.queue);
  state.failureLogs = std::move(uploadState.logs);
  core::pruneFailureLogs(state.failureLogs, board::kFailureLogLimit);
  state.apiProbeSucceeded = true;
  state.apiProbeStatusCode = std::nullopt;
  persistPendingAttendanceRecords(context, state);
  persistFailureLogs(context, state);

  std::optional<core::ApiError> latestRejectedError;
  for (const auto& item : result.data->results) {
    if (item.status == core::AttendanceUploadStatus::Rejected && item.error.has_value()) {
      latestRejectedError = item.error;
    }
  }
  setLastError(state, latestRejectedError);
  state.renderDirty = true;
}

}  // namespace app
