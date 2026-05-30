#include "app/runtime_provisioning_ops.hpp"

#include <Arduino.h>
#include <WiFi.h>

#include "app/runtime_lifecycle_service.hpp"
#include "app/runtime_face_engine_ops.hpp"
#include "app/runtime_network_ops.hpp"
#include "app/runtime_storage_ops.hpp"
#include "app/usb_provisioning_protocol.hpp"
#include "board/app_config.hpp"
#include "core/use_cases.hpp"
#include "infra/template_store_port.hpp"

namespace app {
namespace {

void resetConnectivity(RuntimeState& state) {
  WiFi.disconnect(false, false);
  WiFi.scanDelete();
  state.wifiConnectInProgress = false;
  state.wifiScanInProgress = false;
  state.connectivity = ConnectivityState::Disconnected;
  state.activeWifiSsid.reset();
  state.lastWifiConnectAttemptMs = 0;
  state.lastWifiRetryRoundMs = 0;
  state.lastWifiScanRequestMs = 0;
  state.lastWifiScanCompletedMs = 0;
  state.lastWifiEventSequence = currentWifiDriverEventSequence();
  state.wifiCandidateCursor = 0;
  state.wifiShouldTryLastKnownGood = true;
  state.wifiConfiguredFallbackAttempted = false;
  state.lastWifiDisconnectReason.reset();
  state.activeWifiProfileIndex.reset();
  state.wifiCandidates.clear();
  state.wifiProfileRuntime.clear();
  state.lastActivationAttemptMs = 0;
  state.apiProbeSucceeded = false;
  state.apiProbeStatusCode = std::nullopt;
  invalidatePendingNetworkRequests(state);
  resetNetworkTaskSchedule(state);
}

void resetRuntimeData(RuntimeState& state) {
  state.snapshots = {};
  state.wallClock = {};
  state.pendingEnrollmentReports.clear();
  state.pendingAttendanceRecords.clear();
  state.localAttendanceMarks.clear();
  state.failureLogs.clear();
  state.storageAux = {};
  state.apiProbeInFlight = false;
  state.apiProbeSucceeded = false;
  state.enrollmentReportInFlight = false;
  state.syncInFlight = false;
  state.uploadInFlight = false;
  state.activationInFlight = false;
  state.manualApiProbeRequested = false;
  state.manualActivationRequested = false;
  state.manualSyncRequested = false;
  state.activeEnrollmentTaskId.reset();
  state.activeEnrollmentEmployeeId.reset();
  state.activeEnrollmentEmployeeName.reset();
  state.enrollmentFailureReason.reset();
  state.enrollmentStatusDetail.reset();
  state.faceTopScore.reset();
  state.lastAttendanceFeedback.reset();
  state.lastErrorCode.reset();
  state.lastRecognitionEventKey.reset();
  state.faceDetected = false;
  state.detectedFaceCount = 0;
  state.faceBoxes = {};
  state.faceBoxTones = {};
  state.faceBoxCount = 0;
  state.primaryFaceBoxIndex.reset();
  state.faceBoxFeedbackUntilMs = 0;
  state.enrollmentCapturedSamples = 0;
  state.enrollmentRequiredSamples = 0;
  state.enrollmentState = EnrollmentRunState::Idle;
}

bool clearTemplateStore(const RuntimeContext& context, RuntimeState& state) {
  if (!board::kEnableTemplateStore) {
    state.templateStoreReady = false;
    state.faceModuleEnabled = false;
    state.storageAux.templateStoreHealth.statusCode = infra::kTemplateStoreDisabled;
    state.storageAux.templateStoreHealth.checkedAt = static_cast<uint64_t>(millis());
    state.storageAux.templateStoreHealth.detail = "disabled by board config";
    return true;
  }

  if (!state.templateStoreReady) {
    const infra::TemplateStoreInitStatus initStatus = context.templateStore.begin();
    applyTemplateStoreStatus(context, state, initStatus, initStatus.ready);
  }

  if (!state.templateStoreReady) {
    Serial.println("[APP] template reset failed: template store unavailable");
    return false;
  }

  if (!context.templateStore.clearTemplates()) {
    Serial.println("[APP] template reset failed");
    applyTemplateStoreStatus(context, state, context.templateStore.status(), false);
    return false;
  }

  applyTemplateStoreStatus(context, state, context.templateStore.status(), true);
  return true;
}

bool applyUsbProvisioningCommand(
    const RuntimeContext& context, RuntimeState& state, const UsbProvisioningCommand& command, uint32_t nowMs) {
  static_cast<void>(nowMs);
  switch (command.type) {
    case UsbProvisioningCommandType::GetConfig:
      return true;
    case UsbProvisioningCommandType::SetWifiProfiles:
      state.deviceConfig.wifiProfiles.clear();
      for (const auto& profile : command.wifiProfiles) {
        core::upsertWifiProfile(state.deviceConfig.wifiProfiles, profile);
      }
      persistDeviceConfig(context, state);
      resetConnectivity(state);
      return true;
    case UsbProvisioningCommandType::SetBackendOrigin:
      state.deviceConfig.backendLocator.origin = command.backendOrigin;
      persistDeviceConfig(context, state);
      resetConnectivity(state);
      return true;
    case UsbProvisioningCommandType::SetBootstrapIdentity:
      state.deviceConfig.bootstrapIdentity = command.bootstrapIdentity;
      persistDeviceConfig(context, state);
      return true;
    case UsbProvisioningCommandType::ClearRuntimeCredentials:
      state.deviceConfig.runtimeCredentials = {};
      state.credentials = {};
      persistDeviceConfig(context, state);
      context.localStore.saveCredentials(state.credentials);
      resetConnectivity(state);
      return true;
    case UsbProvisioningCommandType::ClearWifiProfiles:
      state.deviceConfig.wifiProfiles.clear();
      persistDeviceConfig(context, state);
      resetConnectivity(state);
      return true;
    case UsbProvisioningCommandType::ResetDeviceConfig: {
      bool resetOk = true;
      state.deviceConfig = {};
      state.credentials = {};
      state.faceModuleEnabled = false;
      resetRuntimeData(state);
      if (!context.localStore.clearDeviceConfig()) {
        Serial.println("[APP] device config reset failed");
        resetOk = false;
      }
      if (!context.localStore.clearRuntimeData()) {
        Serial.println("[APP] LittleFS runtime reset failed");
        resetOk = false;
      }
      if (!clearTemplateStore(context, state)) {
        resetOk = false;
      }
      resetConnectivity(state);
      prepareFaceRecognition(context, state);
      return resetOk;
    }
    case UsbProvisioningCommandType::Invalid:
    default:
      return false;
  }
}

}  // namespace

void processUsbProvisioning(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  while (Serial.available()) {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      const std::string line = state.serialCommandBuffer;
      state.serialCommandBuffer.clear();
      if (line.empty()) {
        continue;
      }

      auto command = parseUsbProvisioningCommand(line);
      if (!command.has_value()) {
        Serial.println(buildUsbProvisioningResponseFrame(R"({"ok":false,"message":"invalid provisioning command"})").c_str());
        continue;
      }

      const bool ok = applyUsbProvisioningCommand(context, state, command.value(), nowMs);
      const std::string response = buildUsbProvisioningResponse(ok, ok ? "applied" : "rejected", state.deviceConfig, state.lastErrorCode);
      Serial.println(buildUsbProvisioningResponseFrame(response).c_str());
      state.renderDirty = true;
      continue;
    }

    if (state.serialCommandBuffer.size() < 4096) {
      state.serialCommandBuffer.push_back(ch);
    }
  }
}

}  // namespace app
