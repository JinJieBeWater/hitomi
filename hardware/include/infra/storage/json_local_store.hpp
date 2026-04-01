#pragma once

#include <Preferences.h>

#include "infra/local_store.hpp"

namespace infra {

class CredentialsStore {
 public:
  bool begin();
  core::DeviceCredentials load();
  bool save(const core::DeviceCredentials& credentials);

 private:
  Preferences preferences_;
};

class SnapshotStore {
 public:
  core::SnapshotBundle load();
  bool save(const core::SnapshotBundle& snapshots);
};

class AttendanceQueueStore {
 public:
  std::vector<core::PendingAttendanceRecord> load();
  bool save(const std::vector<core::PendingAttendanceRecord>& records);
};

class FailureLogStore {
 public:
  std::vector<core::FailureLogEntry> load();
  bool save(const std::vector<core::FailureLogEntry>& logs);
};

class JsonLocalStore final : public LocalStore {
 public:
  bool begin() override;
  StoredRuntimeState load() override;
  bool saveCredentials(const core::DeviceCredentials& credentials) override;
  bool saveSnapshots(const core::SnapshotBundle& snapshots) override;
  bool savePendingAttendanceRecords(
      const std::vector<core::PendingAttendanceRecord>& records) override;
  bool saveFailureLogs(const std::vector<core::FailureLogEntry>& logs) override;

 private:
  CredentialsStore credentialsStore_;
  SnapshotStore snapshotStore_;
  AttendanceQueueStore attendanceQueueStore_;
  FailureLogStore failureLogStore_;
};

}  // namespace infra
