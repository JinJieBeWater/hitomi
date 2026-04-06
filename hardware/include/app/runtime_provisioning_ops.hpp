#pragma once

#include <cstdint>

#include "app/runtime_context.hpp"
#include "app/runtime_state.hpp"

namespace app {

void processUsbProvisioning(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);

}  // namespace app
