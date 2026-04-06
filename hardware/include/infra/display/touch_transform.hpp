#pragma once

#include <cstdint>

namespace infra {

struct TouchPoint {
  int16_t x = 0;
  int16_t y = 0;
};

struct TouchTransformConfig {
  int16_t width = 0;
  int16_t height = 0;
  bool swapXY = false;
  bool mirrorX = false;
  bool mirrorY = false;
};

struct DisplayOrientation {
  bool swapXY = false;
  bool mirrorX = false;
  bool mirrorY = false;
};

struct TouchReadState {
  TouchPoint lastPoint = {};
  bool pressed = false;
};

struct TouchReadResult {
  TouchPoint point = {};
  bool pressed = false;
  TouchReadState nextState = {};
};

constexpr DisplayOrientation kSzpiPanelOrientation = {
    .swapXY = true,
    .mirrorX = true,
    .mirrorY = false,
};

constexpr DisplayOrientation kSzpiTouchOrientation = {
    .swapXY = true,
    .mirrorX = false,
    .mirrorY = true,
};

constexpr int16_t clampTouchCoord(int32_t value, int16_t upperExclusive) {
  if (upperExclusive <= 0) {
    return 0;
  }

  if (value < 0) {
    return 0;
  }

  const int32_t upperInclusive = static_cast<int32_t>(upperExclusive) - 1;
  if (value > upperInclusive) {
    return static_cast<int16_t>(upperInclusive);
  }

  return static_cast<int16_t>(value);
}

constexpr TouchPoint applyTouchTransform(int32_t rawX, int32_t rawY, const TouchTransformConfig& config) {
  int32_t x = rawX;
  int32_t y = rawY;

  if (config.swapXY) {
    const int32_t tmp = x;
    x = y;
    y = tmp;
  }

  if (config.mirrorX) {
    x = static_cast<int32_t>(config.width) - 1 - x;
  }

  if (config.mirrorY) {
    y = static_cast<int32_t>(config.height) - 1 - y;
  }

  return TouchPoint{
      .x = clampTouchCoord(x, config.width),
      .y = clampTouchCoord(y, config.height),
  };
}

constexpr TouchTransformConfig makeTouchTransform(int16_t width, int16_t height, const DisplayOrientation& orientation) {
  return TouchTransformConfig{
      .width = width,
      .height = height,
      .swapXY = orientation.swapXY,
      .mirrorX = orientation.mirrorX,
      .mirrorY = orientation.mirrorY,
  };
}

constexpr TouchReadResult resolveTouchReadState(
    const TouchReadState& previous,
    bool readSucceeded,
    bool samplePressed,
    const TouchPoint& samplePoint) {
  if (!readSucceeded) {
    return TouchReadResult{
        .point = previous.lastPoint,
        .pressed = previous.pressed,
        .nextState = previous,
    };
  }

  if (!samplePressed) {
    return TouchReadResult{
        .point = previous.lastPoint,
        .pressed = false,
        .nextState = TouchReadState{
            .lastPoint = previous.lastPoint,
            .pressed = false,
        },
    };
  }

  return TouchReadResult{
      .point = samplePoint,
      .pressed = true,
      .nextState = TouchReadState{
          .lastPoint = samplePoint,
          .pressed = true,
      },
  };
}

}  // namespace infra
