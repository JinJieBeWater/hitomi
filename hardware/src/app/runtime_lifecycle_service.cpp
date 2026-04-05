#include "app/runtime_lifecycle_service.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_ota_ops.h>

#include "app/runtime_diagnostics.hpp"
#include "app/runtime_state.hpp"
#include "board/app_config.hpp"
#include "board/pins.hpp"

namespace app {

void waitForSerial() {
  const unsigned long startedAt = millis();
  while (!Serial && millis() - startedAt < board::kSerialWaitTimeoutMs) {
    delay(10);
  }
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  if (!board::wifiConfigured()) {
    Serial.println("[APP] WiFi credentials not configured");
    return;
  }

  Serial.printf("[APP] connecting WiFi SSID=%s\n", board::wifiSsid());
  WiFi.begin(board::wifiSsid(), board::wifiPassword());
}

void pollBootButton(RuntimeState& state, uint32_t nowMs) {
  if (nowMs - state.lastButtonPollMs < board::kButtonPollIntervalMs) {
    return;
  }

  state.lastButtonPollMs = nowMs;
  const bool pressed = digitalRead(board::kBootKeyPin) == LOW;
  if (pressed == state.lastButtonPressed) {
    return;
  }

  state.lastButtonPressed = pressed;
  Serial.println(pressed ? "BOOT pressed" : "BOOT released");
}

void printRuntimeCheck(const RuntimeContext& context, const RuntimeState& state) {
  const esp_partition_t* partition = esp_ota_get_running_partition();
  const RuntimeDiagnostics diagnostics = buildRuntimeDiagnostics(buildRuntimeStatus(context, state));

  Serial.println();
  Serial.println("=== Hitomi Device Runtime ===");
  Serial.printf("FW tag: %s\n", board::kFirmwareTag);
  Serial.printf("Chip model: %s\n", ESP.getChipModel());
  Serial.printf("Chip revision: %d\n", ESP.getChipRevision());
  Serial.printf("SDK version: %s\n", ESP.getSdkVersion());
  Serial.printf("CPU frequency: %lu MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash size: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("PSRAM detected: %s\n", psramFound() ? "yes" : "no");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
  Serial.printf("Credentials store ready: %s\n", state.credentialsReady ? "yes" : "no");
  Serial.printf("LittleFS ready: %s\n", state.filesystemReady ? "yes" : "no");
  Serial.printf("Template store ready: %s\n", state.templateStoreReady ? "yes" : "no");
  Serial.printf("Display ready: %s\n", state.displayReady ? "yes" : "no");
  Serial.printf("WiFi configured: %s\n", board::wifiConfigured() ? "yes" : "no");
  Serial.printf("API configured: %s\n", board::apiConfigured() ? "yes" : "no");
  Serial.printf("Device credentials configured: %s\n", state.credentials.configured() ? "yes" : "no");
  Serial.println(diagnostics.credentialsLine.c_str());
  Serial.println(diagnostics.snapshotLine.c_str());
  Serial.println(diagnostics.storageLine.c_str());
  Serial.println(diagnostics.queueLine.c_str());
  Serial.println(diagnostics.faceLine.c_str());

  if (partition != nullptr) {
    Serial.printf("Running partition: %s (%u bytes)\n", partition->label, partition->size);
  }

  Serial.println("==============================");
}

}  // namespace app
