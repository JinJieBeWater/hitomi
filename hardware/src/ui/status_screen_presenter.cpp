#include "ui/status_screen_presenter.hpp"

#include <algorithm>
#include <ctime>
#include <sstream>
#include <string_view>

#include "app/runtime_face_status.hpp"
#include "core/use_cases.hpp"
#include "infra/template_store_port.hpp"

namespace ui {
namespace {

bool startsWith(const std::string& text, std::string_view prefix) {
  return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix.data()) == 0;
}

bool isAsciiDisplaySafe(std::string_view value) {
  for (const unsigned char ch : value) {
    if (ch < 0x20 || ch > 0x7e) {
      return false;
    }
  }
  return true;
}

std::string asciiDisplayOrEmpty(const std::string& value) {
  return isAsciiDisplaySafe(value) ? value : "";
}

std::string normalizeEmployeeLabel(const std::string& value) {
  if (startsWith(value, "Employee ")) {
    return "员工 " + value.substr(9);
  }
  return value;
}

std::string connectivityLabel(app::ConnectivityState state) {
  switch (state) {
    case app::ConnectivityState::Connected:
      return "已连接";
    case app::ConnectivityState::Disconnected:
      return "未连接";
    case app::ConnectivityState::Unknown:
    default:
      return "未知";
  }
}

std::string activationLabel(const app::RuntimeStatus& status) {
  if (status.activationInFlight) {
    return "激活：处理中";
  }

  switch (status.activationState) {
    case core::DeviceActivationState::Activated:
      return "激活：已激活";
    case core::DeviceActivationState::PendingActivation: {
      if (!status.activationRegistrationId.has_value()) {
        return "激活：待认领";
      }

      std::string code = status.activationRegistrationId.value();
      if (code.size() > 12) {
        code = code.substr(0, 12);
      }
      return "激活：待认领 (" + code + ")";
    }
    case core::DeviceActivationState::Unconfigured:
    default:
      return "激活：待配置";
  }
}

std::string syncLabel(const app::RuntimeStatus& status) {
  if (!status.credentials.configured()) {
    return "未启用";
  }
  if (status.syncInFlight) {
    return "同步中";
  }
  if (status.snapshots.lastSyncAt == 0) {
    return "等待中";
  }

  std::ostringstream oss;
  oss << "已就绪 @" << status.snapshots.lastSyncAt;
  return oss.str();
}

std::string activeEnrollmentSubject(const app::RuntimeStatus& status) {
  if (status.activeEnrollmentTaskId.has_value()) {
    for (const auto& task : status.snapshots.enrollmentTasks) {
      if (task.taskId != status.activeEnrollmentTaskId.value()) {
        continue;
      }
      if (!task.employeeCode.empty()) {
        return "员工 " + task.employeeCode;
      }
      if (!task.employeeId.empty()) {
        return "员工 " + task.employeeId;
      }
      break;
    }
  }

  if (status.activeEnrollmentEmployeeName.has_value()) {
    const std::string normalized = normalizeEmployeeLabel(status.activeEnrollmentEmployeeName.value());
    if (startsWith(normalized, "员工 ") &&
        isAsciiDisplaySafe(std::string_view(normalized).substr(std::string("员工 ").size()))) {
      return normalized;
    }
  }

  return status.activeEnrollmentTaskId.value_or("待处理");
}

std::string enrollmentProgressSuffix(const app::RuntimeStatus& status) {
  if (status.enrollmentRequiredSamples == 0) {
    return "";
  }

  std::ostringstream oss;
  oss << " (" << status.enrollmentCapturedSamples << "/" << status.enrollmentRequiredSamples << ")";
  return oss.str();
}

std::string subtitleLabel(const app::RuntimeStatus& status) {
  if (status.credentials.configured()) {
    return "设备";
  }
  return "SZPI ESP32-S3";
}

std::string credentialsLabel(const app::RuntimeStatus& status) {
  if (status.credentials.configured()) {
    const std::string deviceCode = asciiDisplayOrEmpty(status.credentials.deviceCode);
    if (!deviceCode.empty()) {
      return deviceCode;
    }
    return "已配置";
  }
  if (status.bootstrapConfigured) {
    return "仅引导配置";
  }
  return "缺失";
}

std::string translateEnrollmentFailureReason(const std::string& reason) {
  if (reason == "ENROLLMENT_CANCELLED") {
    return "已取消";
  }
  if (reason == "ENROLLMENT_FAILED") {
    return "录脸失败";
  }
  if (reason == "ENROLLMENT_REPORT_PENDING") {
    return "结果待上报";
  }
  if (reason == "ENROLLMENT_BUSY") {
    return "录脸进行中";
  }
  if (reason == "TEMPLATE_STORE_UNAVAILABLE") {
    return "SD 卡不可用";
  }
  if (reason == "FILESYSTEM_UNAVAILABLE") {
    return "LittleFS 不可用";
  }
  if (reason == "ENROLLMENT_UNAVAILABLE") {
    return "录脸服务不可用";
  }
  if (reason == "ENROLLMENT_TASK_INVALID") {
    return "任务无效";
  }
  if (reason == "ENROLLMENT_START_FAILED") {
    return "启动失败";
  }
  if (reason == "TEMPLATE_STORE_WRITE_FAILED") {
    return "模板写入失败";
  }
  if (reason == "UNSUPPORTED_PIXEL_FORMAT") {
    return "像素格式不支持";
  }
  if (reason == "ENROLLMENT_TIMEOUT") {
    return "录脸超时";
  }
  if (reason == "ENROLLMENT_MODEL_INIT_FAILED") {
    return "录脸模型初始化失败";
  }
  if (reason == "ENROLLMENT_FEATURE_EXTRACTION_FAILED") {
    return "特征提取失败";
  }
  if (reason == "ENROLLMENT_TEMPLATE_READ_FAILED") {
    return "模板读取失败";
  }
  return reason;
}

std::string translateDisplayErrorCode(const std::string& code) {
  if (code == "DEVICE_AUTH_FAILED") {
    return "设备鉴权失败";
  }
  if (code == "DEVICE_DISABLED") {
    return "设备已禁用";
  }
  if (code == "INVALID_REQUEST") {
    return "请求无效";
  }
  if (code == "INTERNAL_ERROR") {
    return "服务异常";
  }
  if (code == "ATTENDANCE_CONFIG_MISSING") {
    return "考勤配置缺失";
  }
  if (code == "ATTENDANCE_NOT_IN_WINDOW") {
    return "非考勤时段";
  }
  if (code == "ATTENDANCE_DUPLICATE_LATER_OR_EQUAL") {
    return "重复打卡";
  }
  if (code == "EMPLOYEE_NOT_FOUND") {
    return "员工不存在";
  }
  if (code == "FACE_NOT_RECOGNIZED") {
    return "未匹配到员工";
  }
  if (code == "TASK_CANCELLED") {
    return "任务已取消";
  }
  if (code == "ENROLLMENT_TASK_NOT_FOUND") {
    return "录脸任务不存在";
  }
  if (code == "ENROLLMENT_TASK_MISMATCH") {
    return "录脸任务不匹配";
  }

  const std::string localized = translateEnrollmentFailureReason(code);
  return localized == code ? "未知错误" : localized;
}

std::string localizedEnrollmentDetail(const app::RuntimeStatus& status) {
  const std::string detail = status.enrollmentStatusDetail.value_or("");
  if (detail.empty()) {
    return "";
  }

  if (detail == "No face detected" || detail == "未检测到人脸") {
    return "请靠近，只保留一张人脸";
  }
  if (detail == "Multiple faces detected" || detail == "检测到多张人脸") {
    return "请只保留一张人脸";
  }
  if (detail == "Hold still for enrollment" || detail == "请保持不动") {
    if (status.enrollmentRequiredSamples > 0 && status.enrollmentCapturedSamples < status.enrollmentRequiredSamples) {
      return "正在采样 " + std::to_string(status.enrollmentCapturedSamples) + "/" +
          std::to_string(status.enrollmentRequiredSamples) + "，请保持不动";
    }
    return "请保持不动";
  }
  if (
      detail == "Waiting for single face" || detail == "Look at the camera" || detail == "请看向摄像头" ||
      detail == "看向摄像头") {
    return "请看向摄像头";
  }
  if (detail == "Pending enrollment report must flush first" || detail == "需先上报上一条录脸结果") {
    return "需先上报上一条录脸结果";
  }
  if (detail == "Enrollment already in progress" || detail == "录脸会话已在进行中") {
    return "录脸会话已在进行中";
  }
  if (detail == "SD card is required for enrollment" || detail == "录脸需要 SD 卡") {
    return "录脸需要 SD 卡";
  }
  if (detail == "LittleFS unavailable" || detail == "LittleFS 不可用") {
    return "LittleFS 不可用";
  }
  if (detail == "Camera or enrollment service unavailable" || detail == "摄像头或录脸服务不可用") {
    return "摄像头或录脸服务不可用";
  }
  if (detail == "Task missing or not pending" || detail == "任务不存在或不是待处理") {
    return "任务不存在或不是待处理";
  }
  if (detail == "Failed to start enrollment session" || detail == "无法启动录脸会话") {
    return "无法启动录脸会话";
  }
  if (detail == "Enrollment cancelled" || detail == "Cancelled" || detail == "已取消") {
    return "已取消";
  }
  if (detail == "Reporting enrollment cancellation" || detail == "正在上报取消结果") {
    return "正在上报取消结果";
  }
  if (detail == "Reporting enrollment failure" || detail == "正在上报录脸失败") {
    return "正在上报录脸失败";
  }
  if (detail == "Saving template to SD" || detail == "正在保存模板到 SD 卡") {
    return "正在保存模板到 SD 卡";
  }
  if (detail == "Template save failed; reporting failure" || detail == "模板保存失败，正在上报失败") {
    return "模板保存失败，正在上报失败";
  }
  if (detail == "Reporting enrollment success" || detail == "正在上报录脸成功") {
    return "正在上报录脸成功";
  }
  if (detail == "Saving to SD..." || detail == "正在保存到 SD 卡...") {
    return "正在保存到 SD 卡...";
  }
  if (detail == "Reporting..." || detail == "正在上报...") {
    return "正在上报...";
  }
  if (detail == "Done." || detail == "Applied" || detail == "已完成") {
    return "已完成";
  }
  if (detail == "Failed" || detail == "失败") {
    return "失败";
  }
  if (startsWith(detail, "Failed: ")) {
    return "失败：" + translateEnrollmentFailureReason(detail.substr(8));
  }
  if (startsWith(detail, "失败：")) {
    return "失败：" + translateEnrollmentFailureReason(detail.substr(std::string("失败：").size()));
  }

  return normalizeEmployeeLabel(detail);
}

std::string friendlyEnrollmentDetail(const app::RuntimeStatus& status) {
  const std::string detail = localizedEnrollmentDetail(status);
  if (!detail.empty()) {
    return detail;
  }

  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
      return "请将单人脸置中";
    case app::EnrollmentRunState::SavingTemplate:
      return "正在保存到 SD 卡...";
    case app::EnrollmentRunState::Reporting:
      return "正在上报...";
    case app::EnrollmentRunState::Done:
      return "已完成";
    case app::EnrollmentRunState::Failed:
      if (status.enrollmentFailureReason.has_value()) {
        return "失败：" + translateEnrollmentFailureReason(status.enrollmentFailureReason.value());
      }
      return "失败";
    case app::EnrollmentRunState::Cancelled:
      return "已取消";
    case app::EnrollmentRunState::Idle:
    default:
      return "";
  }
}

