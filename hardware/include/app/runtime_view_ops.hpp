#pragma once

#include "app/runtime_context.hpp"
#include "app/runtime_state.hpp"

namespace app {

void updateView(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);

}  // namespace app
