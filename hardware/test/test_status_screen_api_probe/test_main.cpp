#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include "app/runtime_status.hpp"
#include "infra/template_store_port.hpp"
#include "ui/status_screen_presenter.hpp"

namespace {

void expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testStatusScreenShowsApiProbeState() {
  app::RuntimeStatus status = {};
  status.connectivity = app::ConnectivityState::Connected;
  status.apiConfigured = true;
  status.apiProbeSucceeded = false;
  status.apiProbeStatusCode = std::optional<std::string>("HTTP_404");

  ui::AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(view.apiLine.find("HTTP_404") != std::string::npos, "api line should surface probe failure code");
}

void testStatusScreenShowsFaceDetectState() {
  app::RuntimeStatus status = {};
  status.faceEngineReady = true;
  status.faceEngineStatusDetail = std::optional<std::string>("Linked (model load deferred)");
  status.faceDetectReady = true;
  status.faceDetected = true;
  status.detectedFaceCount = 1;
  status.faceTopScore = 0.93f;

  ui::AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(
      view.faceLine.find("已连接（模型延后加载）") != std::string::npos,
      "face line should surface engine state detail");
  expect(
      view.faceDetectLine.find("1 张人脸") != std::string::npos &&
          view.faceDetectLine.find("@0.93") != std::string::npos,
      "face detect line should surface detection count and score");
}

void testStatusScreenShowsEnrollmentGuidance() {
  app::RuntimeStatus status = {};
  status.enrollmentState = app::EnrollmentRunState::Capturing;
  status.activeEnrollmentEmployeeName = std::optional<std::string>("Employee 20230001");
  status.enrollmentCapturedSamples = 1;
  status.enrollmentRequiredSamples = 3;
  status.detectedFaceCount = 1;
  status.enrollmentStatusDetail = std::optional<std::string>("Hold still for enrollment");

  ui::AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(view.periodLine.find("录脸：员工 20230001 (1/3)") != std::string::npos, "period line should show enrollment progress");
  expect(
      view.cameraHintLine.find("正在采样 1/3，请保持不动") != std::string::npos,
      "camera hint should show operator-facing enrollment guidance");
  expect(
      view.attendanceResultLine.find("录脸 员工 20230001 (1/3)") != std::string::npos,
      "status card should show active enrollment progress");
  expect(view.captureActive, "capture page should activate during enrollment");
  expect(
      view.captureTitleLine.find("采集：员工 20230001") != std::string::npos,
      "capture page title should show active employee");
  expect(
      view.captureProgressLine.find("1/3") != std::string::npos,
      "capture page should expose sample progress");
  expect(view.captureRequiredSamples == 3, "capture page should expose required sample count");
  expect(view.captureCapturedSamples == 1, "capture page should expose captured sample count");
  expect(view.captureDetectedFaceCount == 1, "capture page should expose detected face count");
  expect(view.captureActionLabel == "取消", "active capture should offer cancel action");
}

void testStatusScreenShowsEnrollmentFailureState() {
  app::RuntimeStatus status = {};
  status.enrollmentState = app::EnrollmentRunState::Failed;
  status.enrollmentFailureReason = std::optional<std::string>("ENROLLMENT_FAILED");
  status.enrollmentStatusDetail = std::optional<std::string>("Failed");

  ui::AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(view.periodLine == "录脸：失败", "period line should show failed enrollment state");
  expect(view.attendanceResultLine == "失败", "status card should show failed enrollment state");
  expect(view.captureActive, "capture page should stay visible for terminal failure confirmation");
  expect(view.captureActionLabel == "返回", "terminal failure should offer a return action");
}

void testStatusScreenLocalizesEnrollmentFailureReason() {
  app::RuntimeStatus status = {};
  status.enrollmentState = app::EnrollmentRunState::Failed;
  status.enrollmentFailureReason = std::optional<std::string>("ENROLLMENT_TIMEOUT");
  status.lastErrorCode = std::optional<std::string>("ENROLLMENT_TIMEOUT");

  ui::AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(
      view.captureStatusLine.find("录脸超时") != std::string::npos,
      "capture status should localize common enrollment failure reasons");
  expect(
      view.errorLine.find("录脸超时") != std::string::npos,
      "error line should localize common enrollment failure reasons");
}

void testStatusScreenShowsEnrollmentCancelledState() {
  app::RuntimeStatus status = {};
  status.enrollmentState = app::EnrollmentRunState::Cancelled;
  status.enrollmentFailureReason = std::optional<std::string>("ENROLLMENT_CANCELLED");
  status.enrollmentStatusDetail = std::optional<std::string>("Cancelled");

  ui::AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(view.periodLine == "录脸：已取消", "period line should show cancelled enrollment state");
  expect(view.attendanceResultLine == "已取消", "status card should show cancelled enrollment state");
  expect(view.captureActive, "capture page should stay visible after cancellation");
  expect(view.captureActionLabel == "返回", "cancelled state should offer a return action");
}

void testStatusScreenPrefersProjectedWallClockTime() {
  app::RuntimeStatus status = {};
  status.currentWallClockTimeMs = 1'774'660'200'000ULL;
  status.snapshots.attendanceConfig = core::AttendanceConfigSnapshot{
      .id = "default",
      .workStartMinute = 540,
      .workEndMinute = 600,
      .offStartMinute = 1080,
      .offEndMinute = 1140,
      .updatedAt = 1,
  };
  status.snapshots.lastServerTime = 1ULL;

  ui::AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(view.periodLine == "时段：上班", "period line should use the projected wall-clock time");
}

}  // namespace

int main() {
  try {
    testStatusScreenShowsApiProbeState();
    testStatusScreenShowsFaceDetectState();
    testStatusScreenShowsEnrollmentGuidance();
    testStatusScreenShowsEnrollmentFailureState();
    testStatusScreenLocalizesEnrollmentFailureReason();
    testStatusScreenShowsEnrollmentCancelledState();
    testStatusScreenPrefersProjectedWallClockTime();
    std::cout << "[PASS] status screen api probe" << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] status screen api probe: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