bool capturePageActive(const app::RuntimeStatus& status) {
  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
    case app::EnrollmentRunState::SavingTemplate:
    case app::EnrollmentRunState::Reporting:
    case app::EnrollmentRunState::Done:
    case app::EnrollmentRunState::Failed:
    case app::EnrollmentRunState::Cancelled:
      return true;
    case app::EnrollmentRunState::Idle:
    default:
      return false;
  }
}

bool captureRunning(const app::RuntimeStatus& status) {
  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
    case app::EnrollmentRunState::SavingTemplate:
      return true;
    case app::EnrollmentRunState::Reporting:
    case app::EnrollmentRunState::Idle:
    case app::EnrollmentRunState::Done:
    case app::EnrollmentRunState::Failed:
    case app::EnrollmentRunState::Cancelled:
    default:
      return false;
  }
}

std::string captureTitle(const app::RuntimeStatus& status) {
  const std::string subject = activeEnrollmentSubject(status);

  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
      return "采集：" + subject;
    case app::EnrollmentRunState::SavingTemplate:
      return "保存中";
    case app::EnrollmentRunState::Reporting:
      return "上报中";
    case app::EnrollmentRunState::Done:
      return "已完成";
    case app::EnrollmentRunState::Failed:
      return "失败";
    case app::EnrollmentRunState::Cancelled:
      return "已取消";
    default:
      return subject.empty() ? "录脸采集" : subject;
  }
}

