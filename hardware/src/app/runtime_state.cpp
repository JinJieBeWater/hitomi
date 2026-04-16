#include "app/runtime_state.hpp"

#include "board/app_config.hpp"
#include "face/ports.hpp"

namespace app {

bool facePortsReady(const RuntimeContext& context) {
  return context.camera.available() &&
      (context.enrollmentService.available() || context.recognitionService.available());
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
  status.faceEngineReady = state.faceEngineReady;
  status.faceDetectReady = state.faceDetectReady;
  status.faceDetected = state.faceDetected;
  status.enrollmentReportInFlight = state.enrollmentReportInFlight;
  status.templateStoreStatusCode = state.storageAux.templateStoreHealth.statusCode;
  status.templateStoreDetail = state.storageAux.templateStoreHealth.detail;
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
  status.enrollmentState = state.enrollmentState;
  status.enrollmentPendingCount = state.pendingEnrollmentReports.size();
  status.enrollmentCapturedSamples = state.enrollmentCapturedSamples;
  status.enrollmentRequiredSamples = state.enrollmentRequiredSamples;
  status.activeEnrollmentTaskId = state.activeEnrollmentTaskId;
  status.activeEnrollmentEmployeeName = state.activeEnrollmentEmployeeName;
  status.enrollmentFailureReason = state.enrollmentFailureReason;
  status.enrollmentStatusDetail = state.enrollmentStatusDetail;
  status.faceEngineStatusDetail = state.faceEngineStatusDetail;
  status.faceDetectStatusDetail = state.faceDetectStatusDetail;
  status.faceTopScore = state.faceTopScore;
  status.detectedFaceCount = state.detectedFaceCount;
  status.faceModuleEnabled = state.faceModuleEnabled;
  status.lastAttendanceFeedback = state.lastAttendanceFeedback;
  const face::CameraStatus cameraStatus = context.camera.status();
  status.cameraAvailable = cameraStatus.supported;
  status.cameraReady = cameraStatus.initialized;
  status.cameraCaptureCount = cameraStatus.captureCount;
  status.cameraFailedCaptureCount = cameraStatus.failedCaptureCount;
  status.cameraLastFrame = cameraStatus.lastFrame;
  status.cameraSensorModel = cameraStatus.sensorModel;
  status.cameraLastError = cameraStatus.lastError;
  status.lastErrorCode = state.lastErrorCode;
  return status;
}

void setLastError(RuntimeState& state, const std::optional<core::ApiError>& error) {
  state.lastErrorCode = error.has_value() ? std::optional<std::string>(error->code) : std::nullopt;
}

}  // namespace app
