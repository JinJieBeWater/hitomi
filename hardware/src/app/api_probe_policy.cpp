#include "app/api_probe_policy.hpp"

#include "board/app_config.hpp"

namespace app {

bool shouldProbeApi(
    bool apiConfigured,
    bool apiProbeInFlight,
    ConnectivityState connectivity,
    uint32_t lastApiProbeAttemptMs,
    uint32_t nowMs) {
  if (!apiConfigured || apiProbeInFlight || connectivity != ConnectivityState::Connected) {
    return false;
  }

  if (lastApiProbeAttemptMs == 0) {
    return true;
  }

  return nowMs - lastApiProbeAttemptMs >= board::kApiProbeIntervalMs;
}

ApiProbeOutcome toApiProbeOutcome(const infra::ApiResult<infra::ServerProbeResponse>& result) {
  if (result.success && result.data.has_value()) {
    return {
        .succeeded = true,
        .statusCode = std::nullopt,
    };
  }

  return {
      .succeeded = false,
      .statusCode = result.error.has_value() && !result.error->code.empty()
          ? std::optional<std::string>(result.error->code)
          : std::optional<std::string>("UNKNOWN_ERROR"),
  };
}

}  // namespace app
