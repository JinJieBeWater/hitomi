#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "app/api_probe_policy.hpp"

namespace {

void expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testShouldProbeApiImmediatelyAfterWifiConnect() {
  expect(
      app::shouldProbeApi(true, false, app::ConnectivityState::Connected, 0, 1'000),
      "api probe should run immediately after WiFi becomes available");
}

void testShouldRespectProbeInterval() {
  expect(
      !app::shouldProbeApi(true, false, app::ConnectivityState::Connected, 5'000, 10'000),
      "api probe should wait for the configured retry interval");
  expect(
      app::shouldProbeApi(true, false, app::ConnectivityState::Connected, 5'000, 20'000),
      "api probe should retry after the configured interval");
}

void testProbeOutcomeUsesTransportOrBusinessErrorCode() {
  infra::ApiResult<infra::ServerProbeResponse> failed = {};
  failed.error = core::ApiError{
      .code = "HTTP_404",
      .message = "not found",
      .retryable = false,
  };

  app::ApiProbeOutcome failedOutcome = app::toApiProbeOutcome(failed);
  expect(!failedOutcome.succeeded, "failed probe should not report success");
  expect(failedOutcome.statusCode.has_value(), "failed probe should preserve an error code");
  expect(failedOutcome.statusCode.value() == "HTTP_404", "failed probe should surface HTTP status code");

  infra::ApiResult<infra::ServerProbeResponse> succeeded = {};
  succeeded.success = true;
  succeeded.data = infra::ServerProbeResponse{
      .service = "hitomi-web",
      .now = 1,
  };

  app::ApiProbeOutcome successOutcome = app::toApiProbeOutcome(succeeded);
  expect(successOutcome.succeeded, "successful probe should report success");
  expect(!successOutcome.statusCode.has_value(), "successful probe should clear previous error code");
}

}  // namespace

int main() {
  try {
    testShouldProbeApiImmediatelyAfterWifiConnect();
    testShouldRespectProbeInterval();
    testProbeOutcomeUsesTransportOrBusinessErrorCode();
    std::cout << "[PASS] api probe policy" << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] api probe policy: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
