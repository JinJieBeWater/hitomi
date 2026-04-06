#pragma once

#include <cstdint>

#include "app/runtime_context.hpp"

namespace app {

struct RuntimeState;

void waitForSerial();
void seedDeviceConfigFromBoardDefaults(const RuntimeContext& context, RuntimeState& state);
void syncDeviceApiClientConfig(const RuntimeContext& context, const RuntimeState& state);
void initWifi();
void ensureWifiConnection(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);
void pollBootButton(RuntimeState& state, uint32_t nowMs);
void printRuntimeCheck(const RuntimeContext& context, const RuntimeState& state);

}  // namespace app
