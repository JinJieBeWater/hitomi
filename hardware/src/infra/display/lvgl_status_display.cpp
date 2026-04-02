#include "infra/display/lvgl_status_display.hpp"

#include <Arduino.h>

namespace infra {
namespace {

constexpr lv_coord_t kScreenWidth = 296;
constexpr lv_coord_t kContentWidth = 296;
constexpr lv_coord_t kTitleY = 12;
constexpr lv_coord_t kSubtitleY = 38;
constexpr lv_coord_t kContentY = 68;
constexpr lv_coord_t kFooterOffsetY = -10;
constexpr uint32_t kTextColorHex = 0xFFFFFF;
constexpr uint32_t kBackgroundColorHex = 0x1F8A3B;

void applyLabelStyle(lv_obj_t* label, lv_text_align_t align) {
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(kTextColorHex), 0);
  lv_obj_set_style_text_align(label, align, 0);
}

lv_obj_t* createStatusLabel(lv_obj_t* parent, lv_coord_t y, lv_text_align_t align) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_width(label, kScreenWidth);
  applyLabelStyle(label, align);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y);
  return label;
}

std::string buildContentText(const ui::AppViewModel& viewModel) {
  std::string content;
  content.reserve(
      viewModel.credentialsLine.size() + viewModel.storageLine.size() + viewModel.wifiLine.size() +
      viewModel.syncLine.size() + viewModel.taskLine.size() + viewModel.queueLine.size() +
      viewModel.errorLine.size() + viewModel.faceLine.size() + 16);
  content.append(viewModel.credentialsLine)
      .append("\n")
      .append(viewModel.storageLine)
      .append("\n")
      .append(viewModel.wifiLine)
      .append("\n")
      .append(viewModel.syncLine)
      .append("\n")
      .append(viewModel.taskLine)
      .append("\n")
      .append(viewModel.queueLine)
      .append("\n")
      .append(viewModel.errorLine)
      .append("\n")
      .append(viewModel.faceLine);
  return content;
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
  const std::string content = buildContentText(viewModel);
  lv_label_set_text(contentLabel_, content.c_str());
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
  lv_obj_set_style_bg_color(screen, lv_color_hex(kBackgroundColorHex), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);

  titleLabel_ = createStatusLabel(screen, kTitleY, LV_TEXT_ALIGN_CENTER);
  subtitleLabel_ = createStatusLabel(screen, kSubtitleY, LV_TEXT_ALIGN_CENTER);

  contentLabel_ = lv_label_create(screen);
  lv_obj_set_width(contentLabel_, kContentWidth);
  lv_label_set_long_mode(contentLabel_, LV_LABEL_LONG_WRAP);
  applyLabelStyle(contentLabel_, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(contentLabel_, LV_ALIGN_TOP_LEFT, 12, kContentY);

  footerLabel_ = lv_label_create(screen);
  applyLabelStyle(footerLabel_, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(footerLabel_, LV_ALIGN_BOTTOM_LEFT, 12, kFooterOffsetY);
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
