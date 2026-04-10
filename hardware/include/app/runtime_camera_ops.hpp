#pragma once

#include <cstdint>

#include "app/runtime_context.hpp"
#include "app/runtime_state.hpp"

namespace app {

void initializeCamera(RuntimeContext& context, RuntimeState& state);
void pollCamera(RuntimeContext& context, RuntimeState& state, uint32_t nowMs);

}  // namespace app
