#include "ui/status_screen_presenter.hpp"

#include <ctime>
#include <sstream>

#include "core/use_cases.hpp"
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

std::string activationLabel(const app::RuntimeStatus& status) {
  if (status.activationInFlight) {
    return "Activation: pending";
  }

  switch (status.activationState) {
    case core::DeviceActivationState::Activated:
      return "Activation: activated";
    case core::DeviceActivationState::PendingActivation: {
      if (!status.activationRegistrationId.has_value()) {
        return "Activation: waiting for claim";
      }

      std::string code = status.activationRegistrationId.value();
      if (code.size() > 12) {
        code = code.substr(0, 12);
      }
      return "Activation: waiting for claim (" + code + ")";
    }
    case core::DeviceActivationState::Unconfigured:
    default:
      return "Activation: setup required";
  }
}

std::string syncLabel(const app::RuntimeStatus& status) {
  if (!status.credentials.configured()) {
    return "Inactive";
  }
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
  if (status.credentials.configured()) {
    return status.credentials.deviceCode;
  }
  if (status.bootstrapConfigured) {
    return "Bootstrap only";
  }
  return "Missing";
}

std::string taskLabel(const app::RuntimeStatus& status) {
  if (!status.credentials.configured()) {
    return "Task: unavailable";
  }
  if (status.snapshots.enrollmentTasks.empty()) {
    return "Task: none";
  }

  const auto& task = status.snapshots.enrollmentTasks.front();
  const std::size_t remaining = status.snapshots.enrollmentTasks.size() - 1;
  std::ostringstream oss;
  oss << "Task: " << task.employeeName;
  if (remaining > 0) {
    oss << " +" << remaining << " more";
  }
  return oss.str();
}

std::string storageLabel(const app::RuntimeStatus& status) {
  if (!status.credentialsReady) {
    return "Credentials unavailable";
  }
  if (!status.filesystemReady) {
    return "LittleFS unavailable";
  }
  if (infra::templateStoreManifestBroken(status.templateStoreStatusCode)) {
    return "SD invalid manifest";
  }
  if (infra::templateStoreMounted(status.templateStoreStatusCode)) {
    std::ostringstream oss;
    oss << "SD ready (templates=" << status.templateCount << ")";
    return oss.str();
  }
  if (!status.templateStoreReady) {
    return "SD unavailable";
  }
  return "Ready";
}

std::string faceModuleLabel(const app::RuntimeStatus& status) {
  return std::string("Face module: ") + (status.faceModuleEnabled ? "Enabled" : "Disabled");
}

std::string apiLabel(const app::RuntimeStatus& status) {
  if (!status.apiConfigured) {
    return "Missing origin";
  }
  if (status.apiProbeInFlight) {
    return "Probing";
  }
  if (status.connectivity != app::ConnectivityState::Connected) {
    return "Waiting for WiFi";
  }
  if (status.apiProbeSucceeded) {
    return "Reachable";
  }
  if (status.apiProbeStatusCode.has_value()) {
    return "Failed (" + status.apiProbeStatusCode.value() + ")";
  }
  return "Waiting";
}

uint64_t effectiveStatusTime(const app::RuntimeStatus& status) {
  if (status.snapshots.lastServerTime > 0) {
    return status.snapshots.lastServerTime;
  }

  return static_cast<uint64_t>(std::time(nullptr)) * 1000ULL;
}

std::string periodLabel(const app::RuntimeStatus& status) {
  if (!status.snapshots.attendanceConfig.has_value()) {
    return "Period: not configured";
  }

  const auto attendanceType =
      core::classifyAttendanceType(status.snapshots.attendanceConfig.value(), effectiveStatusTime(status));
  if (!attendanceType.has_value()) {
    return "Period: outside window";
  }

  return *attendanceType == core::AttendanceRecordType::ClockIn ? "Period: Clock In" : "Period: Clock Out";
}

std::string attendanceResultLabel(const app::RuntimeStatus& status) {
  if (status.pendingQueueSize > 0) {
    return std::to_string(status.pendingQueueSize) + " record(s) queued";
  }

  if (status.lastErrorCode.has_value()) {
    return "Error: " + status.lastErrorCode.value();
  }

  return "";
}

std::vector<EnrollmentTaskItemViewModel> enrollmentTasks(const app::RuntimeStatus& status) {
  std::vector<EnrollmentTaskItemViewModel> items;
  items.reserve(status.snapshots.enrollmentTasks.size());

  for (const auto& task : status.snapshots.enrollmentTasks) {
    const std::string title =
        task.employeeCode.empty() ? task.employeeName : task.employeeCode + " " + task.employeeName;
    items.push_back(EnrollmentTaskItemViewModel{
        .taskId = task.taskId,
        .title = title,
        .meta = "Status: " + task.status,
    });
  }

  return items;
}

std::string enrollmentTaskSummary(const app::RuntimeStatus& status) {
  if (status.snapshots.enrollmentTasks.empty()) {
    return "No enrollment tasks";
  }

  return "Enrollment tasks: " + std::to_string(status.snapshots.enrollmentTasks.size()) + " pending";
}

}  // namespace

AppViewModel StatusScreenPresenter::build(const app::RuntimeStatus& status) {
  AppViewModel view = {};
  view.title = "Hitomi Device";
  view.subtitle = subtitleLabel(status);
  view.periodLine = periodLabel(status);
  view.cameraHintLine = "Camera feed placeholder";
  view.attendanceResultLine = attendanceResultLabel(status);
  view.enrollmentTasks = enrollmentTasks(status);
  view.enrollmentTaskSummaryLine = enrollmentTaskSummary(status);
  view.credentialsLine = credentialsLabel(status);
  view.storageLine = storageLabel(status);
  view.wifiLine = connectivityLabel(status.connectivity) +
      (status.activeWifiSsid.has_value() ? " (" + status.activeWifiSsid.value() + ")" : "");
  view.activationLine = activationLabel(status);
  view.apiLine = apiLabel(status);
  view.syncLine = "Sync: " + syncLabel(status);
  view.taskLine = taskLabel(status);
  view.queueLine = "Queue: " + std::to_string(status.pendingQueueSize);
  view.errorLine = status.lastErrorCode.value_or("None");
  view.faceLine = faceModuleLabel(status);
  view.footer = status.firmwareTag;
  return view;
}

}  // namespace ui
