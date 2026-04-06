#include "app/usb_provisioning_protocol.hpp"

#include <ArduinoJson.h>

namespace app {
namespace {

const char* activationStateValue(core::DeviceActivationState state) {
  switch (state) {
    case core::DeviceActivationState::Activated:
      return "activated";
    case core::DeviceActivationState::PendingActivation:
      return "pending_activation";
    case core::DeviceActivationState::Unconfigured:
    default:
      return "unconfigured";
  }
}

}  // namespace

std::optional<UsbProvisioningCommand> parseUsbProvisioningCommand(const std::string& line) {
  StaticJsonDocument<2048> doc;
  const auto error = deserializeJson(doc, line);
  if (error) {
    return std::nullopt;
  }

  const std::string type = doc["type"] | "";
  UsbProvisioningCommand command = {};

  if (type == "get_config") {
    command.type = UsbProvisioningCommandType::GetConfig;
    return command;
  }

  if (type == "set_wifi_profiles") {
    command.type = UsbProvisioningCommandType::SetWifiProfiles;
    JsonArrayConst profiles = doc["profiles"].as<JsonArrayConst>();
    for (JsonVariantConst item : profiles) {
      command.wifiProfiles.push_back(core::WifiProfile{
          .ssid = item["ssid"] | "",
          .password = item["password"] | "",
          .priority = item["priority"] | 0,
          .lastSuccessAt = item["lastSuccessAt"] | 0ULL,
          .disabled = item["disabled"] | false,
      });
    }
    return command;
  }

  if (type == "set_backend_origin") {
    command.type = UsbProvisioningCommandType::SetBackendOrigin;
    command.backendOrigin = doc["origin"] | "";
    return command;
  }

  if (type == "set_bootstrap_identity") {
    command.type = UsbProvisioningCommandType::SetBootstrapIdentity;
    command.bootstrapIdentity = core::BootstrapIdentity{
        .deviceSerial = doc["deviceSerial"] | "",
        .bootstrapSecret = doc["bootstrapSecret"] | "",
    };
    return command;
  }

  if (type == "clear_runtime_credentials") {
    command.type = UsbProvisioningCommandType::ClearRuntimeCredentials;
    return command;
  }

  if (type == "clear_wifi_profiles") {
    command.type = UsbProvisioningCommandType::ClearWifiProfiles;
    return command;
  }

  if (type == "reset_device_config") {
    command.type = UsbProvisioningCommandType::ResetDeviceConfig;
    return command;
  }

  return std::nullopt;
}

std::string buildUsbProvisioningResponse(
    bool ok, const std::string& message, const core::DeviceConfig& config,
    const std::optional<std::string>& lastErrorCode) {
  StaticJsonDocument<512> doc;
  doc["ok"] = ok;
  doc["message"] = message;

  JsonObject summary = doc.createNestedObject("summary");
  summary["schemaVersion"] = config.schemaVersion;
  summary["wifiProfileCount"] = config.wifiProfiles.size();
  summary["backendOrigin"] = config.backendLocator.origin;
  summary["deviceSerial"] = config.bootstrapIdentity.deviceSerial;
  summary["activationState"] = activationStateValue(config.activationState());
  summary["deviceCode"] = config.runtimeCredentials.deviceCode;
  if (lastErrorCode.has_value()) {
    summary["lastErrorCode"] = lastErrorCode.value();
  } else {
    summary["lastErrorCode"] = nullptr;
  }

  std::string output;
  serializeJson(doc, output);
  return output;
}

}  // namespace app
