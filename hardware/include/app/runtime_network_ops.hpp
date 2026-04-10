#pragma once

#include <cstdint>

#include "app/runtime_context.hpp"
#include "app/runtime_state.hpp"
#include "infra/display_port.hpp"

namespace app {

void resetNetworkTaskSchedule(RuntimeState& state);
void invalidatePendingNetworkRequests(RuntimeState& state);
void probeConnectivity(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);
void handleDisplayCommand(
    const RuntimeContext& context,
    RuntimeState& state,
    const infra::DisplayCommand& command,
    uint32_t nowMs);
void consumeCompletedNetworkRequest(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);
void dispatchNetworkRequest(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);

}  // namespace app
