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
      view.faceLine.find("Linked (model load deferred)") != std::string::npos,
      "face line should surface engine state detail");
  expect(
      view.faceDetectLine.find("1 face(s)") != std::string::npos &&
          view.faceDetectLine.find("@0.93") != std::string::npos,
      "face detect line should surface detection count and score");
}

void testStatusScreenShowsEnrollmentGuidance() {
  app::RuntimeStatus status = {};
  status.enrollmentState = app::EnrollmentRunState::Capturing;
  status.activeEnrollmentEmployeeName = std::optional<std::string>("Alice");
  status.enrollmentCapturedSamples = 1;
  status.enrollmentRequiredSamples = 3;
  status.enrollmentStatusDetail = std::optional<std::string>("Hold still for enrollment");

  ui::AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(view.periodLine.find("Enroll: Alice (1/3)") != std::string::npos, "period line should show enrollment progress");
  expect(
      view.cameraHintLine.find("Sample 1/3. Hold still.") != std::string::npos,
      "camera hint should show operator-facing enrollment guidance");
  expect(
      view.attendanceResultLine.find("Rec Alice (1/3)") != std::string::npos,
      "status card should show active enrollment progress");
  expect(view.captureActive, "capture page should activate during enrollment");
  expect(
      view.captureTitleLine.find("Capture: Alice") != std::string::npos,
      "capture page title should show active employee");
  expect(
      view.captureProgressLine.find("1/3") != std::string::npos,
      "capture page should expose sample progress");
  expect(view.captureActionLabel == "Cancel", "active capture should offer cancel action");
}

void testStatusScreenShowsEnrollmentFailureState() {
  app::RuntimeStatus status = {};
  status.enrollmentState = app::EnrollmentRunState::Failed;
  status.enrollmentFailureReason = std::optional<std::string>("ENROLLMENT_FAILED");
  status.enrollmentStatusDetail = std::optional<std::string>("Failed");

  ui::AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(view.periodLine == "Enroll: failed", "period line should show failed enrollment state");
  expect(view.attendanceResultLine == "Failed", "status card should show failed enrollment state");
  expect(view.captureActive, "capture page should stay visible for terminal failure confirmation");
  expect(view.captureActionLabel == "Back", "terminal failure should offer a return action");
}

void testStatusScreenShowsEnrollmentCancelledState() {
  app::RuntimeStatus status = {};
  status.enrollmentState = app::EnrollmentRunState::Cancelled;
  status.enrollmentFailureReason = std::optional<std::string>("ENROLLMENT_CANCELLED");
  status.enrollmentStatusDetail = std::optional<std::string>("Cancelled");

  ui::AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(view.periodLine == "Enroll: cancelled", "period line should show cancelled enrollment state");
  expect(view.attendanceResultLine == "Cancelled", "status card should show cancelled enrollment state");
  expect(view.captureActive, "capture page should stay visible after cancellation");
  expect(view.captureActionLabel == "Back", "cancelled state should offer a return action");
}

}  // namespace

int main() {
  try {
    testStatusScreenShowsApiProbeState();
    testStatusScreenShowsFaceDetectState();
    testStatusScreenShowsEnrollmentGuidance();
    testStatusScreenShowsEnrollmentFailureState();
    testStatusScreenShowsEnrollmentCancelledState();
    std::cout << "[PASS] status screen api probe" << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] status screen api probe: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
