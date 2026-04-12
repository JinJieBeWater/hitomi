#include "app/runtime_diagnostics.hpp"

#include <sstream>

#include "app/runtime_face_status.hpp"
#include "infra/template_store_port.hpp"

namespace app {
namespace {

std::string storageLine(const RuntimeStatus& status) {
  if (!status.credentialsReady) {
    return "Storage: credentials unavailable";
  }
  if (!status.filesystemReady) {
    return "Storage: LittleFS unavailable";
  }
  if (infra::templateStoreDisabled(status.templateStoreStatusCode)) {
    return "Storage: SD disabled";
  }
  if (infra::templateStoreManifestBroken(status.templateStoreStatusCode)) {
    return "Storage: SD invalid manifest";
  }
  if (status.templateStoreStatusCode == infra::kTemplateStoreIoError) {
    return "Storage: SD io_error (" + status.templateStoreDetail + ")";
  }
  if (status.templateStoreStatusCode == infra::kTemplateStoreMountFailed) {
    return "Storage: SD mount_failed (" + status.templateStoreDetail + ")";
  }
  if (status.templateStoreStatusCode == infra::kTemplateStoreCardMissing) {
    return "Storage: SD card_missing";
  }
  if (infra::templateStoreMounted(status.templateStoreStatusCode)) {
    return "Storage: SD ready (templates=" + std::to_string(status.templateCount) + ")";
  }
  return status.templateStoreStatusCode.empty() ? "Storage: SD unavailable"
                                                : "Storage: " + status.templateStoreStatusCode;
}

bool hasLocalCache(const RuntimeStatus& status) {
  const core::SnapshotBundle& snapshots = status.snapshots;
  return !snapshots.deviceId.empty() || !snapshots.deviceName.empty() || !snapshots.deviceStatus.empty() ||
      snapshots.attendanceConfig.has_value() || !snapshots.employees.empty() || !snapshots.enrollmentTasks.empty() ||
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
  if (status.faceEngineReady || status.faceEngineStatusDetail.has_value()) {
    return formatFaceEngineLine(status, FaceLineStyle::Diagnostics);
  }
  if (infra::templateStoreDisabled(status.templateStoreStatusCode)) {
    return "Recognition: disabled (template store disabled)";
  }
  if (infra::templateStoreManifestBroken(status.templateStoreStatusCode)) {
    return "Recognition: disabled (SD invalid manifest)";
  }
  if (!status.templateStoreReady) {
    return "Recognition: disabled (template store unavailable)";
  }
  if (!status.faceModuleEnabled) {
    return "Recognition: disabled";
  }
  return "Recognition: enabled";
}

std::string faceDetectLine(const RuntimeStatus& status) {
  return formatFaceDetectLine(status, FaceLineStyle::Diagnostics);
}

}  // namespace

RuntimeDiagnostics buildRuntimeDiagnostics(const RuntimeStatus& status) {
  return {
      .credentialsLine = credentialsLine(status),
      .snapshotLine = snapshotLine(status),
      .storageLine = storageLine(status),
      .queueLine = queueLine(status),
      .faceLine = faceLine(status),
      .faceDetectLine = faceDetectLine(status),
  };
}

}  // namespace app
