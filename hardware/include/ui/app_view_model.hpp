#pragma once

#include <string>

namespace ui {

struct AppViewModel {
  std::string title;
  std::string subtitle;
  std::string credentialsLine;
  std::string wifiLine;
  std::string syncLine;
  std::string taskLine;
  std::string queueLine;
  std::string errorLine;
  std::string faceLine;
  std::string footer;
};

}  // namespace ui
