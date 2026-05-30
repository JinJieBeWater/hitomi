#include "app/runtime_network_ops.hpp"

#include <Arduino.h>
#include <WiFi.h>

#include <algorithm>
#include <utility>

#include "app/api_probe_policy.hpp"
#include "app/runtime_enrollment_ops.hpp"
#include "app/runtime_network_planner.hpp"
#include "app/runtime_storage_ops.hpp"
#include "board/app_config.hpp"
#include "core/use_cases.hpp"
#include "infra/display_port.hpp"

namespace app {
namespace {

void notify(
    const RuntimeContext& context,
    infra::DisplayNotificationLevel level,
    const std::string& text,
    uint32_t durationMs) {
  context.display.showNotification(infra::DisplayNotification{
      .level = level,
      .text = text,
      .durationMs = durationMs,
  });
}

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
    case NetworkRequestType::EnrollmentReport:
      if (!state.pendingEnrollmentReports.empty()) {
        request.enrollmentReportRequest = {
            .taskId = state.pendingEnrollmentReports.front().taskId,
            .employeeId = state.pendingEnrollmentReports.front().employeeId,
            .result = state.pendingEnrollmentReports.front().result,
            .finishedAt = state.pendingEnrollmentReports.front().finishedAt,
            .failureReason = state.pendingEnrollmentReports.front().failureReason,
        };
      }
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
    case NetworkRequestType::EnrollmentReport:
      state.enrollmentReportInFlight = true;
      state.lastEnrollmentReportAttemptMs = nowMs;
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

void markRequestDeferred(RuntimeState& state, NetworkRequestType type, uint32_t nowMs) {
  switch (type) {
    case NetworkRequestType::ApiProbe:
      state.lastApiProbeAttemptMs = nowMs;
      break;
    case NetworkRequestType::Activation:
      state.lastActivationAttemptMs = nowMs;
      break;
    case NetworkRequestType::EnrollmentReport:
      state.lastEnrollmentReportAttemptMs = nowMs;
      break;
    case NetworkRequestType::Sync:
      state.lastSyncAttemptMs = nowMs;
      break;
    case NetworkRequestType::Upload:
      state.lastUploadAttemptMs = nowMs;
      break;
    case NetworkRequestType::None:
    default:
      break;
  }
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

void applySyncResult(
    const RuntimeContext& context,
    RuntimeState& state,
    const CompletedNetworkRequest& completed,
    uint32_t nowMs) {
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
  updateWallClockFromSync(state, result.data->serverTime, nowMs);
  state.apiProbeSucceeded = true;
  state.apiProbeStatusCode = std::nullopt;
  persistSnapshots(context, state);
  reconcileTemplatesForEmployees(context, state);
  setLastError(state, std::nullopt);
  state.renderDirty = true;
}

void applyEnrollmentReportResult(
    const RuntimeContext& context,
    RuntimeState& state,
    const CompletedNetworkRequest& completed) {
  static constexpr char kApiPath[] = "/api/device/enrollment/report";

  state.enrollmentReportInFlight = false;
  if (state.pendingEnrollmentReports.empty()) {
    state.renderDirty = true;
    return;
  }

  auto& report = state.pendingEnrollmentReports.front();
  const auto& result = completed.enrollmentReportResult;
  if (!result.success || !result.data.has_value()) {
    setLastError(state, result.error);
    if (result.error.has_value()) {
      Serial.printf(
          "[APP] enrollment report failed task=%s code=%s message=%s\n",
          report.taskId.c_str(),
          result.error->code.c_str(),
          result.error->message.c_str());
      report.lastAttemptAt = completed.requestedAtMs;
      report.lastResultCode = result.error->code;
      persistPendingEnrollmentReports(context, state);
      appendApiFailureLog(context, state, kApiPath, completed.requestedAtMs, result.error.value(), report.taskId);
    }
    state.renderDirty = true;
    return;
  }

  state.apiProbeSucceeded = true;
  state.apiProbeStatusCode = std::nullopt;

  const bool terminalStatus =
      result.data->status == "success" || result.data->status == "failed" || result.data->status == "cancelled";
  if (result.data->applied || terminalStatus) {
    const std::string employeeName =
        state.activeEnrollmentEmployeeName.value_or(report.employeeId.empty() ? "所选员工" : report.employeeId);
    const bool cancelledByServer = result.data->status == "cancelled";
    const bool cancelledLocally =
        report.failureReason.has_value() && report.failureReason.value() == "ENROLLMENT_CANCELLED";
    const bool failedByServer = result.data->status == "failed";
    Serial.printf(
        "[APP] enrollment report applied task=%s status=%s applied=%d\n",
        report.taskId.c_str(),
        result.data->status.c_str(),
        result.data->applied ? 1 : 0);
    core::removeEnrollmentTask(state.snapshots.enrollmentTasks, report.taskId);
    persistSnapshots(context, state);
    state.pendingEnrollmentReports.erase(state.pendingEnrollmentReports.begin());
    persistPendingEnrollmentReports(context, state);
    if (state.enrollmentState == EnrollmentRunState::Reporting) {
      state.enrollmentState =
          (cancelledByServer || cancelledLocally) ? EnrollmentRunState::Cancelled
          : failedByServer ? EnrollmentRunState::Failed
                           : EnrollmentRunState::Done;
      state.enrollmentStatusDetail =
          (cancelledByServer || cancelledLocally) ? std::optional<std::string>("已取消")
          : failedByServer ? std::optional<std::string>("失败")
                           : std::optional<std::string>("已完成");
      state.enrollmentFailureReason =
          (cancelledByServer || cancelledLocally) ? std::optional<std::string>("ENROLLMENT_CANCELLED")
          : failedByServer ? std::optional<std::string>("ENROLLMENT_FAILED")
                           : std::nullopt;
    }
    setLastError(
        state,
        (result.data->status == "cancelled" || cancelledLocally) ? std::optional<core::ApiError>(core::ApiError{
                                                 .code = "TASK_CANCELLED",
                                                 .message = "enrollment task was cancelled",
                                                 .retryable = false,
                                             })
                                           : std::nullopt);
    notify(
        context,
        (cancelledByServer || cancelledLocally) ? infra::DisplayNotificationLevel::Warning
        : failedByServer ? infra::DisplayNotificationLevel::Error
                         : infra::DisplayNotificationLevel::Success,
        (cancelledByServer || cancelledLocally) ? ("已取消：" + employeeName)
        : failedByServer ? ("失败：" + employeeName)
                         : ("已完成：" + employeeName),
        (cancelledByServer || cancelledLocally) ? 2800 : 2200);
    state.renderDirty = true;
    return;
  }

  report.lastAttemptAt = completed.requestedAtMs;
  report.lastResultCode = result.data->status;
  persistPendingEnrollmentReports(context, state);
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
  core::markLocalAttendanceUploads(state.localAttendanceMarks, completed.uploadBatch, result.data->results);
  state.pendingAttendanceRecords = std::move(uploadState.queue);
  state.failureLogs = std::move(uploadState.logs);
  core::pruneFailureLogs(state.failureLogs, board::kFailureLogLimit);
  state.apiProbeSucceeded = true;
  state.apiProbeStatusCode = std::nullopt;
  persistPendingAttendanceRecords(context, state);
  persistLocalAttendanceMarks(context, state);
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
  state.lastEnrollmentReportAttemptMs = 0;
  state.lastSyncAttemptMs = 0;
  state.lastUploadAttemptMs = 0;
}

void invalidatePendingNetworkRequests(RuntimeState& state) {
  state.networkRequestGeneration += 1;
  state.apiProbeInFlight = false;
  state.activationInFlight = false;
  state.enrollmentReportInFlight = false;
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
  Serial.printf(
      "[APP] display command type=%u target=%s state=%u serviceActive=%d\n",
      static_cast<unsigned>(command.type),
      command.targetId.c_str(),
      static_cast<unsigned>(state.enrollmentState),
      context.enrollmentService.active() ? 1 : 0);
  switch (command.type) {
    case infra::DisplayCommandType::RefreshData:
      requestManualRefresh(context, state, nowMs);
      return;
    case infra::DisplayCommandType::StartEnrollmentTask:
      startEnrollmentTask(context, state, command.targetId, nowMs);
      return;
    case infra::DisplayCommandType::CancelEnrollment:
      cancelEnrollmentTask(context, state, nowMs);
      return;
    case infra::DisplayCommandType::DismissCapture:
      dismissCaptureSummary(state);
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
    case NetworkRequestType::EnrollmentReport:
      applyEnrollmentReportResult(context, state, *completed);
      return;
    case NetworkRequestType::Sync:
      applySyncResult(context, state, *completed, nowMs);
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
  const std::size_t uploadBatchSize = request.uploadBatch.size();
  const std::string enrollmentTaskId = request.enrollmentReportRequest.taskId;
  const std::string enrollmentEmployeeId = request.enrollmentReportRequest.employeeId;

  if (!context.networkExecutor.submit(std::move(request))) {
    markRequestDeferred(state, type, nowMs);
    return;
  }

  if (type == NetworkRequestType::Activation) {
    Serial.printf(
        "[APP] Activation attempt path=%s serial=%s\n",
        "/api/device/bootstrap/hello",
        state.deviceConfig.bootstrapIdentity.deviceSerial.c_str());
  } else if (type == NetworkRequestType::EnrollmentReport) {
    Serial.printf(
        "[APP] Enrollment report taskId=%s employeeId=%s\n",
        enrollmentTaskId.c_str(),
        enrollmentEmployeeId.c_str());
  } else if (type == NetworkRequestType::Upload) {
    Serial.printf("[APP] Upload request submitted batch=%u\n", static_cast<unsigned>(uploadBatchSize));
  }
  markRequestSubmitted(state, type, nowMs);
}

}  // namespace app