std::string captureProgress(const app::RuntimeStatus& status) {
  if (status.enrollmentRequiredSamples > 0) {
    return "采样 " + std::to_string(status.enrollmentCapturedSamples) + "/" +
        std::to_string(status.enrollmentRequiredSamples) + "  人脸 " +
        std::to_string(status.detectedFaceCount);
  }

  if (status.detectedFaceCount > 0) {
    return std::to_string(status.detectedFaceCount) + " 张人脸";
  }

  return "";
}

uint8_t captureProgressPercent(const app::RuntimeStatus& status) {
  if (status.enrollmentRequiredSamples == 0) {
    return 0;
  }

  const auto cappedCaptured = std::min(status.enrollmentCapturedSamples, status.enrollmentRequiredSamples);
  return static_cast<uint8_t>((cappedCaptured * 100U) / status.enrollmentRequiredSamples);
}

std::string captureActionLabel(const app::RuntimeStatus& status) {
  return captureRunning(status) ? "取消" : "返回";
}

std::string translateTaskStatus(std::string_view status) {
  if (status == "pending") {
    return "待处理";
  }
  if (status == "success") {
    return "成功";
  }
  if (status == "failed") {
    return "失败";
  }
  if (status == "cancelled") {
    return "已取消";
  }
  return std::string(status);
}

