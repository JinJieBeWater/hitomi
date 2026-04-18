#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ui {

struct EnrollmentTaskItemViewModel {
  std::string taskId;
  std::string title;
  std::string meta;
};

struct AppViewModel {
  std::string title;
  std::string subtitle;
  std::string periodLine;
  std::string cameraHintLine;
  std::string attendanceResultLine;
  bool captureActive = false;
  bool captureRunning = false;
  std::string captureTitleLine;
  std::string captureStatusLine;
  std::string captureProgressLine;
  uint8_t captureProgressPercent = 0;
  uint8_t captureRequiredSamples = 0;
  uint8_t captureCapturedSamples = 0;
  uint8_t captureDetectedFaceCount = 0;
  std::string captureActionLabel;
  std::vector<EnrollmentTaskItemViewModel> enrollmentTasks;
  std::string enrollmentTaskSummaryLine;
  std::string credentialsLine;
  std::string storageLine;
  std::string wifiLine;
  std::string activationLine;
  std::string apiLine;
  std::string faceDetectLine;
  std::string syncLine;
  std::string taskLine;
  std::string queueLine;
  uint16_t pendingQueueCount = 0;
  std::string errorLine;
  std::string faceLine;
  std::string footer;
};

}  // namespace ui
