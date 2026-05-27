#include "app/runtime_provisioning_ops.hpp"

#include <Arduino.h>
#include <WiFi.h>

#include "app/runtime_lifecycle_service.hpp"
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
    case UsbProvisioningCommandType::ResetDeviceConfig:
      if (board::kEnableTemplateStore) {
        if (!state.templateStoreReady || !context.templateStore.clearTemplates()) {
          Serial.println("[APP] template reset failed; keeping device config unchanged");
          applyTemplateStoreStatus(context, state, context.templateStore.status(), false);
          persistStorageAux(context, state);
          return false;
        }
        applyTemplateStoreStatus(context, state, context.templateStore.status(), true);
        persistStorageAux(context, state);
      }
      state.deviceConfig = {};
      state.credentials = {};
      state.faceModuleEnabled = false;
      context.localStore.clearDeviceConfig();
      context.localStore.saveCredentials(state.credentials);
      resetConnectivity(state);
      return true;
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
