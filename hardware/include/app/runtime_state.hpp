#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "app/runtime_context.hpp"
#include "app/runtime_status.hpp"
#include "infra/local_store.hpp"

namespace app {

struct WifiScanCandidate {
  std::size_t profileIndex = 0;
  int32_t rssi = 0;
  uint8_t channel = 0;
  uint8_t authMode = 0;
  std::array<uint8_t, 6> bssid{};
  bool hasBssid = false;
  bool matchesLastKnownGood = false;
};

struct WifiProfileRuntimeState {
  uint32_t cooldownUntilMs = 0;
  std::optional<uint8_t> lastDisconnectReason;
};

struct RuntimeState {
  core::DeviceConfig deviceConfig;
  core::DeviceCredentials credentials;
  core::SnapshotBundle snapshots;
  std::vector<core::PendingAttendanceRecord> pendingAttendanceRecords;
  std::vector<core::FailureLogEntry> failureLogs;
  infra::StorageAuxState storageAux;
  ConnectivityState connectivity = ConnectivityState::Unknown;
  bool credentialsReady = false;
  bool filesystemReady = false;
  bool templateStoreReady = false;
  bool displayReady = false;
  bool faceEngineReady = false;
  bool faceDetectReady = false;
  bool faceDetected = false;
  bool faceModuleEnabled = false;
  bool apiProbeInFlight = false;
  bool apiProbeSucceeded = false;
  bool syncInFlight = false;
  bool uploadInFlight = false;
  bool renderDirty = true;
  bool manualApiProbeRequested = false;
  bool manualActivationRequested = false;
  bool manualSyncRequested = false;
  bool lastButtonPressed = false;
  bool activationInFlight = false;
  bool wifiConnectInProgress = false;
  bool wifiScanInProgress = false;
  bool wifiShouldTryLastKnownGood = true;
  bool wifiConfiguredFallbackAttempted = false;
  uint32_t lastButtonPollMs = 0;
  uint32_t lastApiProbeAttemptMs = 0;
  uint32_t lastCameraPollMs = 0;
  uint32_t lastCameraStatusRenderMs = 0;
  uint32_t lastFaceDetectionMs = 0;
  uint32_t lastNetworkProbeMs = 0;
  uint32_t lastTemplateStoreProbeMs = 0;
  uint32_t lastWifiConnectAttemptMs = 0;
  uint32_t lastWifiRetryRoundMs = 0;
  uint32_t lastWifiScanRequestMs = 0;
  uint32_t lastWifiScanCompletedMs = 0;
  uint32_t networkRequestGeneration = 1;
  uint32_t lastSyncAttemptMs = 0;
  uint32_t lastUploadAttemptMs = 0;
  uint32_t lastActivationAttemptMs = 0;
  uint32_t lastWifiEventSequence = 0;
  std::size_t wifiCandidateCursor = 0;
  std::size_t detectedFaceCount = 0;
  std::optional<std::string> apiProbeStatusCode;
  std::optional<std::string> faceEngineStatusDetail;
  std::optional<std::string> faceDetectStatusDetail;
  std::optional<float> faceTopScore;
  std::optional<std::string> lastErrorCode;
  std::optional<uint8_t> lastWifiDisconnectReason;
  std::optional<std::size_t> activeWifiProfileIndex;
  std::optional<std::string> activeWifiSsid;
  std::string serialCommandBuffer;
  std::vector<WifiScanCandidate> wifiCandidates;
  std::vector<WifiProfileRuntimeState> wifiProfileRuntime;
};

bool facePortsReady(const RuntimeContext& context);
RuntimeStatus buildRuntimeStatus(const RuntimeContext& context, const RuntimeState& state);
void setLastError(RuntimeState& state, const std::optional<core::ApiError>& error);

}  // namespace app
