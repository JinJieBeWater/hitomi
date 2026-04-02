#include "ui/status_screen_presenter.hpp"

#include <sstream>

#include "infra/template_store_port.hpp"

namespace ui {
namespace {

std::string connectivityLabel(app::ConnectivityState state) {
  switch (state) {
    case app::ConnectivityState::Connected:
      return "Connected";
    case app::ConnectivityState::Disconnected:
      return "Disconnected";
    case app::ConnectivityState::Unknown:
    default:
      return "Unknown";
  }
}

std::string syncLabel(const app::RuntimeStatus& status) {
  if (status.syncInFlight) {
    return "Syncing";
  }
  if (status.snapshots.lastSyncAt == 0) {
    return "Waiting";
  }

  std::ostringstream oss;
  oss << "Ready @" << status.snapshots.lastSyncAt;
  return oss.str();
}

std::string subtitleLabel(const app::RuntimeStatus& status) {
  if (status.snapshots.deviceName.empty()) {
    return "SZPI ESP32-S3";
  }
  return status.snapshots.deviceName;
}

std::string credentialsLabel(const app::RuntimeStatus& status) {
  if (!status.credentials.configured()) {
    return "Credentials: missing";
  }
  return "Credentials: " + status.credentials.deviceCode;
}

std::string taskLabel(const app::RuntimeStatus& status) {
  if (!status.snapshots.enrollmentTask.has_value()) {
    return "Task: none";
  }

  const auto& task = status.snapshots.enrollmentTask.value();
  std::ostringstream oss;
  oss << "Task: " << task.employeeName << " (" << task.status << ")";
  return oss.str();
}

std::string storageLabel(const app::RuntimeStatus& status) {
  if (!status.credentialsReady) {
    return "Storage: credentials unavailable";
  }
  if (!status.filesystemReady) {
    return "Storage: LittleFS unavailable";
  }
  if (infra::templateStoreManifestBroken(status.templateStoreStatusCode)) {
    return "Storage: SD invalid manifest";
  }
  if (infra::templateStoreMounted(status.templateStoreStatusCode)) {
    std::ostringstream oss;
    oss << "Storage: SD ready (templates=" << status.templateCount << ")";
    return oss.str();
  }
  if (!status.templateStoreReady) {
    return "Storage: SD unavailable";
  }
  return "Storage: ready";
}

std::string faceModuleLabel(const app::RuntimeStatus& status) {
  return std::string("Face module: ") + (status.faceModuleEnabled ? "Enabled" : "Disabled");
}

}  // namespace

AppViewModel StatusScreenPresenter::build(const app::RuntimeStatus& status) {
  AppViewModel view = {};
  view.title = "Hitomi Device";
  view.subtitle = subtitleLabel(status);
  view.credentialsLine = credentialsLabel(status);
  view.storageLine = storageLabel(status);
  view.wifiLine = "WiFi: " + connectivityLabel(status.connectivity);
  view.syncLine = "Sync: " + syncLabel(status);
  view.taskLine = taskLabel(status);
  view.queueLine = "Queue: " + std::to_string(status.pendingQueueSize);
  view.errorLine = "Last error: " + status.lastErrorCode.value_or("none");
  view.faceLine = faceModuleLabel(status);
  view.footer = status.firmwareTag;
  return view;
}

}  // namespace ui
