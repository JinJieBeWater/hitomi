#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/models.hpp"
#include "face/ports.hpp"

namespace app {

enum class ConnectivityState {
  Unknown,
  Disconnected,
  Connected,
};

struct RuntimeStatus {
  std::string firmwareTag;
  core::DeviceCredentials credentials;
  core::SnapshotBundle snapshots;
  std::size_t pendingQueueSize = 0;
  std::size_t failureLogCount = 0;
  ConnectivityState connectivity = ConnectivityState::Unknown;
  bool credentialsReady = false;
  bool filesystemReady = false;
  bool templateStoreReady = false;
  bool faceEngineReady = false;
  bool faceDetectReady = false;
  bool faceDetected = false;
  std::string templateStoreStatusCode;
  std::size_t templateCount = 0;
  uint64_t sdTotalBytes = 0;
  uint64_t sdUsedBytes = 0;
  bool wifiConfigured = false;
  std::optional<std::string> activeWifiSsid;
  core::DeviceActivationState activationState = core::DeviceActivationState::Unconfigured;
  bool bootstrapConfigured = false;
  std::optional<std::string> activationRegistrationId;
  bool apiConfigured = false;
  bool apiProbeInFlight = false;
  bool apiProbeSucceeded = false;
  bool activationInFlight = false;
  std::optional<std::string> apiProbeStatusCode;
  bool syncInFlight = false;
  bool uploadInFlight = false;
  std::optional<std::string> faceEngineStatusDetail;
  std::optional<std::string> faceDetectStatusDetail;
  std::optional<float> faceTopScore;
  std::size_t detectedFaceCount = 0;
  bool faceModuleEnabled = false;
  bool cameraAvailable = false;
  bool cameraReady = false;
  uint32_t cameraCaptureCount = 0;
  uint32_t cameraFailedCaptureCount = 0;
  face::CameraFrameInfo cameraLastFrame = {};
  std::string cameraSensorModel;
  std::string cameraLastError;
  std::optional<std::string> lastErrorCode;
};

}  // namespace app
