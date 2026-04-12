#include "app/runtime_camera_ops.hpp"

#include "app/runtime_enrollment_ops.hpp"
#include "app/runtime_face_engine_ops.hpp"
#include "board/app_config.hpp"
#include "face/ports.hpp"
#include "infra/display_port.hpp"

namespace app {

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
  if (state.lastCameraPollMs != 0 && nowMs - state.lastCameraPollMs < board::kCameraPreviewIntervalMs) {
    return;
  }

  state.lastCameraPollMs = nowMs;
  auto frame = context.camera.capture();
  if (frame != nullptr && frame->info().pixelFormat == face::CameraPixelFormat::RGB565) {
    context.display.updateCameraPreview(infra::DisplayRgb565Frame{
        .data = frame->data(),
        .width = frame->info().width,
        .height = frame->info().height,
    });
    if (context.enrollmentService.active()) {
      processEnrollmentFrame(context, state, frame->info(), frame->data(), nowMs);
    } else {
      runFaceDetection(state, frame->info(), frame->data(), nowMs);
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
