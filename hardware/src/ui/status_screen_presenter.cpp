#include "ui/status_screen_presenter.hpp"

#include <sstream>

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

std::string taskLabel(const app::RuntimeStatus& status) {
  if (!status.snapshots.enrollmentTask.has_value()) {
    return "Task: none";
  }

  const auto& task = status.snapshots.enrollmentTask.value();
  std::ostringstream oss;
  oss << "Task: " << task.employeeName << " (" << task.status << ")";
  return oss.str();
}

}  // namespace

AppViewModel StatusScreenPresenter::build(const app::RuntimeStatus& status) {
  AppViewModel view = {};
  view.title = "Hitomi Device";
  view.subtitle = status.snapshots.deviceName.empty() ? "SZPI ESP32-S3" : status.snapshots.deviceName;
  view.credentialsLine = status.credentials.configured()
      ? "Credentials: " + status.credentials.deviceCode
      : "Credentials: missing";
  view.wifiLine = "WiFi: " + connectivityLabel(status.connectivity);
  view.syncLine = "Sync: " + syncLabel(status);
  view.taskLine = taskLabel(status);
  view.queueLine = "Queue: " + std::to_string(status.pendingQueueSize);
  view.errorLine = "Last error: " + status.lastErrorCode.value_or("none");
  view.faceLine = std::string("Face module: ") + (status.faceModuleEnabled ? "Enabled" : "Disabled");
  view.footer = status.firmwareTag;
  return view;
}

}  // namespace ui
