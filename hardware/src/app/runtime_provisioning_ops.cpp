#include "app/runtime_provisioning_ops.hpp"

#include <Arduino.h>
#include <WiFi.h>

#include "app/runtime_network_ops.hpp"
#include "app/runtime_storage_ops.hpp"
#include "app/usb_provisioning_protocol.hpp"
#include "core/use_cases.hpp"
#include "infra/device_api_client.hpp"

namespace app {
namespace {

void resetConnectivity(RuntimeState& state) {
  WiFi.disconnect(false, false);
  WiFi.scanDelete();
  state.wifiScanInProgress = false;
  state.connectivity = ConnectivityState::Disconnected;
  state.activeWifiSsid.reset();
  state.lastWifiConnectAttemptMs = 0;
  state.lastActivationAttemptMs = 0;
  state.apiProbeSucceeded = false;
  state.apiProbeStatusCode = std::nullopt;
  state.activationInFlight = false;
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
      context.deviceApiClient.setBaseUrl(state.deviceConfig.backendLocator.origin);
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
      state.deviceConfig = {};
      state.credentials = {};
      context.localStore.clearDeviceConfig();
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
        Serial.println(R"({"ok":false,"message":"invalid provisioning command"})");
        continue;
      }

      const bool ok = applyUsbProvisioningCommand(context, state, command.value(), nowMs);
      const std::string response = buildUsbProvisioningResponse(ok, ok ? "applied" : "rejected", state.deviceConfig, state.lastErrorCode);
      Serial.println(response.c_str());
      state.renderDirty = true;
      continue;
    }

    if (state.serialCommandBuffer.size() < 4096) {
      state.serialCommandBuffer.push_back(ch);
    }
  }
}

}  // namespace app
