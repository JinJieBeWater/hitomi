#pragma once

#include <cstdint>

#include "app/runtime_context.hpp"

namespace app {

struct RuntimeState;

void waitForSerial();
void initWifi();
void pollBootButton(RuntimeState& state, uint32_t nowMs);
void printRuntimeCheck(const RuntimeContext& context, const RuntimeState& state);

}  // namespace app
