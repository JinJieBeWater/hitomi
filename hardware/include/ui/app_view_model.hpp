#pragma once

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
  std::vector<EnrollmentTaskItemViewModel> enrollmentTasks;
  std::string enrollmentTaskSummaryLine;
  std::string credentialsLine;
  std::string storageLine;
  std::string wifiLine;
  std::string activationLine;
  std::string apiLine;
  std::string syncLine;
  std::string taskLine;
  std::string queueLine;
  std::string errorLine;
  std::string faceLine;
  std::string footer;
};

}  // namespace ui
