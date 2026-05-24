#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "app/usb_provisioning_protocol.hpp"

namespace {

void expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testParseSetWifiProfilesCommand() {
  auto command = app::parseUsbProvisioningCommand(
      R"({"type":"set_wifi_profiles","profiles":[{"ssid":"Lab","password":"secret","priority":5}]})");

  expect(command.has_value(), "wifi profile command should parse");
  expect(command->type == app::UsbProvisioningCommandType::SetWifiProfiles, "command type should match");
  expect(command->wifiProfiles.size() == 1, "wifi profiles should parse");
  expect(command->wifiProfiles.front().ssid == "Lab", "ssid should parse");
}

void testParseBootstrapIdentityCommand() {
  auto command = app::parseUsbProvisioningCommand(
      R"({"type":"set_bootstrap_identity","deviceSerial":"BOOT-001","bootstrapSecret":"secret"})");

  expect(command.has_value(), "bootstrap command should parse");
  expect(command->type == app::UsbProvisioningCommandType::SetBootstrapIdentity, "command type should match");
  expect(command->bootstrapIdentity.deviceSerial == "BOOT-001", "device serial should parse");
}

void testParseResetDeviceConfigCommand() {
  auto command = app::parseUsbProvisioningCommand(R"({"type":"reset_device_config"})");

  expect(command.has_value(), "reset command should parse");
  expect(command->type == app::UsbProvisioningCommandType::ResetDeviceConfig, "command type should match");
}

void testBuildProvisioningResponseOmitsSecrets() {
  core::DeviceConfig config = {};
  config.backendLocator.origin = "http://192.168.1.10:3000";
  config.bootstrapIdentity.deviceSerial = "BOOT-001";
  config.bootstrapIdentity.bootstrapSecret = "bootstrap-secret";
  config.runtimeCredentials.deviceCode = "DEV-001";
  config.runtimeCredentials.apiKey = "top-secret";
  config.wifiProfiles.push_back(core::WifiProfile{
      .ssid = "Lab",
      .password = "wifi-secret",
      .priority = 5,
      .lastSuccessAt = 123,
      .disabled = false,
  });

  const std::string response = app::buildUsbProvisioningResponse(true, "ok", config);
  expect(response.find("DEV-001") != std::string::npos, "response should include device code");
  expect(response.find("Lab") != std::string::npos, "response should include wifi profiles");
  expect(response.find("wifi-secret") != std::string::npos, "response should include wifi password for editing");
  expect(response.find("top-secret") == std::string::npos, "response should not include api key");
  expect(response.find("bootstrap-secret") == std::string::npos, "response should not include bootstrap secret");
}

}  // namespace

int main() {
  try {
    testParseSetWifiProfilesCommand();
    testParseBootstrapIdentityCommand();
    testParseResetDeviceConfigCommand();
    testBuildProvisioningResponseOmitsSecrets();
    std::cout << "[PASS] usb provisioning protocol" << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] usb provisioning protocol: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
