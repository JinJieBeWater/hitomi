#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/models.hpp"

namespace app {

enum class UsbProvisioningCommandType {
  Invalid,
  GetConfig,
  SetWifiProfiles,
  SetBackendOrigin,
  SetBootstrapIdentity,
  ClearRuntimeCredentials,
  ClearWifiProfiles,
  ResetDeviceConfig,
};

struct UsbProvisioningCommand {
  UsbProvisioningCommandType type = UsbProvisioningCommandType::Invalid;
  std::vector<core::WifiProfile> wifiProfiles;
  std::string backendOrigin;
  core::BootstrapIdentity bootstrapIdentity;
  std::string error;
};

std::optional<UsbProvisioningCommand> parseUsbProvisioningCommand(const std::string& line);
std::string buildUsbProvisioningResponse(bool ok, const std::string& message, const core::DeviceConfig& config);

}  // namespace app
