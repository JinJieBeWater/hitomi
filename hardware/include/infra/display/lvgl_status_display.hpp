#pragma once

#include <cstdint>

#include <lvgl.h>

#include "board/app_config.hpp"
#include "infra/display/szpi_lvgl_display.hpp"
#include "infra/display_port.hpp"

namespace infra {

class LvglStatusDisplay final : public DisplayPort {
 public:
  bool init() override;
  bool ready() const override;
  void render(const ui::AppViewModel& viewModel) override;
  void tick(uint32_t nowMs) override;

 private:
  void createUi();
  static const char* lvglLogLevelName(lv_log_level_t level);
  static void printLvglLog(lv_log_level_t level, const char* message);

  SzpiLvglDisplay driver_;
  lv_display_t* display_ = nullptr;
  lv_obj_t* titleLabel_ = nullptr;
  lv_obj_t* subtitleLabel_ = nullptr;
  lv_obj_t* credentialsLabel_ = nullptr;
  lv_obj_t* storageLabel_ = nullptr;
  lv_obj_t* wifiLabel_ = nullptr;
  lv_obj_t* syncLabel_ = nullptr;
  lv_obj_t* taskLabel_ = nullptr;
  lv_obj_t* queueLabel_ = nullptr;
  lv_obj_t* errorLabel_ = nullptr;
  lv_obj_t* faceLabel_ = nullptr;
  lv_obj_t* footerLabel_ = nullptr;
  uint32_t lastLvglTickMs_ = 0;
};

}  // namespace infra
