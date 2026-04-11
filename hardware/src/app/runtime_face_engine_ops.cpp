#include "app/runtime_face_engine_ops.hpp"

#include <Arduino.h>
#include <cstddef>
#include <list>
#include <string>
#include <memory>

#include "board/app_config.hpp"
#include "dl_detect_define.hpp"
#include "dl_image_define.hpp"
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"

namespace app {
namespace {

constexpr bool kFaceComponentsLinked =
    sizeof(HumanFaceDetect) > 0 && sizeof(HumanFaceRecognizer) > 0;

struct FaceDetectionRuntime {
  std::unique_ptr<HumanFaceDetect> detector;
};

FaceDetectionRuntime& faceDetectionRuntime() {
  static FaceDetectionRuntime runtime;
  return runtime;
}

void markFaceDetectUnavailable(RuntimeState& state, const char* detail) {
  state.faceDetectReady = false;
  state.faceDetectStatusDetail = detail;
  state.renderDirty = true;
}

void applyFaceDetectionResult(
    RuntimeState& state,
    bool detected,
    std::size_t count,
    std::optional<float> score) {
  const bool changed =
      detected != state.faceDetected ||
      count != state.detectedFaceCount ||
      score != state.faceTopScore;

  state.faceDetected = detected;
  state.detectedFaceCount = count;
  state.faceTopScore = score;
  state.faceDetectStatusDetail = detected ? std::optional<std::string>("Detected")
                                          : std::optional<std::string>("No face");
  if (changed) {
    state.renderDirty = true;
  }
}

bool ensureFaceDetector(RuntimeState& state) {
  auto& runtime = faceDetectionRuntime();
  if (runtime.detector != nullptr) {
    if (!state.faceDetectReady) {
      state.faceDetectReady = true;
      state.faceDetectStatusDetail = "Ready";
      state.renderDirty = true;
    }
    return true;
  }

  runtime.detector = std::make_unique<HumanFaceDetect>(
      static_cast<HumanFaceDetect::model_type_t>(CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL),
      false);
  state.faceDetectReady = runtime.detector != nullptr;
  state.faceDetectStatusDetail = state.faceDetectReady
      ? std::optional<std::string>("Ready")
      : std::optional<std::string>("Detector init failed");
  state.renderDirty = true;
  return state.faceDetectReady;
}

float topScore(const std::list<dl::detect::result_t>& results) {
  float top = 0.0f;
  for (const auto& result : results) {
    if (result.score > top) {
      top = result.score;
    }
  }
  return top;
}

}  // namespace

void initializeFaceEngine(RuntimeState& state) {
#if defined(CONFIG_HUMAN_FACE_DETECT_MODEL_LOCATION) && defined(CONFIG_HUMAN_FACE_FEAT_MODEL_LOCATION)
  state.faceEngineReady = kFaceComponentsLinked;
  state.faceEngineStatusDetail = kFaceComponentsLinked
      ? std::optional<std::string>("Linked (model load deferred)")
      : std::optional<std::string>("Link check failed");
#else
  state.faceEngineReady = false;
  state.faceEngineStatusDetail = "model config missing";
#endif

  state.renderDirty = true;
  state.faceDetectStatusDetail = "Waiting for first frame";

  Serial.printf("[FACE] %s\n", state.faceEngineStatusDetail.value_or("unavailable").c_str());
}

void runFaceDetection(
    RuntimeState& state,
    const face::CameraFrameInfo& frameInfo,
    const uint8_t* frameData,
    uint32_t nowMs) {
  if (!state.faceEngineReady || frameData == nullptr) {
    return;
  }
  if (frameInfo.pixelFormat != face::CameraPixelFormat::RGB565) {
    markFaceDetectUnavailable(state, "Unsupported pixel format");
    return;
  }
  if (state.lastFaceDetectionMs != 0 &&
      nowMs - state.lastFaceDetectionMs < board::kFaceDetectIntervalMs) {
    return;
  }
  state.lastFaceDetectionMs = nowMs;
  if (!ensureFaceDetector(state)) {
    return;
  }
  auto& runtime = faceDetectionRuntime();
  dl::image::img_t image = {
      .data = const_cast<uint8_t*>(frameData),
      .width = frameInfo.width,
      .height = frameInfo.height,
      .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
  };
  const auto& results = runtime.detector->run(image);
  const bool detected = !results.empty();
  const std::size_t count = results.size();
  const std::optional<float> score = detected ? std::optional<float>(topScore(results)) : std::nullopt;
  applyFaceDetectionResult(state, detected, count, score);

  if (detected) {
    Serial.printf(
        "[FACE] detected count=%u top=%.3f\n",
        static_cast<unsigned>(count),
        static_cast<double>(score.value_or(0.0f)));
  } else {
    Serial.println("[FACE] detected count=0");
  }
}

}  // namespace app
