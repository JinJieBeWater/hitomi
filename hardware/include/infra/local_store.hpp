#pragma once

#include <vector>

#include "core/models.hpp"
#include "infra/template_store_port.hpp"

namespace infra {

struct LocalStoreInitStatus {
  bool credentialsReady = false;
  bool filesystemReady = false;
};

struct StorageAuxState {
  TemplateLibrarySummary templateLibrarySummary;
  TemplateStoreHealthSummary templateStoreHealth;
};

struct StoredRuntimeState {
  core::DeviceConfig deviceConfig;
  core::DeviceCredentials credentials;
  core::SnapshotBundle snapshots;
  std::vector<core::PendingEnrollmentReport> pendingEnrollmentReports;
  std::vector<core::PendingAttendanceRecord> pendingAttendanceRecords;
  std::vector<core::FailureLogEntry> failureLogs;
  StorageAuxState storageAux;
};

class LocalStore {
 public:
  virtual ~LocalStore() = default;

  virtual LocalStoreInitStatus begin() = 0;
  virtual StoredRuntimeState load() = 0;
  virtual bool saveDeviceConfig(const core::DeviceConfig& config) = 0;
  virtual bool clearDeviceConfig() = 0;
  virtual bool saveCredentials(const core::DeviceCredentials& credentials) = 0;
  virtual bool saveSnapshots(const core::SnapshotBundle& snapshots) = 0;
  virtual bool savePendingEnrollmentReports(
      const std::vector<core::PendingEnrollmentReport>& reports) = 0;
  virtual bool savePendingAttendanceRecords(
      const std::vector<core::PendingAttendanceRecord>& records) = 0;
  virtual bool saveFailureLogs(const std::vector<core::FailureLogEntry>& logs) = 0;
  virtual bool saveStorageAux(const StorageAuxState& storageAux) = 0;
};

}  // namespace infra
