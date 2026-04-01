#include "app/runtime_diagnostics.hpp"

#include <sstream>

namespace app {
namespace {

bool hasLocalCache(const RuntimeStatus& status) {
  const core::SnapshotBundle& snapshots = status.snapshots;
  return !snapshots.deviceId.empty() || !snapshots.deviceName.empty() || !snapshots.deviceStatus.empty() ||
      snapshots.attendanceConfig.has_value() || !snapshots.employees.empty() || snapshots.enrollmentTask.has_value() ||
      snapshots.lastSyncAt != 0 || snapshots.lastServerTime != 0;
}

std::string credentialsLine(const RuntimeStatus& status) {
  if (!status.credentialsReady) {
    return "Credentials store: unavailable";
  }
  if (!status.credentials.configured()) {
    return "Credentials: missing";
  }
  return "Credentials: ready (" + status.credentials.deviceCode + ")";
}

std::string snapshotLine(const RuntimeStatus& status) {
  if (!status.filesystemReady) {
    return "Local cache: unavailable";
  }
  if (!hasLocalCache(status)) {
    return "Local cache: empty";
  }

  std::ostringstream oss;
  oss << "Local cache: ready (employees=" << status.snapshots.employees.size();
  if (status.snapshots.lastSyncAt != 0) {
    oss << ", lastSync=" << status.snapshots.lastSyncAt;
  }
  oss << ")";
  return oss.str();
}

std::string queueLine(const RuntimeStatus& status) {
  std::ostringstream oss;
  oss << "Pending uploads: " << status.pendingQueueSize << ", failure logs: " << status.failureLogCount;
  return oss.str();
}

std::string faceLine(const RuntimeStatus& status) {
  if (!status.templateStoreReady) {
    return "Recognition: disabled (template store unavailable)";
  }
  if (!status.faceModuleEnabled) {
    return "Recognition: disabled";
  }
  return "Recognition: enabled";
}

}  // namespace

RuntimeDiagnostics buildRuntimeDiagnostics(const RuntimeStatus& status) {
  return {
      .credentialsLine = credentialsLine(status),
      .snapshotLine = snapshotLine(status),
      .queueLine = queueLine(status),
      .faceLine = faceLine(status),
  };
}

}  // namespace app
