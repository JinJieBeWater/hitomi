#pragma once

#include <cstdint>

#include "app/runtime_state.hpp"
#include "face/ports.hpp"

namespace app {

void initializeFaceEngine(RuntimeState& state);
void runFaceDetection(
    RuntimeState& state,
    const face::CameraFrameInfo& frameInfo,
    const uint8_t* frameData,
    uint32_t nowMs);

}  // namespace app
