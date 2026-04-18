#include "app/runtime_enrollment_ops.hpp"

#include <Arduino.h>

#include <optional>

#include "app/runtime_storage_ops.hpp"
#include "board/app_config.hpp"
#include "core/use_cases.hpp"
#include "infra/display_port.hpp"

namespace app {
namespace {

const core::EnrollmentTaskSnapshot* findEnrollmentTask(
    const std::vector<core::EnrollmentTaskSnapshot>& tasks,
    const std::string& taskId) {
  for (const auto& task : tasks) {
    if (task.taskId == taskId) {
      return &task;
    }
  }
  return nullptr;
}

bool hasBlockingPendingEnrollmentReport(const RuntimeState& state, const std::string& taskId) {
  for (const auto& report : state.pendingEnrollmentReports) {
    if (report.taskId != taskId) {
      return true;
    }
  }
  return false;
}

void updateProgressSnapshot(RuntimeState& state, const face::EnrollmentProgress& progress) {
  state.enrollmentCapturedSamples = progress.capturedSamples;
  state.enrollmentRequiredSamples = progress.requiredSamples;
  state.detectedFaceCount = progress.detectedFaceCount;
  state.enrollmentStatusDetail = progress.detail.empty() ? std::nullopt
                                                         : std::optional<std::string>(progress.detail);
  if (progress.active) {
    state.enrollmentState = EnrollmentRunState::Capturing;
  }
}

void enqueueEnrollmentReport(
    const RuntimeContext& context,
    RuntimeState& state,
    const face::EnrollmentResult& result,
    std::optional<std::string> failureReasonOverride) {
  const std::string finalResult =
      failureReasonOverride.has_value() || !result.success ? "failed" : "success";
  Serial.printf(
      "[APP] enqueue enrollment report task=%s employee=%s result=%s reason=%s finishedAt=%llu\n",
      result.taskId.c_str(),
      result.employeeId.c_str(),
      finalResult.c_str(),
      failureReasonOverride.has_value()
          ? failureReasonOverride->c_str()
          : (result.failureReason.has_value() ? result.failureReason->c_str() : "-"),
      static_cast<unsigned long long>(result.finishedAt));
  core::upsertPendingEnrollmentReport(
      state.pendingEnrollmentReports,
      core::PendingEnrollmentReport{
          .taskId = result.taskId,
          .employeeId = result.employeeId,
          .result = failureReasonOverride.has_value() || !result.success ? "failed" : "success",
          .finishedAt = result.finishedAt,
          .failureReason = failureReasonOverride.has_value() ? failureReasonOverride : result.failureReason,
          .lastAttemptAt = std::nullopt,
          .lastResultCode = std::nullopt,
      });
  persistPendingEnrollmentReports(context, state);
}

void notify(
    const RuntimeContext& context,
    infra::DisplayNotificationLevel level,
    const std::string& text,
    uint32_t durationMs) {
  context.display.showNotification(infra::DisplayNotification{
      .level = level,
      .text = text,
      .durationMs = durationMs,
  });
}

std::string enrollmentDisplayLabel(const core::EnrollmentTaskSnapshot& task) {
  if (!task.employeeCode.empty()) {
    return "员工 " + task.employeeCode;
  }
  if (!task.employeeId.empty()) {
    return "员工 " + task.employeeId;
  }
  return "员工";
}

}  // namespace

void startEnrollmentTask(
    const RuntimeContext& context,
    RuntimeState& state,
    const std::string& taskId,
    uint32_t nowMs) {
  static_cast<void>(nowMs);

  if (hasBlockingPendingEnrollmentReport(state, taskId)) {
    Serial.printf(
        "[APP] enrollment blocked: pending report for different task current=%s pendingCount=%u\n",
        taskId.c_str(),
        static_cast<unsigned>(state.pendingEnrollmentReports.size()));
    state.enrollmentState = EnrollmentRunState::Reporting;
    state.enrollmentStatusDetail = "需先上报上一条录脸结果";
    state.enrollmentFailureReason = "ENROLLMENT_REPORT_PENDING";
    state.lastErrorCode = "ENROLLMENT_REPORT_PENDING";
    notify(context, infra::DisplayNotificationLevel::Warning, "请先完成结果上报", 2200);
    state.renderDirty = true;
    return;
  }
  if (context.enrollmentService.active()) {
    Serial.println("[APP] enrollment blocked: already in progress");
    state.enrollmentStatusDetail = "录脸会话已在进行中";
    state.enrollmentFailureReason = "ENROLLMENT_BUSY";
    state.lastErrorCode = "ENROLLMENT_BUSY";
    notify(context, infra::DisplayNotificationLevel::Warning, "录脸进行中", 1800);
    state.renderDirty = true;
    return;
  }
  if (!state.templateStoreReady) {
    Serial.println("[APP] enrollment blocked: template store unavailable");
    state.enrollmentState = EnrollmentRunState::Failed;
    state.enrollmentStatusDetail = "录脸需要 SD 卡";
    state.enrollmentFailureReason = "TEMPLATE_STORE_UNAVAILABLE";
    state.lastErrorCode = "TEMPLATE_STORE_UNAVAILABLE";
    notify(context, infra::DisplayNotificationLevel::Error, "需要 SD 卡", 2200);
    state.renderDirty = true;
    return;
  }
  if (!state.filesystemReady) {
    Serial.println("[APP] enrollment blocked: LittleFS unavailable");
    state.enrollmentState = EnrollmentRunState::Failed;
    state.enrollmentStatusDetail = "LittleFS 不可用";
    state.enrollmentFailureReason = "FILESYSTEM_UNAVAILABLE";
    state.lastErrorCode = "FILESYSTEM_UNAVAILABLE";
    notify(context, infra::DisplayNotificationLevel::Error, "LittleFS 不可用", 1800);
    state.renderDirty = true;
    return;
  }
  if (!context.camera.available() || !context.camera.ready() || !context.enrollmentService.available()) {
    Serial.printf(
        "[APP] enrollment blocked: cameraAvailable=%d cameraReady=%d serviceAvailable=%d\n",
        context.camera.available() ? 1 : 0,
        context.camera.ready() ? 1 : 0,
        context.enrollmentService.available() ? 1 : 0);
    state.enrollmentState = EnrollmentRunState::Failed;
    state.enrollmentStatusDetail = "摄像头或录脸服务不可用";
    state.enrollmentFailureReason = "ENROLLMENT_UNAVAILABLE";
    state.lastErrorCode = "ENROLLMENT_UNAVAILABLE";
    notify(context, infra::DisplayNotificationLevel::Error, "摄像头不可用", 2200);
    state.renderDirty = true;
    return;
  }

  const auto* task = findEnrollmentTask(state.snapshots.enrollmentTasks, taskId);
  if (task == nullptr || task->status != "pending") {
    Serial.printf("[APP] enrollment blocked: task invalid taskId=%s\n", taskId.c_str());
    state.enrollmentState = EnrollmentRunState::Failed;
    state.enrollmentStatusDetail = "任务不存在或不是待处理";
    state.enrollmentFailureReason = "ENROLLMENT_TASK_INVALID";
    state.lastErrorCode = "ENROLLMENT_TASK_INVALID";
    notify(context, infra::DisplayNotificationLevel::Warning, "任务不是待处理状态", 2000);
    state.renderDirty = true;
    return;
  }

  if (!context.enrollmentService.start(face::EnrollmentRequest{
          .taskId = task->taskId,
          .employeeId = task->employeeId,
          .employeeName = task->employeeName,
      })) {
    Serial.printf("[APP] enrollment start failed taskId=%s employeeId=%s\n", task->taskId.c_str(), task->employeeId.c_str());
    state.enrollmentState = EnrollmentRunState::Failed;
    state.enrollmentStatusDetail = "无法启动录脸会话";
    state.enrollmentFailureReason = "ENROLLMENT_START_FAILED";
    state.lastErrorCode = "ENROLLMENT_START_FAILED";
    notify(context, infra::DisplayNotificationLevel::Error, "启动失败", 2000);
    state.renderDirty = true;
    return;
  }

  state.activeEnrollmentTaskId = task->taskId;
  state.activeEnrollmentEmployeeId = task->employeeId;
  state.activeEnrollmentEmployeeName = enrollmentDisplayLabel(*task);
  state.enrollmentCapturedSamples = 0;
  state.enrollmentRequiredSamples = board::kEnrollmentRequiredSamples;
  state.detectedFaceCount = 0;
  state.enrollmentFailureReason = std::nullopt;
  state.enrollmentStatusDetail = "请看向摄像头";
  state.enrollmentState = EnrollmentRunState::Preparing;
  setLastError(state, std::nullopt);
  if (!state.pendingEnrollmentReports.empty()) {
    Serial.printf(
        "[APP] enrollment retry allowed taskId=%s pendingCount=%u\n",
        task->taskId.c_str(),
        static_cast<unsigned>(state.pendingEnrollmentReports.size()));
  }
  notify(
      context,
      infra::DisplayNotificationLevel::Info,
      "开始录脸：" + enrollmentDisplayLabel(*task),
      1800);
  state.renderDirty = true;
  Serial.printf("[APP] Enrollment session started taskId=%s employeeId=%s\n", task->taskId.c_str(), task->employeeId.c_str());
}

void cancelEnrollmentTask(
    const RuntimeContext& context,
    RuntimeState& state,
    uint32_t nowMs) {
  static_cast<void>(nowMs);

  if (!context.enrollmentService.active()) {
    notify(context, infra::DisplayNotificationLevel::Warning, "当前没有可取消的任务", 1600);
    return;
  }

  context.enrollmentService.cancel();
  const auto result = context.enrollmentService.takeResult();
  if (!result.has_value()) {
    state.enrollmentState = EnrollmentRunState::Cancelled;
    state.enrollmentStatusDetail = "已取消";
    state.enrollmentFailureReason = "ENROLLMENT_CANCELLED";
    state.lastErrorCode = "ENROLLMENT_CANCELLED";
    notify(context, infra::DisplayNotificationLevel::Warning, "已取消", 1600);
    state.renderDirty = true;
    return;
  }

  state.enrollmentFailureReason = "ENROLLMENT_CANCELLED";
  state.enrollmentState = EnrollmentRunState::Reporting;
  state.enrollmentStatusDetail = "正在上报取消结果";
  enqueueEnrollmentReport(context, state, result.value(), std::optional<std::string>("ENROLLMENT_CANCELLED"));
  state.lastErrorCode = "ENROLLMENT_CANCELLED";
  notify(context, infra::DisplayNotificationLevel::Warning, "已取消，正在上报", 1800);
  state.renderDirty = true;
}

void dismissCaptureSummary(RuntimeState& state) {
  if (
      state.enrollmentState != EnrollmentRunState::Reporting &&
      state.enrollmentState != EnrollmentRunState::Done &&
      state.enrollmentState != EnrollmentRunState::Failed &&
      state.enrollmentState != EnrollmentRunState::Cancelled) {
    return;
  }

  state.enrollmentState = EnrollmentRunState::Idle;
  state.activeEnrollmentTaskId.reset();
  state.activeEnrollmentEmployeeId.reset();
  state.activeEnrollmentEmployeeName.reset();
  state.enrollmentCapturedSamples = 0;
  state.enrollmentRequiredSamples = 0;
  state.detectedFaceCount = 0;
  state.enrollmentFailureReason.reset();
  state.enrollmentStatusDetail.reset();
  state.renderDirty = true;
}

void processEnrollmentFrame(
    const RuntimeContext& context,
    RuntimeState& state,
    const face::CameraFrameInfo& frameInfo,
    const uint8_t* frameData,
    uint32_t nowMs) {
  if (!context.enrollmentService.active()) {
    return;
  }

  context.enrollmentService.processFrame(frameInfo, frameData, nowMs);
  updateProgressSnapshot(state, context.enrollmentService.progress());

  auto result = context.enrollmentService.takeResult();
  if (!result.has_value()) {
    state.renderDirty = true;
    return;
  }

  state.enrollmentFailureReason = result->failureReason;
  if (!result->success) {
    Serial.printf(
        "[APP] enrollment local failure task=%s employee=%s reason=%s\n",
        result->taskId.c_str(),
        result->employeeId.c_str(),
        result->failureReason.has_value() ? result->failureReason->c_str() : "UNKNOWN");
    state.enrollmentState = EnrollmentRunState::Reporting;
    state.enrollmentStatusDetail = "正在上报录脸失败";
    enqueueEnrollmentReport(context, state, result.value(), result->failureReason);
    state.lastErrorCode = result->failureReason;
    notify(
        context,
        infra::DisplayNotificationLevel::Error,
        "录脸失败：" + result->failureReason.value_or("UNKNOWN"),
        3200);
    state.renderDirty = true;
    return;
  }

  state.enrollmentState = EnrollmentRunState::SavingTemplate;
  state.enrollmentStatusDetail = "正在保存模板到 SD 卡";
  Serial.printf(
      "[APP] enrollment local success task=%s employee=%s templateBytes=%u\n",
      result->taskId.c_str(),
      result->employeeId.c_str(),
      static_cast<unsigned>(result->templateBytes.size()));
  if (!context.templateStore.upsertTemplate(result->employeeId, result->templateBytes, result->finishedAt)) {
    const auto templateStatus = context.templateStore.status();
    Serial.printf(
        "[APP] enrollment template save failed employee=%s status=%s detail=%s ready=%d\n",
        result->employeeId.c_str(),
        templateStatus.health.statusCode.c_str(),
        templateStatus.health.detail.c_str(),
        templateStatus.ready ? 1 : 0);
    Serial.printf("[APP] enrollment template save failed employee=%s\n", result->employeeId.c_str());
    applyTemplateStoreStatus(context, state, context.templateStore.status(), false);
    persistStorageAux(context, state);
    state.enrollmentFailureReason = "TEMPLATE_STORE_WRITE_FAILED";
    state.enrollmentStatusDetail = "模板保存失败，正在上报失败";
    enqueueEnrollmentReport(context, state, result.value(), std::optional<std::string>("TEMPLATE_STORE_WRITE_FAILED"));
    state.enrollmentState = EnrollmentRunState::Reporting;
    state.lastErrorCode = "TEMPLATE_STORE_WRITE_FAILED";
    notify(context, infra::DisplayNotificationLevel::Error, "模板保存失败", 2200);
    state.renderDirty = true;
    return;
  }

  applyTemplateStoreStatus(context, state, context.templateStore.status(), true);
  persistStorageAux(context, state);
  Serial.printf("[APP] enrollment template saved employee=%s\n", result->employeeId.c_str());
  state.enrollmentState = EnrollmentRunState::Reporting;
  state.enrollmentStatusDetail = "正在上报录脸成功";
  enqueueEnrollmentReport(context, state, result.value(), std::nullopt);
  setLastError(state, std::nullopt);
  notify(context, infra::DisplayNotificationLevel::Success, "已保存，正在上报", 1800);
  state.renderDirty = true;
}

}  // namespace app
