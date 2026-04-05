#include "infra/display/lvgl_status_display.hpp"

#include <Arduino.h>
#include <lvgl.h>

#include <memory>
#include <utility>

#include "board/app_config.hpp"
#include "infra/display/szpi_lvgl_display.hpp"

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

struct LvglStatusDisplayData {
  SzpiLvglDisplay driver;
  lv_display_t* display = nullptr;
  lv_obj_t* titleLabel = nullptr;
  lv_obj_t* subtitleLabel = nullptr;
  lv_obj_t* contentLabel = nullptr;
  lv_obj_t* footerLabel = nullptr;
  uint32_t lastLvglTickMs = 0;
};

const char* lvglLogLevelName(lv_log_level_t level) {
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

void printLvglLog(lv_log_level_t level, const char* message) {
  Serial.printf("[LVGL][%s] %s\n", lvglLogLevelName(level), message);
}

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
      viewModel.apiLine.size() + viewModel.syncLine.size() + viewModel.taskLine.size() +
      viewModel.queueLine.size() + viewModel.errorLine.size() + viewModel.faceLine.size() + 16);
  content.append(viewModel.credentialsLine)
      .append("\n")
      .append(viewModel.storageLine)
      .append("\n")
      .append(viewModel.wifiLine)
      .append("\n")
      .append(viewModel.apiLine)
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

void createUi(LvglStatusDisplayData& data) {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(kBackgroundColorHex), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);

  data.titleLabel = createStatusLabel(screen, kTitleY, LV_TEXT_ALIGN_CENTER);
  data.subtitleLabel = createStatusLabel(screen, kSubtitleY, LV_TEXT_ALIGN_CENTER);

  data.contentLabel = lv_label_create(screen);
  lv_obj_set_width(data.contentLabel, kContentWidth);
  lv_label_set_long_mode(data.contentLabel, LV_LABEL_LONG_WRAP);
  applyLabelStyle(data.contentLabel, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(data.contentLabel, LV_ALIGN_TOP_LEFT, 12, kContentY);

  data.footerLabel = lv_label_create(screen);
  applyLabelStyle(data.footerLabel, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(data.footerLabel, LV_ALIGN_BOTTOM_LEFT, 12, kFooterOffsetY);
}

}  // namespace

struct LvglStatusDisplay::Impl : LvglStatusDisplayData {};

LvglStatusDisplay::LvglStatusDisplay() : impl_(std::make_unique<Impl>()) {}

LvglStatusDisplay::~LvglStatusDisplay() = default;

LvglStatusDisplay::LvglStatusDisplay(LvglStatusDisplay&&) noexcept = default;

LvglStatusDisplay& LvglStatusDisplay::operator=(LvglStatusDisplay&&) noexcept = default;

bool LvglStatusDisplay::init() {
  lv_init();
  lv_log_register_print_cb(printLvglLog);

  if (!impl_->driver.init()) {
    Serial.println("[DISPLAY] driver init failed");
    return false;
  }

  impl_->display = impl_->driver.display();
  createUi(*impl_);
  impl_->lastLvglTickMs = millis();
  lv_timer_handler();
  return true;
}

bool LvglStatusDisplay::ready() const {
  return impl_->display != nullptr;
}

void LvglStatusDisplay::render(const ui::AppViewModel& viewModel) {
  if (!ready()) {
    return;
  }

  lv_label_set_text(impl_->titleLabel, viewModel.title.c_str());
  lv_label_set_text(impl_->subtitleLabel, viewModel.subtitle.c_str());
  const std::string content = buildContentText(viewModel);
  lv_label_set_text(impl_->contentLabel, content.c_str());
  lv_label_set_text(impl_->footerLabel, viewModel.footer.c_str());
  lv_timer_handler();
}

void LvglStatusDisplay::tick(uint32_t nowMs) {
  if (!ready()) {
    return;
  }

  lv_tick_inc(nowMs - impl_->lastLvglTickMs);
  impl_->lastLvglTickMs = nowMs;
  lv_timer_handler_run_in_period(board::kLvglHandlerPeriodMs);
}

}  // namespace infra