std::string enrollmentTaskLabel(const core::EnrollmentTaskSnapshot& task) {
  if (!task.employeeCode.empty()) {
    return "员工 " + task.employeeCode;
  }
  if (!task.employeeId.empty()) {
    return "员工 " + task.employeeId;
  }
  return "员工";
}

std::string taskLabel(const app::RuntimeStatus& status) {
  if (status.enrollmentState == app::EnrollmentRunState::Capturing ||
      status.enrollmentState == app::EnrollmentRunState::Preparing) {
    std::ostringstream oss;
    oss << "任务：录脸中 " << activeEnrollmentSubject(status);
    if (status.enrollmentRequiredSamples > 0) {
      oss << " (" << status.enrollmentCapturedSamples << "/" << status.enrollmentRequiredSamples << ")";
    }
    return oss.str();
  }
  if (status.enrollmentState == app::EnrollmentRunState::Reporting) {
    return "任务：正在上报录脸结果";
  }
  if (status.enrollmentState == app::EnrollmentRunState::Failed) {
    return "任务：录脸失败";
  }
  if (status.enrollmentState == app::EnrollmentRunState::Done) {
    return "任务：录脸完成";
  }
  if (status.enrollmentState == app::EnrollmentRunState::Cancelled) {
    return "任务：录脸已取消";
  }

  if (!status.credentials.configured()) {
    return "任务：不可用";
  }
  if (status.snapshots.enrollmentTasks.empty()) {
    return "任务：无";
  }

  const auto& task = status.snapshots.enrollmentTasks.front();
  const std::size_t remaining = status.snapshots.enrollmentTasks.size() - 1;
  std::ostringstream oss;
  oss << "任务：" << enrollmentTaskLabel(task);
  if (remaining > 0) {
    oss << " + " << remaining << " 个";
  }
  return oss.str();
}

