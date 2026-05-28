#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifndef HITOMI_WIFI_SSID
#define HITOMI_WIFI_SSID ""
#endif

#ifndef HITOMI_WIFI_PASSWORD
#define HITOMI_WIFI_PASSWORD ""
#endif

#ifndef HITOMI_API_BASE_URL
#define HITOMI_API_BASE_URL ""
#endif

namespace board {

constexpr char kFirmwareTag[] = "hardware-runtime-2026-04-01-01";
constexpr uint32_t kSerialBaudRate = 115200;
constexpr unsigned long kSerialWaitTimeoutMs = 2000;
constexpr unsigned long kLoopYieldDelayMs = 1;
constexpr uint32_t kLvglHandlerPeriodMs = 5;
constexpr uint32_t kButtonPollIntervalMs = 50;
constexpr uint32_t kNetworkProbeIntervalMs = 250;
constexpr uint32_t kWifiConnectRetryIntervalMs = 3 * 1000;
constexpr uint32_t kWifiConnectAttemptTimeoutMs = 5 * 1000;
constexpr uint32_t kWifiReconnectCooldownMs = 1500;
constexpr uint32_t kWifiScanRetryIntervalMs = 8 * 1000;
constexpr uint32_t kWifiAuthFailureCooldownMs = 2 * 60 * 1000;
constexpr uint32_t kApiProbeIntervalMs = 15 * 1000;
constexpr uint32_t kActivationRetryIntervalMs = 5 * 1000;
constexpr uint32_t kSyncIntervalMs = 60 * 1000;
constexpr uint32_t kUploadRetryIntervalMs = 15 * 1000;
constexpr uint32_t kTemplateStoreProbeIntervalMs = 2000;
constexpr uint32_t kCameraPreviewIntervalMs = 33;
constexpr uint32_t kCameraPreviewRenderIntervalMs = 120;
constexpr uint32_t kCameraStatusRefreshIntervalMs = 500;
constexpr uint32_t kFaceDetectIntervalMs = 750;
constexpr float kFaceRecognitionThreshold = 0.5F;
constexpr uint32_t kFaceRecognitionCooldownMs = 5000;
constexpr uint32_t kEnrollmentFrameIntervalMs = 250;
constexpr uint32_t kEnrollmentSessionTimeoutMs = 15 * 1000;
constexpr uint32_t kCameraPowerStabilizeDelayMs = 30;
constexpr uint32_t kCameraXclkHz = 20 * 1000 * 1000;
constexpr std::size_t kCameraFrameBufferCount = 2;
constexpr std::size_t kMaxFaceBoxes = 4;
constexpr bool kCameraRgb565BigEndian = true;
constexpr uint32_t kHttpRequestTimeoutMs = 5000;
constexpr std::size_t kFailureLogLimit = 200;
constexpr std::size_t kEnrollmentRequiredSamples = 3;
constexpr std::size_t kUploadBatchSize = 100;
constexpr char kSdMountPoint[] = "/sdcard";
constexpr bool kSdMode1Bit = true;
constexpr bool kSdFormatIfMountFailed = false;
constexpr uint8_t kSdMaxOpenFiles = 5;
constexpr bool kEnableTemplateStore = true;

inline const char* wifiSsid() {
  return HITOMI_WIFI_SSID;
}

inline const char* wifiPassword() {
  return HITOMI_WIFI_PASSWORD;
}

inline const char* apiBaseUrl() {
  return HITOMI_API_BASE_URL;
}

inline bool wifiConfigured() {
  return std::strlen(wifiSsid()) > 0;
}

inline bool apiConfigured() {
  return std::strlen(apiBaseUrl()) > 0;
}

}  // namespace board
