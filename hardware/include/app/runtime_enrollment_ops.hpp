#pragma once

#include <cstdint>

#include "app/runtime_context.hpp"
#include "app/runtime_state.hpp"
#include "face/ports.hpp"

namespace app {

void startEnrollmentTask(
    const RuntimeContext& context,
    RuntimeState& state,
    const std::string& taskId,
    uint32_t nowMs);

void cancelEnrollmentTask(
    const RuntimeContext& context,
    RuntimeState& state,
    uint32_t nowMs);

void dismissCaptureSummary(RuntimeState& state);

void processEnrollmentFrame(
    const RuntimeContext& context,
    RuntimeState& state,
    const face::CameraFrameInfo& frameInfo,
    const uint8_t* frameData,
    uint32_t nowMs);

}  // namespace app
