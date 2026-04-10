#pragma once

#include <cstdint>

#include "app/runtime_network_executor.hpp"
#include "app/runtime_state.hpp"

namespace app {

bool hasNetworkRequestInFlight(const RuntimeState& state);
NetworkRequestType nextNetworkRequestType(const RuntimeState& state, uint32_t nowMs);

}  // namespace app
