#pragma once

#include <cstdint>

#include "app/runtime_context.hpp"
#include "app/runtime_state.hpp"
#include "infra/display_port.hpp"

namespace app {

void resetNetworkTaskSchedule(RuntimeState& state);
void probeConnectivity(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);
bool shouldProbeApi(const RuntimeContext& context, const RuntimeState& state, uint32_t nowMs);
void handleDisplayCommand(
    const RuntimeContext& context,
    RuntimeState& state,
    const infra::DisplayCommand& command,
    uint32_t nowMs);
void performApiProbe(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);
void performManualRefresh(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);
bool shouldActivate(const RuntimeContext& context, const RuntimeState& state, uint32_t nowMs);
void performActivation(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);
bool shouldSync(const RuntimeContext& context, const RuntimeState& state, uint32_t nowMs);
bool shouldUpload(const RuntimeContext& context, const RuntimeState& state, uint32_t nowMs);
void performSync(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);
void performUpload(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);

}  // namespace app
