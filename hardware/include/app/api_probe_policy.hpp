#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "app/runtime_status.hpp"
#include "infra/device_api_client.hpp"

namespace app {

struct ApiProbeOutcome {
  bool succeeded = false;
  std::optional<std::string> statusCode;
};

bool shouldProbeApi(
    bool apiConfigured,
    bool apiProbeInFlight,
    ConnectivityState connectivity,
    uint32_t lastApiProbeAttemptMs,
    uint32_t nowMs);

ApiProbeOutcome toApiProbeOutcome(const infra::ApiResult<infra::ServerProbeResponse>& result);

}  // namespace app
