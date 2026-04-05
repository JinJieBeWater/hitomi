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

}  // namespace

int main() {
  try {
    testStatusScreenShowsApiProbeState();
    std::cout << "[PASS] status screen api probe" << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] status screen api probe: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