std::string storageLabel(const app::RuntimeStatus& status) {
  if (!status.credentialsReady) {
    return "凭据不可用";
  }
  if (!status.filesystemReady) {
    return "LittleFS 不可用";
  }
  if (infra::templateStoreDisabled(status.templateStoreStatusCode)) {
    return "SD 已禁用";
  }
  if (infra::templateStoreManifestBroken(status.templateStoreStatusCode)) {
    return "SD 清单无效";
  }
  if (status.templateStoreStatusCode == infra::kTemplateStoreIoError) {
    return "SD 读写错误";
  }
  if (status.templateStoreStatusCode == infra::kTemplateStoreMountFailed) {
    return "SD 挂载失败";
  }
  if (status.templateStoreStatusCode == infra::kTemplateStoreCardMissing) {
    return "未检测到 SD 卡";
  }
  if (infra::templateStoreMounted(status.templateStoreStatusCode)) {
    std::ostringstream oss;
    oss << "SD 已就绪 (模板=" << status.templateCount << ")";
    return oss.str();
  }
  if (!status.templateStoreReady) {
    return status.templateStoreStatusCode.empty() ? "SD 未就绪" : "SD " + status.templateStoreStatusCode;
  }
  return "已就绪";
}

std::string apiLabel(const app::RuntimeStatus& status) {
  if (!status.apiConfigured) {
    return "缺少地址";
  }
  if (status.apiProbeInFlight) {
    return "探测中";
  }
  if (status.connectivity != app::ConnectivityState::Connected) {
    return "等待 Wi-Fi";
  }
  if (status.apiProbeSucceeded) {
    return "可达";
  }
  if (status.apiProbeStatusCode.has_value()) {
    return "失败 (" + status.apiProbeStatusCode.value() + ")";
  }
  return "等待中";
}

uint64_t effectiveStatusTime(const app::RuntimeStatus& status) {
  if (status.currentWallClockTimeMs.has_value()) {
    return status.currentWallClockTimeMs.value();
  }

  return static_cast<uint64_t>(std::time(nullptr)) * 1000ULL;
}

std::string periodLabel(const app::RuntimeStatus& status) {
  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
      return "录脸：" + activeEnrollmentSubject(status) + enrollmentProgressSuffix(status);
    case app::EnrollmentRunState::SavingTemplate:
      return "录脸：保存中";
    case app::EnrollmentRunState::Reporting:
      return "录脸：上报中";
    case app::EnrollmentRunState::Done:
      return "录脸：已完成";
    case app::EnrollmentRunState::Failed:
      return "录脸：失败";
    case app::EnrollmentRunState::Cancelled:
      return "录脸：已取消";
    case app::EnrollmentRunState::Idle:
    default:
      break;
  }

  if (!status.snapshots.attendanceConfig.has_value()) {
    return "时段：未配置";
  }

  const auto attendanceType =
      core::classifyAttendanceType(status.snapshots.attendanceConfig.value(), effectiveStatusTime(status));
  if (!attendanceType.has_value()) {
    return "时段：非考勤时段";
  }

  return *attendanceType == core::AttendanceRecordType::ClockIn ? "时段：上班" : "时段：下班";
}

std::string attendanceResultLabel(const app::RuntimeStatus& status) {
  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
      return "录脸 " + activeEnrollmentSubject(status) + enrollmentProgressSuffix(status);
    case app::EnrollmentRunState::SavingTemplate:
      return "保存中...";
    case app::EnrollmentRunState::Reporting:
      return "上报中...";
    case app::EnrollmentRunState::Done:
      return "已完成";
    case app::EnrollmentRunState::Failed:
      return "失败";
    case app::EnrollmentRunState::Cancelled:
      return "已取消";
    case app::EnrollmentRunState::Idle:
    default:
      break;
  }

  if (status.pendingQueueSize > 0) {
    return std::to_string(status.pendingQueueSize) + " 条待上传";
  }

  if (status.lastAttendanceFeedback.has_value()) {
    return normalizeEmployeeLabel(status.lastAttendanceFeedback.value());
  }

  if (status.lastErrorCode.has_value()) {
    return "错误：" + translateDisplayErrorCode(status.lastErrorCode.value());
  }

  return "";
}

std::vector<EnrollmentTaskItemViewModel> enrollmentTasks(const app::RuntimeStatus& status) {
  std::vector<EnrollmentTaskItemViewModel> items;
  items.reserve(status.snapshots.enrollmentTasks.size());

  for (const auto& task : status.snapshots.enrollmentTasks) {
    items.push_back(EnrollmentTaskItemViewModel{
        .taskId = task.taskId,
        .title = enrollmentTaskLabel(task),
        .meta = "状态：" + translateTaskStatus(task.status),
    });
  }

  return items;
}

