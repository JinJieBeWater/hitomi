#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace core {

enum class AttendanceRecordType {
  ClockIn,
  ClockOut,
};

enum class AttendanceUploadStatus {
  Saved,
  UpdatedEarlier,
  IgnoredDuplicate,
  Rejected,
};

enum class QueueMutationAction {
  Inserted,
  ReplacedWithEarlier,
  IgnoredLaterOrEqual,
};

enum class BackendLocatorMode {
  FixedOrigin,
};

enum class DeviceActivationState {
  Unconfigured,
  PendingActivation,
  Activated,
};

struct DeviceCredentials {
  std::string deviceCode;
  std::string apiKey;

  bool configured() const {
    return !deviceCode.empty() && !apiKey.empty();
  }
};

struct WifiProfile {
  std::string ssid;
  std::string password;
  int priority = 0;
  uint64_t lastSuccessAt = 0;
  bool disabled = false;

  bool configured() const {
    return !ssid.empty() && !password.empty() && !disabled;
  }
};

struct BackendLocator {
  BackendLocatorMode mode = BackendLocatorMode::FixedOrigin;
  std::string origin;

  bool configured() const {
    return !origin.empty();
  }
};

struct BootstrapIdentity {
  std::string deviceSerial;
  std::string bootstrapSecret;

  bool configured() const {
    return !deviceSerial.empty() && !bootstrapSecret.empty();
  }
};

struct DeviceConfig {
  uint32_t schemaVersion = 1;
  std::vector<WifiProfile> wifiProfiles;
  BackendLocator backendLocator;
  BootstrapIdentity bootstrapIdentity;
  DeviceCredentials runtimeCredentials;
  bool diagnosticsEnabled = false;

  bool wifiConfigured() const {
    for (const auto& profile : wifiProfiles) {
      if (profile.configured()) {
        return true;
      }
    }
    return false;
  }

  DeviceActivationState activationState() const {
    if (runtimeCredentials.configured()) {
      return DeviceActivationState::Activated;
    }
    if (wifiConfigured() && backendLocator.configured() && bootstrapIdentity.configured()) {
      return DeviceActivationState::PendingActivation;
    }
    return DeviceActivationState::Unconfigured;
  }
};

struct AttendanceConfigSnapshot {
  std::string id;
  int workStartMinute = 0;
  int workEndMinute = 0;
  int offStartMinute = 0;
  int offEndMinute = 0;
  uint64_t updatedAt = 0;
};

struct EmployeeSnapshot {
  std::string id;
  std::string code;
  std::string name;
  uint64_t updatedAt = 0;
};

struct EnrollmentTaskSnapshot {
  std::string taskId;
  std::string employeeId;
  std::string employeeCode;
  std::string employeeName;
  std::string status;
  uint64_t createdAt = 0;
  uint64_t updatedAt = 0;
};

struct PendingAttendanceRecord {
  std::string clientRecordId;
  std::string employeeId;
  uint64_t recognizedAt = 0;
  AttendanceRecordType type = AttendanceRecordType::ClockIn;
  std::string localDate;
  uint64_t createdAt = 0;
  std::optional<uint64_t> lastAttemptAt;
  std::optional<std::string> lastResultCode;
};

struct FailureLogEntry {
  std::string id;
  std::string api;
  uint64_t occurredAt = 0;
  std::string code;
  std::string message;
  std::optional<std::string> relatedId;
};

struct ApiError {
  std::string code;
  std::string message;
  bool retryable = false;
};

struct AttendanceUploadItemResult {
  std::string clientRecordId;
  AttendanceUploadStatus status = AttendanceUploadStatus::Saved;
  std::optional<std::string> attendanceRecordId;
  std::optional<ApiError> error;
};

struct SyncPayload {
  uint64_t serverTime = 0;
  std::string timezone = "Asia/Shanghai";
  std::string deviceId;
  std::string deviceName;
  std::string deviceStatus;
  std::optional<AttendanceConfigSnapshot> attendanceConfig;
  std::vector<EmployeeSnapshot> employees;
  std::vector<EnrollmentTaskSnapshot> enrollmentTasks;
};

struct SnapshotBundle {
  std::string timezone = "Asia/Shanghai";
  std::string deviceId;
  std::string deviceName;
  std::string deviceStatus;
  std::optional<AttendanceConfigSnapshot> attendanceConfig;
  uint64_t attendanceConfigSyncedAt = 0;
  std::vector<EmployeeSnapshot> employees;
  uint64_t employeesSyncedAt = 0;
  std::vector<EnrollmentTaskSnapshot> enrollmentTasks;
  uint64_t enrollmentTaskSyncedAt = 0;
  uint64_t lastSyncAt = 0;
  uint64_t lastServerTime = 0;
};

struct QueueMutationResult {
  QueueMutationAction action = QueueMutationAction::Inserted;
  std::size_t index = 0;
};

struct UploadApplicationResult {
  std::vector<PendingAttendanceRecord> queue;
  std::vector<FailureLogEntry> logs;
};

}  // namespace core
