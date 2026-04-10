#include "app/runtime_network_ops.hpp"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>
#include <utility>

#include "app/api_probe_policy.hpp"
#include "app/runtime_network_planner.hpp"
#include "app/runtime_storage_ops.hpp"
#include "board/app_config.hpp"
#include "core/use_cases.hpp"

namespace app {
namespace {

void clearManualRefreshRequests(RuntimeState& state) {
  state.manualApiProbeRequested = false;
  state.manualActivationRequested = false;
  state.manualSyncRequested = false;
}

void reconcileManualRefreshRequests(RuntimeState& state) {
  if (state.manualApiProbeRequested && !state.deviceConfig.backendLocator.configured()) {
    state.manualApiProbeRequested = false;
  }
  if (
      state.manualActivationRequested &&
      (state.credentials.configured() || !state.deviceConfig.backendLocator.configured() ||
       !state.deviceConfig.bootstrapIdentity.configured())) {
    state.manualActivationRequested = false;
  }
  if (
      state.manualSyncRequested &&
      (!state.deviceConfig.backendLocator.configured() ||
       (!state.credentials.configured() && !state.manualActivationRequested))) {
    state.manualSyncRequested = false;
  }
}

void requestManualRefresh(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  Serial.println("[APP] Manual refresh requested");
  probeConnectivity(context, state, nowMs);

  if (state.connectivity != ConnectivityState::Connected) {
    clearManualRefreshRequests(state);
    state.renderDirty = true;
    return;
  }

  const bool apiConfigured = state.deviceConfig.backendLocator.configured();
  state.manualApiProbeRequested = apiConfigured;
  state.manualActivationRequested =
      apiConfigured && !state.credentials.configured() && state.deviceConfig.bootstrapIdentity.configured();
  state.manualSyncRequested = apiConfigured && (state.credentials.configured() || state.manualActivationRequested);
  state.renderDirty = true;
}

PendingNetworkRequest buildNetworkRequest(const RuntimeState& state, NetworkRequestType type, uint32_t nowMs) {
  PendingNetworkRequest request = {};
  request.type = type;
  request.generation = state.networkRequestGeneration;
  request.requestedAtMs = nowMs;
  request.baseUrl = state.deviceConfig.backendLocator.origin;
  request.credentials = state.credentials;

  switch (type) {
    case NetworkRequestType::Activation:
      request.activationRequest = {
          .deviceSerial = state.deviceConfig.bootstrapIdentity.deviceSerial,
          .bootstrapSecret = state.deviceConfig.bootstrapIdentity.bootstrapSecret,
          .firmwareTag = board::kFirmwareTag,
          .wifiSsid = state.activeWifiSsid,
      };
      return request;
    case NetworkRequestType::Upload: {
      const std::size_t batchSize =
          std::min<std::size_t>(state.pendingAttendanceRecords.size(), board::kUploadBatchSize);
      request.uploadBatch.assign(
          state.pendingAttendanceRecords.begin(),
          state.pendingAttendanceRecords.begin() + batchSize);
      return request;
    }
    case NetworkRequestType::ApiProbe:
    case NetworkRequestType::Sync:
    case NetworkRequestType::None:
    default:
      return request;
  }
}

void markRequestSubmitted(RuntimeState& state, NetworkRequestType type, uint32_t nowMs) {
  switch (type) {
    case NetworkRequestType::ApiProbe:
      state.apiProbeInFlight = true;
      state.lastApiProbeAttemptMs = nowMs;
      break;
    case NetworkRequestType::Activation:
      state.activationInFlight = true;
      state.lastActivationAttemptMs = nowMs;
      break;
    case NetworkRequestType::Sync:
      state.syncInFlight = true;
      state.lastSyncAttemptMs = nowMs;
      break;
    case NetworkRequestType::Upload:
      state.uploadInFlight = true;
      state.lastUploadAttemptMs = nowMs;
      break;
    case NetworkRequestType::None:
    default:
      break;
  }
  state.renderDirty = true;
}

void applyApiProbeResult(RuntimeState& state, const infra::ApiResult<infra::ServerProbeResponse>& result) {
  state.apiProbeInFlight = false;
  state.manualApiProbeRequested = false;

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

void applyActivationResult(
    const RuntimeContext& context,
    RuntimeState& state,
    const CompletedNetworkRequest& completed) {
  static constexpr char kApiPath[] = "/api/device/bootstrap/hello";

  state.activationInFlight = false;
  state.manualActivationRequested = false;

  const auto& result = completed.activationResult;
  if (!result.success || !result.data.has_value()) {
    setLastError(state, result.error);
    if (result.error.has_value()) {
      Serial.printf(
          "[APP] Activation failed path=%s code=%s message=%s\n",
          kApiPath,
          result.error->code.c_str(),
          result.error->message.c_str());
      appendApiFailureLog(context, state, kApiPath, completed.requestedAtMs, result.error.value(), std::nullopt);
    }
    if (!state.credentials.configured()) {
      state.manualSyncRequested = false;
    }
    state.renderDirty = true;
    return;
  }

  const auto& data = result.data.value();
  if (data.state == "activated" && data.deviceCode.has_value() && data.apiKey.has_value()) {
    Serial.printf("[APP] Activated deviceCode=%s\n", data.deviceCode.value().c_str());
    state.deviceConfig.runtimeCredentials.deviceCode = data.deviceCode.value();
    state.deviceConfig.runtimeCredentials.apiKey = data.apiKey.value();
    state.credentials = state.deviceConfig.runtimeCredentials;
    persistDeviceConfig(context, state);
    context.localStore.saveCredentials(state.credentials);
    state.faceModuleEnabled = state.templateStoreReady && facePortsReady(context) && state.credentials.configured();
  } else {
    Serial.printf("[APP] Activation response state=%s\n", data.state.c_str());
    if (!state.credentials.configured()) {
      state.manualSyncRequested = false;
    }
  }

  setLastError(state, std::nullopt);
  state.renderDirty = true;
}

void applySyncResult(const RuntimeContext& context, RuntimeState& state, const CompletedNetworkRequest& completed) {
  state.syncInFlight = false;
  state.manualSyncRequested = false;

  const auto& result = completed.syncResult;
  if (!result.success || !result.data.has_value()) {
    setLastError(state, result.error);
    if (result.error.has_value()) {
      appendApiFailureLog(context, state, "/api/device/sync", completed.requestedAtMs, result.error.value(), std::nullopt);
    }
    state.renderDirty = true;
    return;
  }

  state.snapshots = core::applySyncSnapshot(state.snapshots, result.data.value(), completed.requestedAtMs);
  state.apiProbeSucceeded = true;
  state.apiProbeStatusCode = std::nullopt;
  persistSnapshots(context, state);
  setLastError(state, std::nullopt);
  state.renderDirty = true;
}

void applyUploadResult(const RuntimeContext& context, RuntimeState& state, const CompletedNetworkRequest& completed) {
  state.uploadInFlight = false;

  const auto& result = completed.uploadResult;
  if (!result.success || !result.data.has_value()) {
    setLastError(state, result.error);
    if (result.error.has_value()) {
      markUploadAttemptFailure(context, state, completed.uploadBatch, completed.requestedAtMs, result.error->code);
      appendApiFailureLog(
          context,
          state,
          "/api/device/attendance/upload",
          completed.requestedAtMs,
          result.error.value(),
          std::nullopt);
    }
    state.renderDirty = true;
    return;
  }

  auto uploadState =
      core::applyUploadResults(state.pendingAttendanceRecords, state.failureLogs, result.data->results, completed.requestedAtMs);
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

}  // namespace

void resetNetworkTaskSchedule(RuntimeState& state) {
  state.lastApiProbeAttemptMs = 0;
  state.lastActivationAttemptMs = 0;
  state.lastSyncAttemptMs = 0;
  state.lastUploadAttemptMs = 0;
}

void invalidatePendingNetworkRequests(RuntimeState& state) {
  state.networkRequestGeneration += 1;
  state.apiProbeInFlight = false;
  state.activationInFlight = false;
  state.syncInFlight = false;
  state.uploadInFlight = false;
  clearManualRefreshRequests(state);
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
      invalidatePendingNetworkRequests(state);
      state.activeWifiSsid = std::nullopt;
    }
  }
}

void handleDisplayCommand(
    const RuntimeContext& context,
    RuntimeState& state,
    const infra::DisplayCommand& command,
    uint32_t nowMs) {
  switch (command.type) {
    case infra::DisplayCommandType::RefreshData:
      requestManualRefresh(context, state, nowMs);
      return;
    case infra::DisplayCommandType::StartEnrollmentTask:
      Serial.printf("[APP] Enrollment start requested taskId=%s\n", command.targetId.c_str());
      state.renderDirty = true;
      return;
    default:
      return;
  }
}

void consumeCompletedNetworkRequest(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  static_cast<void>(nowMs);

  const auto completed = context.networkExecutor.consumeCompleted();
  if (!completed.has_value()) {
    return;
  }

  if (completed->generation != state.networkRequestGeneration) {
    Serial.printf(
        "[APP] Dropping stale network result generation=%lu current=%lu\n",
        static_cast<unsigned long>(completed->generation),
        static_cast<unsigned long>(state.networkRequestGeneration));
    return;
  }

  switch (completed->type) {
    case NetworkRequestType::ApiProbe:
      applyApiProbeResult(state, completed->probeResult);
      return;
    case NetworkRequestType::Activation:
      applyActivationResult(context, state, *completed);
      return;
    case NetworkRequestType::Sync:
      applySyncResult(context, state, *completed);
      return;
    case NetworkRequestType::Upload:
      applyUploadResult(context, state, *completed);
      return;
    case NetworkRequestType::None:
    default:
      return;
  }
}

void dispatchNetworkRequest(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  reconcileManualRefreshRequests(state);

  const NetworkRequestType type = nextNetworkRequestType(state, nowMs);
  if (type == NetworkRequestType::None) {
    return;
  }

  PendingNetworkRequest request = buildNetworkRequest(state, type, nowMs);
  if (type == NetworkRequestType::Upload && request.uploadBatch.empty()) {
    return;
  }

  if (!context.networkExecutor.submit(std::move(request))) {
    return;
  }

  if (type == NetworkRequestType::Activation) {
    Serial.printf(
        "[APP] Activation attempt path=%s serial=%s\n",
        "/api/device/bootstrap/hello",
        state.deviceConfig.bootstrapIdentity.deviceSerial.c_str());
  }
  markRequestSubmitted(state, type, nowMs);
}

}  // namespace app
