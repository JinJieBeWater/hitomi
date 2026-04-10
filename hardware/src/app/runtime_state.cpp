#include "app/runtime_state.hpp"

#include "board/app_config.hpp"
#include "face/ports.hpp"

namespace app {

bool facePortsReady(const RuntimeContext& context) {
  return context.camera.available() && context.enrollmentService.available() &&
      context.recognitionService.available();
}

RuntimeStatus buildRuntimeStatus(const RuntimeContext& context, const RuntimeState& state) {
  // Keep status projection separate from orchestration so app-only edits stay local.
  RuntimeStatus status = {};
  status.firmwareTag = board::kFirmwareTag;
  status.credentials = state.credentials;
  status.snapshots = state.snapshots;
  status.pendingQueueSize = state.pendingAttendanceRecords.size();
  status.failureLogCount = state.failureLogs.size();
  status.connectivity = state.connectivity;
  status.credentialsReady = state.credentialsReady;
  status.filesystemReady = state.filesystemReady;
  status.templateStoreReady = state.templateStoreReady;
  status.templateStoreStatusCode = state.storageAux.templateStoreHealth.statusCode;
  status.templateCount = state.storageAux.templateLibrarySummary.templateCount;
  status.sdTotalBytes = state.storageAux.templateStoreHealth.totalBytes;
  status.sdUsedBytes = state.storageAux.templateStoreHealth.usedBytes;
  status.wifiConfigured = state.deviceConfig.wifiConfigured();
  status.activeWifiSsid = state.activeWifiSsid;
  status.activationState = state.deviceConfig.activationState();
  status.bootstrapConfigured = state.deviceConfig.bootstrapIdentity.configured();
  status.apiConfigured = state.deviceConfig.backendLocator.configured();
  status.apiProbeInFlight = state.apiProbeInFlight;
  status.apiProbeSucceeded = state.apiProbeSucceeded;
  status.apiProbeStatusCode = state.apiProbeStatusCode;
  status.activationInFlight = state.activationInFlight;
  status.syncInFlight = state.syncInFlight;
  status.uploadInFlight = state.uploadInFlight;
  status.faceModuleEnabled = state.faceModuleEnabled;
  status.lastErrorCode = state.lastErrorCode;
  return status;
}

void setLastError(RuntimeState& state, const std::optional<core::ApiError>& error) {
  state.lastErrorCode = error.has_value() ? std::optional<std::string>(error->code) : std::nullopt;
}

}  // namespace app
