#pragma once

#include <vector>

#include "core/models.hpp"

namespace infra {

struct LocalStoreInitStatus {
  bool credentialsReady = false;
  bool filesystemReady = false;
};

struct StoredRuntimeState {
  core::DeviceCredentials credentials;
  core::SnapshotBundle snapshots;
  std::vector<core::PendingAttendanceRecord> pendingAttendanceRecords;
  std::vector<core::FailureLogEntry> failureLogs;
};

class LocalStore {
 public:
  virtual ~LocalStore() = default;

  virtual LocalStoreInitStatus begin() = 0;
  virtual StoredRuntimeState load() = 0;
  virtual bool saveCredentials(const core::DeviceCredentials& credentials) = 0;
  virtual bool saveSnapshots(const core::SnapshotBundle& snapshots) = 0;
  virtual bool savePendingAttendanceRecords(
      const std::vector<core::PendingAttendanceRecord>& records) = 0;
  virtual bool saveFailureLogs(const std::vector<core::FailureLogEntry>& logs) = 0;
};

}  // namespace infra