std::string enrollmentTaskSummary(const app::RuntimeStatus& status) {
  if (status.enrollmentPendingCount > 0) {
    return "结果待上报";
  }
  if (status.snapshots.enrollmentTasks.empty()) {
    return "暂无任务";
  }

  return std::to_string(status.snapshots.enrollmentTasks.size()) + " 个任务";
}

std::string cameraLabel(const app::RuntimeStatus& status) {
  const std::string enrollmentDetail = friendlyEnrollmentDetail(status);
  if (!enrollmentDetail.empty()) {
    return enrollmentDetail;
  }

  if (!status.cameraAvailable) {
    return "摄像头不可用";
  }

  if (!status.cameraReady) {
    if (!status.cameraLastError.empty()) {
      const std::string cameraError = asciiDisplayOrEmpty(status.cameraLastError);
      if (!cameraError.empty()) {
        return "摄像头初始化失败\n" + cameraError;
      }
      return "摄像头初始化失败";
    }
    return "摄像头已连接\n等待初始化";
  }

  const std::string sensorModel = asciiDisplayOrEmpty(status.cameraSensorModel);
  std::ostringstream oss;
  oss << (sensorModel.empty() ? "摄像头已就绪" : sensorModel + " 已就绪");
  if (status.cameraLastFrame.bytes > 0) {
    oss << "\n" << status.cameraLastFrame.width << "x" << status.cameraLastFrame.height << " "
        << face::cameraPixelFormatName(status.cameraLastFrame.pixelFormat) << " #" << status.cameraCaptureCount;
  } else {
    oss << "\n等待首帧";
  }
  return oss.str();
}

}  // namespace

AppViewModel StatusScreenPresenter::build(const app::RuntimeStatus& status) {
  AppViewModel view = {};
  view.title = "Hitomi";
  view.subtitle = subtitleLabel(status);
  view.periodLine = periodLabel(status);
  view.cameraHintLine = cameraLabel(status);
  view.attendanceResultLine = attendanceResultLabel(status);
  view.captureActive = capturePageActive(status);
  view.captureRunning = captureRunning(status);
  view.captureTitleLine = captureTitle(status);
  view.captureStatusLine = friendlyEnrollmentDetail(status);
  view.captureProgressLine = captureProgress(status);
  view.captureProgressPercent = captureProgressPercent(status);
  view.captureRequiredSamples =
      static_cast<uint8_t>(std::min<uint32_t>(status.enrollmentRequiredSamples, 255U));
  view.captureCapturedSamples =
      static_cast<uint8_t>(std::min<uint32_t>(status.enrollmentCapturedSamples, 255U));
  view.captureDetectedFaceCount =
      static_cast<uint8_t>(std::min<uint32_t>(status.detectedFaceCount, 255U));
  view.captureActionLabel = captureActionLabel(status);
  view.enrollmentTasks = enrollmentTasks(status);
  view.enrollmentTaskSummaryLine = enrollmentTaskSummary(status);
  view.credentialsLine = credentialsLabel(status);
  view.storageLine = storageLabel(status);
  view.wifiLine = connectivityLabel(status.connectivity);
  if (status.activeWifiSsid.has_value()) {
    const std::string ssid = asciiDisplayOrEmpty(status.activeWifiSsid.value());
    if (!ssid.empty()) {
      view.wifiLine += " (" + ssid + ")";
    }
  }
  view.activationLine = activationLabel(status);
  view.apiLine = apiLabel(status);
  view.faceDetectLine = app::formatFaceDetectLine(status, app::FaceLineStyle::Presenter);
  view.syncLine = "同步：" + syncLabel(status);
  view.taskLine = taskLabel(status);
  view.queueLine = "队列：" + std::to_string(status.pendingQueueSize);
  view.pendingQueueCount = static_cast<uint16_t>(std::min<uint32_t>(status.pendingQueueSize, 65535U));
  view.errorLine = status.lastErrorCode.has_value() ? translateDisplayErrorCode(status.lastErrorCode.value()) : "无";
  view.faceLine = app::formatFaceEngineLine(status, app::FaceLineStyle::Presenter);
  view.footer = status.firmwareTag;
  return view;
}

}  // namespace ui
