#pragma once

#include <cstdint>

#include "app/runtime_context.hpp"
#include "app/runtime_state.hpp"
#include "face/ports.hpp"

namespace app {

void initializeFaceEngine(RuntimeState& state);
void prepareFaceRecognition(const RuntimeContext& context, RuntimeState& state);
void discardPendingRecognition();
void runFaceDetection(
    const RuntimeContext& context,
    RuntimeState& state,
    const face::CameraFrameInfo& frameInfo,
    const uint8_t* frameData,
    uint32_t nowMs);

}  // namespace app
