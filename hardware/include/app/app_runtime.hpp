#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "app/runtime_status.hpp"
#include "face/ports.hpp"
#include "infra/device_api_client.hpp"
#include "infra/display_port.hpp"
#include "infra/local_store.hpp"

namespace app {

class AppRuntime {
 public:
  AppRuntime(
      infra::DisplayPort& display,
      infra::LocalStore& localStore,
      infra::DeviceApiClient& deviceApiClient,
      face::CameraPort& camera,
      face::EnrollmentServicePort& enrollmentService,
      face::RecognitionServicePort& recognitionService);

  void setup();
  void tick(uint32_t nowMs);

 private:
  void waitForSerial();
  void printRuntimeCheck();
  void loadPersistedState();
  void initWifi();
  void pollBootButton(uint32_t nowMs);
  void probeConnectivity(uint32_t nowMs);
  bool shouldSync(uint32_t nowMs) const;
  bool shouldUpload(uint32_t nowMs) const;
  void performSync(uint32_t nowMs);
  void performUpload(uint32_t nowMs);
  void markUploadAttemptFailure(
      const std::vector<core::PendingAttendanceRecord>& batch,
      uint64_t occurredAt,
      const std::string& errorCode);
  void appendApiFailureLog(
      const char* api, uint64_t occurredAt, const core::ApiError& error, std::optional<std::string> relatedId);
  void updateView();
  void setLastError(const std::optional<core::ApiError>& error);

  infra::DisplayPort& display_;
  infra::LocalStore& localStore_;
  infra::DeviceApiClient& deviceApiClient_;
  face::CameraPort& camera_;
  face::EnrollmentServicePort& enrollmentService_;
  face::RecognitionServicePort& recognitionService_;

  core::DeviceCredentials credentials_;
  core::SnapshotBundle snapshots_;
  std::vector<core::PendingAttendanceRecord> pendingAttendanceRecords_;
  std::vector<core::FailureLogEntry> failureLogs_;
  ConnectivityState connectivity_ = ConnectivityState::Unknown;
  bool storageReady_ = false;
  bool displayReady_ = false;
  bool faceModuleEnabled_ = false;
  bool syncInFlight_ = false;
  bool uploadInFlight_ = false;
  bool renderDirty_ = true;
  bool lastButtonPressed_ = false;
  uint32_t lastButtonPollMs_ = 0;
  uint32_t lastNetworkProbeMs_ = 0;
  uint32_t lastSyncAttemptMs_ = 0;
  uint32_t lastUploadAttemptMs_ = 0;
  std::optional<std::string> lastErrorCode_;
};

}  // namespace app
