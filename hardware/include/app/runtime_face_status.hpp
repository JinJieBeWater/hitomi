#pragma once

#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include "app/runtime_status.hpp"

namespace app {

enum class FaceLineStyle {
  Diagnostics,
  Presenter,
};

inline std::string formatFaceEngineLine(const RuntimeStatus& status, FaceLineStyle style) {
  const char* readyLabel = style == FaceLineStyle::Presenter ? "Face engine: Ready" : "Face engine: ready";
  const char* failedPrefix = style == FaceLineStyle::Presenter ? "Face engine: Failed (" : "Face engine: failed (";
  if (status.faceEngineReady) {
    if (status.faceEngineStatusDetail.has_value() && !status.faceEngineStatusDetail->empty()) {
      return "Face engine: " + status.faceEngineStatusDetail.value();
    }
    return readyLabel;
  }
  if (status.faceEngineStatusDetail.has_value() && !status.faceEngineStatusDetail->empty()) {
    return std::string(failedPrefix) + status.faceEngineStatusDetail.value() + ")";
  }
  return style == FaceLineStyle::Presenter ? "Face engine: Unavailable" : "Face engine: unavailable";
}

inline std::string formatFaceDetectLine(const RuntimeStatus& status, FaceLineStyle style) {
  if (!status.faceEngineReady) {
    return "Detect: engine unavailable";
  }
  if (!status.faceDetectReady) {
    return "Detect: " + status.faceDetectStatusDetail.value_or("idle");
  }
  if (!status.faceDetected) {
    return "Detect: none";
  }

  std::ostringstream oss;
  if (style == FaceLineStyle::Presenter) {
    oss << "Detect: " << status.detectedFaceCount << " face(s)";
    if (status.faceTopScore.has_value()) {
      oss << " @" << std::fixed << std::setprecision(2) << status.faceTopScore.value();
    }
    return oss.str();
  }

  oss << "Detect: faces=" << status.detectedFaceCount;
  if (status.faceTopScore.has_value()) {
    oss << ", top=" << status.faceTopScore.value();
  }
  return oss.str();
}

}  // namespace app
