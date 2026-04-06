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
    return "Credentials: " + status.credentials.deviceCode;
  }
  if (status.bootstrapConfigured) {
    return "Credentials: bootstrap only";
  }
  return "Credentials: missing";
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

std::string apiLabel(const app::RuntimeStatus& status) {
  if (!status.apiConfigured) {
    return "API: missing origin";
  }
  if (status.apiProbeInFlight) {
    return "API: probing";
  }
  if (status.connectivity != app::ConnectivityState::Connected) {
    return "API: waiting for WiFi";
  }
  if (status.apiProbeSucceeded) {
    return "API: reachable";
  }
  if (status.apiProbeStatusCode.has_value()) {
    return "API: failed (" + status.apiProbeStatusCode.value() + ")";
  }
  return "API: waiting";
}

uint64_t effectiveStatusTime(const app::RuntimeStatus& status) {
  if (status.snapshots.lastServerTime > 0) {
    return status.snapshots.lastServerTime;
  }

  return static_cast<uint64_t>(std::time(nullptr)) * 1000ULL;
}

std::string periodLabel(const app::RuntimeStatus& status) {
  if (!status.snapshots.attendanceConfig.has_value()) {
    return "当前时段: 未配置";
  }

  const auto attendanceType =
      core::classifyAttendanceType(status.snapshots.attendanceConfig.value(), effectiveStatusTime(status));
  if (!attendanceType.has_value()) {
    return "当前时段: 非打卡时间";
  }

  return *attendanceType == core::AttendanceRecordType::ClockIn ? "当前时段: 上班" : "当前时段: 下班";
}

std::string attendanceResultLabel(const app::RuntimeStatus& status) {
  if (status.pendingQueueSize > 0) {
    return "最近打卡成功: 已缓存 " + std::to_string(status.pendingQueueSize) + " 条记录";
  }

  if (status.lastErrorCode.has_value()) {
    return "最近打卡: 等待业务链路接入 (" + status.lastErrorCode.value() + ")";
  }

  return "最近打卡成功: 占位，等待识别与打卡链路接入";
}

std::vector<EnrollmentTaskItemViewModel> enrollmentTasks(const app::RuntimeStatus& status) {
  std::vector<EnrollmentTaskItemViewModel> items;
  items.reserve(status.snapshots.enrollmentTasks.size());

  for (const auto& task : status.snapshots.enrollmentTasks) {
    std::ostringstream meta;
    meta << task.employeeCode << " | " << task.status << " | " << task.taskId;
    items.push_back(EnrollmentTaskItemViewModel{
        .taskId = task.taskId,
        .title = task.employeeName,
        .meta = meta.str(),
    });
  }

  return items;
}

std::string enrollmentTaskSummary(const app::RuntimeStatus& status) {
  if (status.snapshots.enrollmentTasks.empty()) {
    return "当前设备暂无录脸任务";
  }

  return "待处理录脸任务: " + std::to_string(status.snapshots.enrollmentTasks.size()) + " 条";
}

}  // namespace

AppViewModel StatusScreenPresenter::build(const app::RuntimeStatus& status) {
  AppViewModel view = {};
  view.title = "Hitomi Device";
  view.subtitle = subtitleLabel(status);
  view.periodLine = periodLabel(status);
  view.cameraHintLine = "摄像头画面占位";
  view.attendanceResultLine = attendanceResultLabel(status);
  view.enrollmentTasks = enrollmentTasks(status);
  view.enrollmentTaskSummaryLine = enrollmentTaskSummary(status);
  view.credentialsLine = credentialsLabel(status);
  view.storageLine = storageLabel(status);
  view.wifiLine = "WiFi: " + connectivityLabel(status.connectivity) +
      (status.activeWifiSsid.has_value() ? " (" + status.activeWifiSsid.value() + ")" : "");
  view.activationLine = activationLabel(status);
  view.apiLine = apiLabel(status);
  view.syncLine = "Sync: " + syncLabel(status);
  view.taskLine = taskLabel(status);
  view.queueLine = "Queue: " + std::to_string(status.pendingQueueSize);
  view.errorLine = "Last error: " + status.lastErrorCode.value_or("none");
  view.faceLine = faceModuleLabel(status);
  view.footer = status.firmwareTag;
  return view;
}

}  // namespace ui
