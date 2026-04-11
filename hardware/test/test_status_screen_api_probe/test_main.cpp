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

}  // namespace

int main() {
  try {
    testStatusScreenShowsApiProbeState();
    testStatusScreenShowsFaceDetectState();
    std::cout << "[PASS] status screen api probe" << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] status screen api probe: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
