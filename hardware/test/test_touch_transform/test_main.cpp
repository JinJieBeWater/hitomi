#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "infra/display/touch_transform.hpp"

namespace {

void expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testPassThroughTransformKeepsRawCoordinates() {
  constexpr infra::TouchTransformConfig config = {
      .width = 320,
      .height = 240,
      .swapXY = false,
      .mirrorX = false,
      .mirrorY = false,
  };

  constexpr infra::TouchPoint point = infra::applyTouchTransform(42, 99, config);
  static_assert(point.x == 42, "x should pass through");
  static_assert(point.y == 99, "y should pass through");

  expect(point.x == 42, "x should pass through at runtime");
  expect(point.y == 99, "y should pass through at runtime");
}

void testSwapAndMirrorMatchesBoardTransformMath() {
  constexpr infra::TouchTransformConfig config =
      infra::makeTouchTransform(320, 240, infra::kSzpiTouchOrientation);

  const infra::TouchPoint point = infra::applyTouchTransform(10, 20, config);
  expect(point.x == 20, "x should come from the raw y coordinate after swapping");
  expect(point.y == 229, "y should be mirrored after swapping");
}

void testTransformClampsOutOfRangeCoordinates() {
  constexpr infra::TouchTransformConfig config =
      infra::makeTouchTransform(320, 240, infra::kSzpiTouchOrientation);

  const infra::TouchPoint point = infra::applyTouchTransform(-50, 500, config);
  expect(point.x == 319, "swapped x should clamp to the right edge");
  expect(point.y == 239, "mirrored y should clamp to the bottom edge");
}

void testReadFailureKeepsPreviousPressedState() {
  constexpr infra::TouchReadState previous = {
      .lastPoint = {.x = 120, .y = 80},
      .pressed = true,
  };

  constexpr infra::TouchReadResult resolved =
      infra::resolveTouchReadState(previous, false, false, infra::TouchPoint{});

  static_assert(resolved.pressed, "read failure should preserve pressed state");
  static_assert(resolved.point.x == 120, "read failure should preserve last x");
  static_assert(resolved.point.y == 80, "read failure should preserve last y");

  expect(resolved.pressed, "read failure should preserve pressed state");
  expect(resolved.point.x == 120, "read failure should preserve last x");
  expect(resolved.point.y == 80, "read failure should preserve last y");
}

void testReleaseClearsPressedStateButKeepsLastPoint() {
  constexpr infra::TouchReadState previous = {
      .lastPoint = {.x = 100, .y = 60},
      .pressed = true,
  };

  const infra::TouchReadResult resolved =
      infra::resolveTouchReadState(previous, true, false, infra::TouchPoint{});

  expect(!resolved.pressed, "release sample should clear the pressed state");
  expect(!resolved.nextState.pressed, "stored state should become released");
  expect(resolved.point.x == 100, "release should keep the last x for LVGL");
  expect(resolved.point.y == 60, "release should keep the last y for LVGL");
}

void testPressedSampleUpdatesStateAndPoint() {
  constexpr infra::TouchReadState previous = {
      .lastPoint = {.x = 0, .y = 0},
      .pressed = false,
  };

  const infra::TouchPoint samplePoint = {.x = 210, .y = 35};
  const infra::TouchReadResult resolved = infra::resolveTouchReadState(previous, true, true, samplePoint);

  expect(resolved.pressed, "pressed sample should report pressed");
  expect(resolved.nextState.pressed, "stored state should become pressed");
  expect(resolved.point.x == 210, "pressed sample should use the new x");
  expect(resolved.point.y == 35, "pressed sample should use the new y");
}

}  // namespace

int main() {
  try {
    testPassThroughTransformKeepsRawCoordinates();
    testSwapAndMirrorMatchesBoardTransformMath();
    testTransformClampsOutOfRangeCoordinates();
    testReadFailureKeepsPreviousPressedState();
    testReleaseClearsPressedStateButKeepsLastPoint();
    testPressedSampleUpdatesStateAndPoint();
    std::cout << "[PASS] touch transform" << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] touch transform: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
