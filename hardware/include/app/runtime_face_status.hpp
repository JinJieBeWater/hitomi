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

inline std::string localizeFaceEngineDetailForPresenter(const std::string& detail) {
  if (detail == "Linked (model load deferred)") {
    return "已连接";
  }
  if (detail == "Link check failed") {
    return "链接检查失败";
  }
  if (detail == "model config missing") {
    return "缺少模型配置";
  }
  return detail;
}

inline std::string localizeFaceDetectDetailForPresenter(const std::string& detail) {
  if (detail == "Waiting for first frame") {
    return "等待首帧";
  }
  if (detail == "Detected") {
    return "已检测到人脸";
  }
  if (detail == "No face") {
    return "未检测到人脸";
  }
  if (detail == "Ready") {
    return "已就绪";
  }
  if (detail == "Detector init failed") {
    return "检测器初始化失败";
  }
  if (detail == "Unsupported pixel format") {
    return "像素格式不支持";
  }
  if (detail == "Bad camera frame") {
    return "相机坏帧，已跳过";
  }
  if (detail == "Recognizing") {
    return "识别中...";
  }
  if (detail == "No matching employee") {
    return "未匹配到员工";
  }
  if (detail == "Attendance recorded") {
    return "考勤已记录";
  }
  if (detail == "Duplicate attendance") {
    return "今日已打卡";
  }
  if (detail == "Not in attendance window") {
    return "不在考勤时段";
  }
  if (detail == "Attendance config missing") {
    return "考勤配置缺失";
  }
  return detail;
}

inline std::string formatFaceEngineLine(const RuntimeStatus& status, FaceLineStyle style) {
  const char* readyLabel = style == FaceLineStyle::Presenter ? "已就绪" : "Face engine: ready";
  const char* failedPrefix = style == FaceLineStyle::Presenter ? "不可用（" : "Face engine: failed (";
  if (status.faceEngineReady) {
    if (status.faceEngineStatusDetail.has_value() && !status.faceEngineStatusDetail->empty()) {
      const std::string detail = style == FaceLineStyle::Presenter
          ? localizeFaceEngineDetailForPresenter(status.faceEngineStatusDetail.value())
          : status.faceEngineStatusDetail.value();
      return std::string(style == FaceLineStyle::Presenter ? "" : "Face engine: ") + detail;
    }
    return readyLabel;
  }
  if (status.faceEngineStatusDetail.has_value() && !status.faceEngineStatusDetail->empty()) {
    const std::string detail = style == FaceLineStyle::Presenter
        ? localizeFaceEngineDetailForPresenter(status.faceEngineStatusDetail.value())
        : status.faceEngineStatusDetail.value();
    return std::string(failedPrefix) + detail + ")";
  }
  return style == FaceLineStyle::Presenter ? "不可用" : "Face engine: unavailable";
}

inline std::string formatFaceDetectLine(const RuntimeStatus& status, FaceLineStyle style) {
  if (!status.faceEngineReady) {
    return style == FaceLineStyle::Presenter ? "检测：识别引擎不可用" : "Detect: engine unavailable";
  }
  if (!status.faceDetectReady) {
    const std::string detail = status.faceDetectStatusDetail.value_or("idle");
    return std::string(style == FaceLineStyle::Presenter ? "检测：" : "Detect: ") +
        (style == FaceLineStyle::Presenter ? localizeFaceDetectDetailForPresenter(detail) : detail);
  }
  const std::string detail = status.faceDetectStatusDetail.value_or("");
  if (detail == "Bad camera frame") {
    return std::string(style == FaceLineStyle::Presenter ? "检测：" : "Detect: ") +
        (style == FaceLineStyle::Presenter ? localizeFaceDetectDetailForPresenter(detail) : detail);
  }
  if (!status.faceDetected) {
    return style == FaceLineStyle::Presenter ? "检测：无人脸" : "Detect: none";
  }
  if (
      detail == "Recognizing" || detail == "No matching employee" || detail == "Attendance recorded" ||
      detail == "Duplicate attendance" || detail == "Not in attendance window" ||
      detail == "Attendance config missing") {
    return std::string(style == FaceLineStyle::Presenter ? "检测：" : "Detect: ") +
        (style == FaceLineStyle::Presenter ? localizeFaceDetectDetailForPresenter(detail) : detail);
  }

  std::ostringstream oss;
  if (style == FaceLineStyle::Presenter) {
    oss << "检测：" << status.detectedFaceCount << " 张人脸";
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
