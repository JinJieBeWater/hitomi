#include "app/runtime_camera_ops.hpp"

#include <Arduino.h>

#include "app/runtime_enrollment_ops.hpp"
#include "app/runtime_face_engine_ops.hpp"
#include "board/app_config.hpp"
#include "face/ports.hpp"
#include "infra/display_port.hpp"

namespace app {
namespace {

struct CameraPerfWindow {
  uint32_t startedAtMs = 0;
  uint32_t captureAttempts = 0;
  uint32_t captureMisses = 0;
  uint32_t previewUpdates = 0;
  uint32_t enrollmentPreviewUpdates = 0;
  uint32_t detectionPreviewUpdates = 0;
  uint64_t captureUs = 0;
  uint64_t previewSubmitUs = 0;
};

CameraPerfWindow& cameraPerfWindow() {
  static CameraPerfWindow window;
  return window;
}

void maybeLogCameraPerf(uint32_t nowMs) {
  auto& window = cameraPerfWindow();
  if (window.startedAtMs == 0) {
    window.startedAtMs = nowMs;
    return;
  }
  if (nowMs - window.startedAtMs < 1000) {
    return;
  }

  Serial.printf(
      "[PERF][CAM] capture=%u miss=%u preview=%u enroll_preview=%u detect_preview=%u capture_avg_us=%llu preview_avg_us=%llu\n",
      static_cast<unsigned>(window.captureAttempts),
      static_cast<unsigned>(window.captureMisses),
      static_cast<unsigned>(window.previewUpdates),
      static_cast<unsigned>(window.enrollmentPreviewUpdates),
      static_cast<unsigned>(window.detectionPreviewUpdates),
      static_cast<unsigned long long>(
          window.captureAttempts == 0 ? 0ULL : window.captureUs / window.captureAttempts),
      static_cast<unsigned long long>(
          window.previewUpdates == 0 ? 0ULL : window.previewSubmitUs / window.previewUpdates));

  window = CameraPerfWindow{.startedAtMs = nowMs};
}

bool previewRenderDue(const RuntimeState& state, uint32_t nowMs) {
  return state.lastCameraPreviewRenderMs == 0 ||
      nowMs - state.lastCameraPreviewRenderMs >= board::kCameraPreviewRenderIntervalMs;
}

void markPreviewRendered(RuntimeState& state, uint32_t nowMs) {
  state.lastCameraPreviewRenderMs = nowMs;
}

infra::DisplayRgb565Frame buildDetectionPreviewFrame(
    const RuntimeState& state,
    const face::CameraFrameInfo& frameInfo,
    const uint8_t* frameData) {
  infra::DisplayRgb565Frame preview = {
      .data = frameData,
      .width = frameInfo.width,
      .height = frameInfo.height,
      .faceBoxes = {},
      .faceBoxCount = 0,
      .primaryFaceBoxIndex = std::nullopt,
  };
  for (std::size_t index = 0; index < state.faceBoxCount && index < preview.faceBoxes.size(); index += 1) {
    preview.faceBoxes[index] = state.faceBoxes[index];
  }
  preview.faceBoxCount = std::min(state.faceBoxCount, preview.faceBoxes.size());
  preview.primaryFaceBoxIndex = state.primaryFaceBoxIndex;
  return preview;
}

}  // namespace

void initializeCamera(RuntimeContext& context, RuntimeState& state) {
  state.faceModuleEnabled = facePortsReady(context);
  if (!context.camera.available()) {
    context.display.clearCameraPreview();
    state.renderDirty = true;
    return;
  }

  context.camera.init();
  if (context.camera.ready()) {
    auto warmupFrame = context.camera.capture();
    if (warmupFrame != nullptr && warmupFrame->info().pixelFormat == face::CameraPixelFormat::RGB565) {
      context.display.updateCameraPreview(infra::DisplayRgb565Frame{
          .data = warmupFrame->data(),
          .width = warmupFrame->info().width,
          .height = warmupFrame->info().height,
          .faceBoxes = {},
          .faceBoxCount = 0,
          .primaryFaceBoxIndex = std::nullopt,
      });
    }
  } else {
    context.display.clearCameraPreview();
  }

  state.faceModuleEnabled = facePortsReady(context);
  state.renderDirty = true;
}

void pollCamera(RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  if (!context.camera.available() || !context.camera.ready()) {
    return;
  }
  maybeLogCameraPerf(nowMs);
  const bool shouldPreview = previewRenderDue(state, nowMs);
  const bool shouldEnrollmentProcess =
      context.enrollmentService.active() &&
      (state.lastEnrollmentFrameSampleMs == 0 ||
       nowMs - state.lastEnrollmentFrameSampleMs >= board::kEnrollmentFrameIntervalMs);
  const bool shouldFaceProcess =
      !context.enrollmentService.active() &&
      (state.lastFaceDetectionMs == 0 ||
       nowMs - state.lastFaceDetectionMs >= board::kFaceDetectIntervalMs);

  if (!shouldPreview && !shouldEnrollmentProcess && !shouldFaceProcess) {
    return;
  }

  state.lastCameraPollMs = nowMs;
  auto& perf = cameraPerfWindow();
  const uint32_t captureStartedUs = micros();
  auto frame = context.camera.capture();
  perf.captureAttempts += 1;
  perf.captureUs += static_cast<uint64_t>(micros() - captureStartedUs);
  if (frame == nullptr) {
    perf.captureMisses += 1;
  } else if (frame->info().pixelFormat == face::CameraPixelFormat::RGB565) {
    if (shouldEnrollmentProcess) {
      processEnrollmentFrame(context, state, frame->info(), frame->data(), nowMs);
      state.lastEnrollmentFrameSampleMs = nowMs;
    } else if (shouldFaceProcess) {
      runFaceDetection(context, state, frame->info(), frame->data(), nowMs);
    }

    if (shouldPreview) {
      const uint32_t previewStartedUs = micros();
      const infra::DisplayRgb565Frame preview = context.enrollmentService.active()
          ? infra::DisplayRgb565Frame{
                .data = frame->data(),
                .width = frame->info().width,
                .height = frame->info().height,
                .faceBoxes = {},
                .faceBoxCount = 0,
                .primaryFaceBoxIndex = std::nullopt,
            }
          : buildDetectionPreviewFrame(state, frame->info(), frame->data());
      context.display.updateCameraPreview(preview);
      perf.previewUpdates += 1;
      perf.previewSubmitUs += static_cast<uint64_t>(micros() - previewStartedUs);
      if (context.enrollmentService.active()) {
        perf.enrollmentPreviewUpdates += 1;
      } else {
        perf.detectionPreviewUpdates += 1;
      }
      markPreviewRendered(state, nowMs);
    }
  }
  const bool shouldRefreshStatus =
      nowMs - state.lastCameraStatusRenderMs >= board::kCameraStatusRefreshIntervalMs;
  if (shouldRefreshStatus) {
    state.lastCameraStatusRenderMs = nowMs;
  }
  if (shouldRefreshStatus || !context.camera.status().lastError.empty()) {
    state.renderDirty = true;
  }
}

}  // namespace app
