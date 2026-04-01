#include "infra/display/lvgl_status_display.hpp"

#include <Arduino.h>

namespace infra {
namespace {

lv_obj_t* createStatusLabel(lv_obj_t* parent, lv_coord_t y, lv_text_align_t align) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_width(label, 280);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(0xF4F1DE), 0);
  lv_obj_set_style_text_align(label, align, 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  return label;
}

}  // namespace

bool LvglStatusDisplay::init() {
  lv_init();
  lv_log_register_print_cb(printLvglLog);

  if (!driver_.init()) {
    Serial.println("[DISPLAY] driver init failed");
    return false;
  }

  display_ = driver_.display();
  createUi();
  lastLvglTickMs_ = millis();
  lv_timer_handler();
  return true;
}

bool LvglStatusDisplay::ready() const {
  return display_ != nullptr;
}

void LvglStatusDisplay::render(const ui::AppViewModel& viewModel) {
  if (!ready()) {
    return;
  }

  lv_label_set_text(titleLabel_, viewModel.title.c_str());
  lv_label_set_text(subtitleLabel_, viewModel.subtitle.c_str());
  lv_label_set_text(credentialsLabel_, viewModel.credentialsLine.c_str());
  lv_label_set_text(wifiLabel_, viewModel.wifiLine.c_str());
  lv_label_set_text(syncLabel_, viewModel.syncLine.c_str());
  lv_label_set_text(taskLabel_, viewModel.taskLine.c_str());
  lv_label_set_text(queueLabel_, viewModel.queueLine.c_str());
  lv_label_set_text(errorLabel_, viewModel.errorLine.c_str());
  lv_label_set_text(faceLabel_, viewModel.faceLine.c_str());
  lv_label_set_text(footerLabel_, viewModel.footer.c_str());
  lv_timer_handler();
}

void LvglStatusDisplay::tick(uint32_t nowMs) {
  if (!ready()) {
    return;
  }

  lv_tick_inc(nowMs - lastLvglTickMs_);
  lastLvglTickMs_ = nowMs;
  lv_timer_handler_run_in_period(board::kLvglHandlerPeriodMs);
}

void LvglStatusDisplay::createUi() {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x102542), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  titleLabel_ = createStatusLabel(screen, 12, LV_TEXT_ALIGN_CENTER);

  subtitleLabel_ = createStatusLabel(screen, 40, LV_TEXT_ALIGN_CENTER);
  lv_obj_set_style_text_color(subtitleLabel_, lv_color_hex(0xBFD7EA), 0);

  credentialsLabel_ = createStatusLabel(screen, 76, LV_TEXT_ALIGN_LEFT);
  wifiLabel_ = createStatusLabel(screen, 98, LV_TEXT_ALIGN_LEFT);
  syncLabel_ = createStatusLabel(screen, 120, LV_TEXT_ALIGN_LEFT);
  taskLabel_ = createStatusLabel(screen, 142, LV_TEXT_ALIGN_LEFT);
  queueLabel_ = createStatusLabel(screen, 164, LV_TEXT_ALIGN_LEFT);
  errorLabel_ = createStatusLabel(screen, 186, LV_TEXT_ALIGN_LEFT);
  lv_obj_set_style_text_color(errorLabel_, lv_color_hex(0xE76F51), 0);
  faceLabel_ = createStatusLabel(screen, 208, LV_TEXT_ALIGN_LEFT);

  footerLabel_ = lv_label_create(screen);
  lv_obj_set_style_text_color(footerLabel_, lv_color_hex(0xE9C46A), 0);
  lv_obj_align(footerLabel_, LV_ALIGN_BOTTOM_MID, 0, -10);
}

const char* LvglStatusDisplay::lvglLogLevelName(lv_log_level_t level) {
  switch (level) {
    case LV_LOG_LEVEL_TRACE:
      return "Trace";
    case LV_LOG_LEVEL_INFO:
      return "Info";
    case LV_LOG_LEVEL_WARN:
      return "Warn";
    case LV_LOG_LEVEL_ERROR:
      return "Error";
    case LV_LOG_LEVEL_USER:
      return "User";
    case LV_LOG_LEVEL_NONE:
      return "None";
    default:
      return "Log";
  }
}

void LvglStatusDisplay::printLvglLog(lv_log_level_t level, const char* message) {
  Serial.printf("[LVGL][%s] %s\n", lvglLogLevelName(level), message);
}

}  // namespace infra
