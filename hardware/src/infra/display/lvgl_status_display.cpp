#include "infra/display/lvgl_status_display.hpp"

#include <Arduino.h>
#include <lvgl.h>
#include <libs/tiny_ttf/lv_tiny_ttf.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "board/app_config.hpp"
#include "fonts/hitomi_ui_subset_ttf.h"
#include "infra/display/szpi_lvgl_display.hpp"

namespace infra {
namespace {

constexpr lv_coord_t kSidebarWidth = 84;
constexpr lv_coord_t kPaneInset = 12;
constexpr lv_coord_t kPageX = kSidebarWidth + kPaneInset;
constexpr lv_coord_t kPageY = 14;
constexpr lv_coord_t kPageWidth = SzpiLvglDisplay::kHorizontalResolution - kSidebarWidth - (kPaneInset * 2);
constexpr lv_coord_t kPageHeight = SzpiLvglDisplay::kVerticalResolution - kPageY - 8;

constexpr lv_coord_t kNavButtonWidth = 64;
constexpr lv_coord_t kNavButtonHeight = 32;
constexpr lv_coord_t kSidebarTitleY = 10;
constexpr lv_coord_t kSidebarSubtitleY = 34;
constexpr lv_coord_t kSidebarFooterY = -10;
constexpr lv_coord_t kNavStartY = 66;
constexpr lv_coord_t kNavStepY = 38;

constexpr lv_coord_t kHomePeriodY = 0;
constexpr lv_coord_t kHomePeriodHeight = 28;
constexpr lv_coord_t kHomeStatusY = 36;
constexpr lv_coord_t kHomeStatusHeight = 0;
constexpr lv_coord_t kHomeCameraY = 36;
constexpr lv_coord_t kHomeCameraHeight = (kPageWidth * 3) / 4;

constexpr lv_coord_t kEnrollHeaderY = 0;
constexpr lv_coord_t kEnrollSummaryY = 18;
constexpr lv_coord_t kEnrollMetaY = 18;
constexpr lv_coord_t kEnrollTaskY = 40;
constexpr lv_coord_t kEnrollTaskHeight = 44;
constexpr lv_coord_t kEnrollTaskGap = 6;
constexpr lv_coord_t kEnrollTaskListHeight = 112;
constexpr lv_coord_t kEnrollDetailY = 160;
constexpr lv_coord_t kEnrollDetailHeight = 58;
constexpr lv_coord_t kEnrollActionHeight = 22;
constexpr lv_coord_t kEnrollActionGap = 8;

constexpr lv_coord_t kCaptureHeaderY = 0;
constexpr lv_coord_t kCapturePreviewY = 22;
constexpr lv_coord_t kCapturePreviewHeight = 108;
constexpr lv_coord_t kCaptureStatusY = 138;
constexpr lv_coord_t kCaptureProgressY = 162;
constexpr lv_coord_t kCaptureBarY = 184;
constexpr lv_coord_t kCaptureBarHeight = 8;
constexpr lv_coord_t kCaptureActionY = 194;
constexpr lv_coord_t kCaptureActionHeight = 24;
constexpr lv_coord_t kCaptureFacePillWidth = 70;
constexpr lv_coord_t kCaptureFacePillHeight = 18;
constexpr lv_coord_t kCaptureSampleDotSize = 10;
constexpr lv_coord_t kCaptureSampleDotGap = 8;
constexpr std::size_t kCaptureSampleIndicatorCount = 5;

constexpr lv_coord_t kSystemSummaryHeight = 54;
constexpr lv_coord_t kSystemSummaryY = 0;
constexpr lv_coord_t kSystemRowsY = 64;
constexpr lv_coord_t kSystemRowHeight = 20;
constexpr lv_coord_t kSystemRowGap = 2;

constexpr uint32_t kSidebarBackgroundHex = 0x0D3B2A;
constexpr uint32_t kPaneBackgroundHex = 0x1F8A3B;
constexpr uint32_t kPanelHex = 0x2A9D50;
constexpr uint32_t kPanelMutedHex = 0x247E47;
constexpr uint32_t kPanelBorderHex = 0x8ED1A0;
constexpr uint32_t kPanelBorderMutedHex = 0x1C5B34;
constexpr uint32_t kCameraPlaceholderHex = 0x113F2C;
constexpr uint32_t kTextColorHex = 0xFFFFFF;
constexpr uint32_t kMutedTextHex = 0xDCE8DD;
constexpr uint32_t kNavButtonHex = 0x145A32;
constexpr uint32_t kNavButtonActiveHex = 0xF0F3BD;
constexpr uint32_t kNavButtonActiveTextHex = 0x0D3B2A;
constexpr uint32_t kAccentHex = 0xF4A261;
constexpr uint32_t kGoodHex = 0x5FD38D;
constexpr uint32_t kWarnHex = 0xF4C35C;
constexpr uint32_t kErrorHex = 0xE76F51;
constexpr std::size_t kDisplayCommandQueueCapacity = 4;
constexpr std::size_t kDisplayNotificationQueueCapacity = 4;
constexpr lv_coord_t kNotificationWidth = 72;
constexpr lv_coord_t kNotificationMinHeight = 44;
constexpr lv_coord_t kNotificationBottomY = -10;

enum class SidebarPage : std::size_t {
  Home = 0,
  Enroll,
  Capture,
  System,
};

constexpr std::size_t kPageCount = 4;

struct SidebarPageSpec {
  SidebarPage page;
  const char* label;
};

constexpr std::array<SidebarPageSpec, 3> kSidebarPages = {{
    {.page = SidebarPage::Home, .label = "首页"},
    {.page = SidebarPage::Enroll, .label = "录脸"},
    {.page = SidebarPage::System, .label = "系统"},
}};

struct LvglStatusDisplayData;

struct SidebarBinding {
  LvglStatusDisplayData* owner = nullptr;
  SidebarPage page = SidebarPage::Home;
};

enum class StatusTone : uint8_t {
  Neutral = 0,
  Good,
  Warn,
  Error,
};

struct StatusRow {
  lv_obj_t* panel = nullptr;
  lv_obj_t* led = nullptr;
  lv_obj_t* titleLabel = nullptr;
  lv_obj_t* valueLabel = nullptr;
};

struct PreviewBuffer {
  std::vector<uint16_t> pixels = {};
  lv_coord_t width = 0;
  lv_coord_t height = 0;
};

struct LvglStatusDisplayData {
  SzpiLvglDisplay driver;
  lv_display_t* display = nullptr;
  std::array<lv_obj_t*, kSidebarPages.size()> navButtons = {};
  std::array<SidebarBinding, kSidebarPages.size()> navBindings = {};
  std::array<lv_obj_t*, kPageCount> pageContainers = {};
  lv_obj_t* sidebarTitleLabel = nullptr;
  lv_obj_t* sidebarSubtitleLabel = nullptr;
  lv_obj_t* sidebarFooterLabel = nullptr;

  lv_obj_t* homePeriodLabel = nullptr;
  lv_obj_t* homeStatusLabel = nullptr;
  lv_obj_t* homeCameraImage = nullptr;
  lv_obj_t* homeCameraFrame = nullptr;
  lv_obj_t* homeCameraLabel = nullptr;
  lv_image_dsc_t previewImageDsc = {};
  std::array<lv_obj_t*, DisplayRgb565Frame::kMaxFaceBoxes> homeFaceBoxOverlays = {};
  std::array<PreviewBuffer, 2> previewBuffers = {};
  std::size_t activePreviewBufferIndex = 0;
  bool homeCameraPreviewReady = false;
  DisplayRgb565Frame latestPreviewFrame = {};

  lv_obj_t* enrollSummaryLabel = nullptr;
  lv_obj_t* enrollMetaLabel = nullptr;
  lv_obj_t* enrollTaskList = nullptr;
  lv_obj_t* enrollSelectionCard = nullptr;
  lv_obj_t* enrollSelectionLed = nullptr;
  lv_obj_t* enrollSelectionTitleLabel = nullptr;
  lv_obj_t* enrollSelectionMetaLabel = nullptr;
  lv_obj_t* enrollRefreshButton = nullptr;
  lv_obj_t* enrollStartButton = nullptr;

  lv_obj_t* capturePreviewImage = nullptr;
  lv_obj_t* capturePreviewFrame = nullptr;
  lv_obj_t* captureHintLabel = nullptr;
  lv_obj_t* captureFaceStateLabel = nullptr;
  lv_obj_t* captureStatusLabel = nullptr;
  lv_obj_t* captureProgressLabel = nullptr;
  lv_obj_t* captureProgressBar = nullptr;
  lv_obj_t* captureCancelButton = nullptr;
  std::array<lv_obj_t*, kCaptureSampleIndicatorCount> captureSampleDots = {};
  std::array<lv_obj_t*, DisplayRgb565Frame::kMaxFaceBoxes> captureFaceBoxOverlays = {};

  lv_obj_t* systemSummaryCard = nullptr;
  lv_obj_t* systemActivationLabel = nullptr;
  lv_obj_t* systemSyncLabel = nullptr;
  lv_obj_t* systemFooterLabel = nullptr;
  std::array<StatusRow, 7> systemRows = {};

  ui::AppViewModel lastViewModel = {};
  uint32_t lastLvglTickMs = 0;
  SidebarPage currentPage = SidebarPage::Home;
  bool hasViewModel = false;
  bool previewNeedsPresent = false;
  uint32_t lastPreviewPresentMs = 0;
  std::string selectedEnrollmentTaskId;
  bool enrollmentRequestQueued = false;
  std::array<DisplayCommand, kDisplayCommandQueueCapacity> commandQueue = {};
  std::size_t commandHead = 0;
  std::size_t queuedCommandCount = 0;
  lv_obj_t* notificationPanel = nullptr;
  lv_obj_t* notificationLabel = nullptr;
  std::array<DisplayNotification, kDisplayNotificationQueueCapacity> notificationQueue = {};
  std::size_t notificationHead = 0;
  std::size_t queuedNotificationCount = 0;
  bool notificationVisible = false;
  uint32_t notificationHideAtMs = 0;
};

struct DisplayPreviewPerfWindow {
  uint32_t startedAtMs = 0;
  uint32_t previewUpdates = 0;
  uint32_t skippedByPage = 0;
  uint64_t resampleUs = 0;
  uint64_t lvglUs = 0;
};

lv_font_t* gUiFont12 = nullptr;
lv_font_t* gUiFont14 = nullptr;

bool initUiFonts() {
  if (gUiFont12 != nullptr && gUiFont14 != nullptr) {
    return true;
  }

  if (gUiFont12 == nullptr) {
    gUiFont12 = lv_tiny_ttf_create_data(hitomi_ui_subset_ttf, hitomi_ui_subset_ttf_len, 12);
  }
  if (gUiFont14 == nullptr) {
    gUiFont14 = lv_tiny_ttf_create_data(hitomi_ui_subset_ttf, hitomi_ui_subset_ttf_len, 14);
  }

  if (gUiFont12 != nullptr && gUiFont14 != nullptr) {
    return true;
  }

  Serial.println("[LVGL] tiny_ttf UI font init failed; falling back to builtin font");
  if (gUiFont12 != nullptr) {
    lv_tiny_ttf_destroy(gUiFont12);
    gUiFont12 = nullptr;
  }
  if (gUiFont14 != nullptr) {
    lv_tiny_ttf_destroy(gUiFont14);
    gUiFont14 = nullptr;
  }
  return false;
}

void destroyUiFonts() {
  if (gUiFont12 != nullptr) {
    lv_tiny_ttf_destroy(gUiFont12);
    gUiFont12 = nullptr;
  }
  if (gUiFont14 != nullptr) {
    lv_tiny_ttf_destroy(gUiFont14);
    gUiFont14 = nullptr;
  }
}

DisplayPreviewPerfWindow& displayPreviewPerfWindow() {
  static DisplayPreviewPerfWindow window;
  return window;
}

void maybeLogDisplayPreviewPerf(uint32_t nowMs) {
  auto& window = displayPreviewPerfWindow();
  if (window.startedAtMs == 0) {
    window.startedAtMs = nowMs;
    return;
  }
  if (nowMs - window.startedAtMs < 1000) {
    return;
  }

  Serial.printf(
      "[PERF][DISPLAY] preview=%u skipped=%u resample_avg_us=%llu lvgl_avg_us=%llu\n",
      static_cast<unsigned>(window.previewUpdates),
      static_cast<unsigned>(window.skippedByPage),
      static_cast<unsigned long long>(
          window.previewUpdates == 0 ? 0ULL : window.resampleUs / window.previewUpdates),
      static_cast<unsigned long long>(
          window.previewUpdates == 0 ? 0ULL : window.lvglUs / window.previewUpdates));

  window = DisplayPreviewPerfWindow{.startedAtMs = nowMs};
}

void enrollTaskButtonEventCallback(lv_event_t* event);
void enrollRefreshButtonEventCallback(lv_event_t* event);
void captureCancelButtonEventCallback(lv_event_t* event);
bool enqueueCommand(LvglStatusDisplayData& data, DisplayCommand command);
bool enqueueNotification(LvglStatusDisplayData& data, DisplayNotification notification);

constexpr std::size_t toIndex(SidebarPage page) {
  return static_cast<std::size_t>(page);
}

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

const lv_font_t* uiLabelFont() {
  return gUiFont14 != nullptr ? gUiFont14 : &lv_font_source_han_sans_sc_14_cjk;
}

const lv_font_t* uiCaptionFont() {
  return gUiFont12 != nullptr ? gUiFont12 : &lv_font_source_han_sans_sc_14_cjk;
}

void applyLabelStyle(lv_obj_t* label, lv_text_align_t align, uint32_t colorHex = kTextColorHex) {
  lv_obj_set_style_text_font(label, uiLabelFont(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(colorHex), 0);
  lv_obj_set_style_text_align(label, align, 0);
}

void applyCaptionStyle(lv_obj_t* label, lv_text_align_t align, uint32_t colorHex = kMutedTextHex) {
  lv_obj_set_style_text_font(label, uiCaptionFont(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(colorHex), 0);
  lv_obj_set_style_text_align(label, align, 0);
}

void applyPanelStyle(
    lv_obj_t* panel,
    uint32_t bgHex,
    uint32_t borderHex = kPanelBorderMutedHex,
    lv_coord_t radius = 14,
    lv_coord_t borderWidth = 1) {
  lv_obj_set_style_bg_color(panel, lv_color_hex(bgHex), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(panel, borderWidth, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(borderHex), 0);
  lv_obj_set_style_radius(panel, radius, 0);
}

void applyCompactValueStyle(lv_obj_t* label, lv_text_align_t align, uint32_t colorHex = kTextColorHex) {
  lv_obj_set_style_text_font(label, uiCaptionFont(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(colorHex), 0);
  lv_obj_set_style_text_align(label, align, 0);
}

void applyNavLabelStyle(lv_obj_t* label, uint32_t colorHex = kTextColorHex) {
  lv_obj_set_style_text_font(label, uiCaptionFont(), 0);
  lv_obj_set_style_text_color(label, lv_color_hex(colorHex), 0);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
}

std::string lowerCopy(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

bool startsWith(const std::string& text, const char* prefix) {
  const std::string_view prefixView(prefix);
  return text.size() >= prefixView.size() && text.compare(0, prefixView.size(), prefixView.data()) == 0;
}

std::string stripPrefix(const std::string& text, const char* prefix) {
  return startsWith(text, prefix) ? text.substr(std::char_traits<char>::length(prefix)) : text;
}

StatusTone wifiToneFromLine(const std::string& line) {
  if (startsWith(line, "已连接") || startsWith(line, "Connected")) {
    return StatusTone::Good;
  }
  if (startsWith(line, "未连接") || startsWith(line, "Disconnected")) {
    return StatusTone::Error;
  }
  return StatusTone::Warn;
}

StatusTone syncToneFromLine(const std::string& line) {
  const std::string value = lowerCopy(line);
  if (line.find("已就绪") != std::string::npos || value.find("ready") != std::string::npos) {
    return StatusTone::Good;
  }
  return StatusTone::Warn;
}

StatusTone taskToneFromSummary(const std::string& line) {
  if (line == "暂无任务" || line == "No tasks") {
    return StatusTone::Neutral;
  }
  return StatusTone::Warn;
}

StatusTone apiToneFromLine(const std::string& line) {
  const std::string value = lowerCopy(line);
  if (line.find("可达") != std::string::npos || value.find("reachable") != std::string::npos) {
    return StatusTone::Good;
  }
  if (line.find("失败") != std::string::npos || value.find("failed") != std::string::npos) {
    return StatusTone::Error;
  }
  return StatusTone::Warn;
}

StatusTone credentialsToneFromLine(const std::string& line) {
  if (line == "缺失" || line == "Missing") {
    return StatusTone::Error;
  }
  if (line == "仅引导配置" || line == "Bootstrap only") {
    return StatusTone::Warn;
  }
  return StatusTone::Good;
}

StatusTone activationToneFromLine(const std::string& line) {
  const std::string value = lowerCopy(line);
  if (line.find("已激活") != std::string::npos || value.find("activated") != std::string::npos) {
    return StatusTone::Good;
  }
  return StatusTone::Warn;
}

StatusTone storageToneFromLine(const std::string& line) {
  const std::string value = lowerCopy(line);
  if (line.find("已就绪") != std::string::npos || value.find("ready") != std::string::npos) {
    return StatusTone::Good;
  }
  if (line.find("已禁用") != std::string::npos || value.find("disabled") != std::string::npos) {
    return StatusTone::Warn;
  }
  return StatusTone::Error;
}

StatusTone genericSystemTone(const std::string& line) {
  const std::string value = lowerCopy(line);
  if (line.empty() || line == "无" || value == "none") {
    return StatusTone::Neutral;
  }
  if (line.find("已就绪") != std::string::npos || line.find("已连接") != std::string::npos ||
      line.find("可达") != std::string::npos || line.find("张人脸") != std::string::npos ||
      line.find("已记录") != std::string::npos || value.find("ready") != std::string::npos ||
      value.find("linked") != std::string::npos || value.find("reachable") != std::string::npos ||
      value.find("face(s)") != std::string::npos) {
    return StatusTone::Good;
  }
  if (line.find("等待") != std::string::npos || line.find("待上报") != std::string::npos ||
      line.find("待认领") != std::string::npos || line.find("未启用") != std::string::npos ||
      line.find("已禁用") != std::string::npos || value.find("wait") != std::string::npos ||
      value.find("pending") != std::string::npos || value.find("inactive") != std::string::npos ||
      value.find("disabled") != std::string::npos) {
    return StatusTone::Warn;
  }
  if (line.find("失败") != std::string::npos || line.find("错误") != std::string::npos ||
      line.find("缺失") != std::string::npos || line.find("不可用") != std::string::npos ||
      value.find("fail") != std::string::npos || value.find("error") != std::string::npos ||
      value.find("missing") != std::string::npos || value.find("unavailable") != std::string::npos) {
    return StatusTone::Error;
  }
  return StatusTone::Neutral;
}

uint32_t toneLedHex(StatusTone tone) {
  switch (tone) {
    case StatusTone::Good:
      return kGoodHex;
    case StatusTone::Warn:
      return kWarnHex;
    case StatusTone::Error:
      return kErrorHex;
    case StatusTone::Neutral:
    default:
      return kMutedTextHex;
  }
}

uint32_t tonePanelHex(StatusTone tone) {
  switch (tone) {
    case StatusTone::Good:
      return 0x256F42;
    case StatusTone::Warn:
      return 0x77561D;
    case StatusTone::Error:
      return 0x6A3027;
    case StatusTone::Neutral:
    default:
      return kPanelMutedHex;
  }
}

uint32_t toneBorderHex(StatusTone tone) {
  switch (tone) {
    case StatusTone::Good:
      return 0x74D795;
    case StatusTone::Warn:
      return 0xF4C35C;
    case StatusTone::Error:
      return 0xF08D73;
    case StatusTone::Neutral:
    default:
      return kPanelBorderHex;
  }
}

void setLedTone(lv_obj_t* led, StatusTone tone) {
  if (led == nullptr) {
    return;
  }

  lv_led_set_color(led, lv_color_hex(toneLedHex(tone)));
  if (tone == StatusTone::Neutral) {
    lv_led_set_brightness(led, 80);
  } else {
    lv_led_set_brightness(led, 255);
  }
  lv_led_on(led);
}

std::string compactSyncValue(const std::string& line) {
  const std::string value = startsWith(line, "同步：") ? stripPrefix(line, "同步：") : stripPrefix(line, "Sync: ");
  if (value.find("已就绪") != std::string::npos || value.find("Ready") != std::string::npos) {
    return "已就绪";
  }
  if (value.find("同步中") != std::string::npos || value.find("Syncing") != std::string::npos) {
    return "同步中";
  }
  if (value.find("等待") != std::string::npos || value.find("Waiting") != std::string::npos) {
    return "等待";
  }
  if (value.find("未启用") != std::string::npos || value.find("Inactive") != std::string::npos) {
    return "未启用";
  }
  return value;
}

uint32_t notificationColorHex(DisplayNotificationLevel level) {
  switch (level) {
    case DisplayNotificationLevel::Success:
      return 0x2A9D50;
    case DisplayNotificationLevel::Warning:
      return 0xE9C46A;
    case DisplayNotificationLevel::Error:
      return 0xC44536;
    case DisplayNotificationLevel::Info:
    default:
      return 0x145A32;
  }
}

uint32_t notificationTextHex(DisplayNotificationLevel level) {
  switch (level) {
    case DisplayNotificationLevel::Warning:
      return kSidebarBackgroundHex;
    case DisplayNotificationLevel::Info:
    case DisplayNotificationLevel::Success:
    case DisplayNotificationLevel::Error:
    default:
      return kTextColorHex;
  }
}

void applyNavButtonStyle(lv_obj_t* button, bool active) {
  lv_obj_set_style_bg_color(button, lv_color_hex(active ? kNavButtonActiveHex : kNavButtonHex), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(
      button,
      lv_color_hex(active ? kAccentHex : kPanelBorderMutedHex),
      0);
  lv_obj_set_style_radius(button, 10, 0);

  lv_obj_t* label = lv_obj_get_child(button, 0);
  if (label != nullptr) {
    lv_obj_set_style_text_color(label, lv_color_hex(active ? kNavButtonActiveTextHex : kTextColorHex), 0);
  }
}

void applyActionButtonStyle(lv_obj_t* button, bool enabled) {
  lv_obj_set_style_bg_color(button, lv_color_hex(enabled ? kAccentHex : kPanelMutedHex), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, lv_color_hex(enabled ? kPanelBorderHex : kPanelBorderMutedHex), 0);
  lv_obj_set_style_radius(button, 12, 0);
  lv_obj_set_style_text_color(button, lv_color_hex(kSidebarBackgroundHex), 0);
}

void applyTaskButtonStyle(lv_obj_t* button, bool selected, bool enabled) {
  const uint32_t color = !enabled ? kPanelMutedHex : (selected ? kAccentHex : kPanelHex);
  lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(
      button,
      lv_color_hex(selected ? kPanelBorderHex : kPanelBorderMutedHex),
      0);
  lv_obj_set_style_radius(button, 12, 0);
}

lv_obj_t* createPageContainer(lv_obj_t* parent) {
  lv_obj_t* page = lv_obj_create(parent);
  lv_obj_remove_style_all(page);
  lv_obj_set_size(page, kPageWidth, kPageHeight);
  lv_obj_align(page, LV_ALIGN_TOP_LEFT, kPageX, kPageY);
  lv_obj_set_style_bg_color(page, lv_color_hex(kPaneBackgroundHex), 0);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(page, 0, 0);
  lv_obj_remove_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
  return page;
}

lv_obj_t* createPageLabel(lv_obj_t* parent, lv_coord_t y, lv_coord_t width, uint32_t colorHex = kTextColorHex) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_width(label, width);
  applyLabelStyle(label, LV_TEXT_ALIGN_LEFT, colorHex);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, y);
  return label;
}

StatusRow createSystemRow(lv_obj_t* parent, lv_coord_t y, const char* title) {
  StatusRow row = {};
  row.panel = lv_obj_create(parent);
  lv_obj_set_size(row.panel, kPageWidth, kSystemRowHeight);
  lv_obj_align(row.panel, LV_ALIGN_TOP_LEFT, 0, y);
  applyPanelStyle(row.panel, kPanelHex, kPanelBorderMutedHex, 12);
  lv_obj_set_style_pad_left(row.panel, 12, 0);
  lv_obj_set_style_pad_right(row.panel, 10, 0);
  lv_obj_set_style_pad_top(row.panel, 4, 0);
  lv_obj_set_style_pad_bottom(row.panel, 4, 0);
  lv_obj_remove_flag(row.panel, LV_OBJ_FLAG_SCROLLABLE);

  row.led = lv_led_create(row.panel);
  lv_obj_set_size(row.led, 8, 8);
  lv_obj_align(row.led, LV_ALIGN_LEFT_MID, 0, 0);
  setLedTone(row.led, StatusTone::Neutral);

  row.titleLabel = lv_label_create(row.panel);
  lv_obj_set_width(row.titleLabel, 34);
  applyCaptionStyle(row.titleLabel, LV_TEXT_ALIGN_LEFT);
  lv_label_set_text(row.titleLabel, title);
  lv_obj_align(row.titleLabel, LV_ALIGN_LEFT_MID, 14, 0);

  row.valueLabel = lv_label_create(row.panel);
  lv_obj_set_width(row.valueLabel, kPageWidth - 56);
  lv_label_set_long_mode(row.valueLabel, LV_LABEL_LONG_DOT);
  applyLabelStyle(row.valueLabel, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(row.valueLabel, LV_ALIGN_LEFT_MID, 48, 0);
  return row;
}

void setSystemRowValue(const StatusRow& row, const std::string& value, StatusTone tone) {
  if (row.panel != nullptr) {
    applyPanelStyle(row.panel, tonePanelHex(tone), toneBorderHex(tone), 12);
  }
  if (row.valueLabel != nullptr) {
    lv_label_set_text(row.valueLabel, value.c_str());
  }
  setLedTone(row.led, tone);
}

bool enqueueCommand(LvglStatusDisplayData& data, DisplayCommand command) {
  if (data.queuedCommandCount >= data.commandQueue.size()) {
    Serial.println("[DISPLAY] command queue full; dropping UI command");
    return false;
  }

  const std::size_t tail = (data.commandHead + data.queuedCommandCount) % data.commandQueue.size();
  data.commandQueue[tail] = std::move(command);
  data.queuedCommandCount += 1;
  return true;
}

bool enqueueNotification(LvglStatusDisplayData& data, DisplayNotification notification) {
  if (data.queuedNotificationCount >= data.notificationQueue.size()) {
    Serial.println("[DISPLAY] notification queue full; dropping toast");
    return false;
  }

  const std::size_t tail =
      (data.notificationHead + data.queuedNotificationCount) % data.notificationQueue.size();
  data.notificationQueue[tail] = std::move(notification);
  data.queuedNotificationCount += 1;
  return true;
}

void refreshNavButtons(const LvglStatusDisplayData& data) {
  for (std::size_t index = 0; index < data.navButtons.size(); index += 1) {
    if (data.navButtons[index] != nullptr) {
      applyNavButtonStyle(data.navButtons[index], kSidebarPages[index].page == data.currentPage);
    }
  }
}

void showCurrentPage(LvglStatusDisplayData& data) {
  for (std::size_t index = 0; index < data.pageContainers.size(); index += 1) {
    if (data.pageContainers[index] == nullptr) {
      continue;
    }

    if (index == toIndex(data.currentPage)) {
      lv_obj_remove_flag(data.pageContainers[index], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(data.pageContainers[index], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

std::size_t visibleEnrollmentTaskCount(const ui::AppViewModel& viewModel) {
  return viewModel.enrollmentTasks.size();
}

const ui::EnrollmentTaskItemViewModel* selectedEnrollmentTask(const LvglStatusDisplayData& data) {
  if (data.selectedEnrollmentTaskId.empty()) {
    return nullptr;
  }

  for (const auto& task : data.lastViewModel.enrollmentTasks) {
    if (task.taskId == data.selectedEnrollmentTaskId) {
      return &task;
    }
  }

  return nullptr;
}

StatusTone enrollSelectionTone(const LvglStatusDisplayData& data, bool hasTask) {
  if (!hasTask) {
    return taskToneFromSummary(data.lastViewModel.enrollmentTaskSummaryLine);
  }
  if (data.enrollmentRequestQueued) {
    return StatusTone::Warn;
  }
  if (selectedEnrollmentTask(data) != nullptr) {
    return StatusTone::Good;
  }
  return StatusTone::Neutral;
}

std::string enrollSelectionTitle(const LvglStatusDisplayData& data, bool hasTask) {
  const auto* task = selectedEnrollmentTask(data);
  if (task != nullptr) {
    return task->title;
  }
  if (hasTask) {
    return "选择任务";
  }
  return "暂无任务";
}

std::string enrollSelectionMeta(const LvglStatusDisplayData& data, bool hasTask) {
  const auto* task = selectedEnrollmentTask(data);
  if (task != nullptr) {
    if (data.enrollmentRequestQueued) {
      return "正在进入录脸";
    }
    return task->meta;
  }
  if (hasTask) {
    return "点选列表后开始";
  }
  return compactSyncValue(data.lastViewModel.syncLine);
}

std::string enrollSelectionFooter(const LvglStatusDisplayData& data) {
  if (data.lastViewModel.pendingQueueCount > 0) {
    return data.lastViewModel.queueLine;
  }
  return compactSyncValue(data.lastViewModel.syncLine);
}

std::string captureFaceStateText(const ui::AppViewModel& viewModel) {
  if (viewModel.captureDetectedFaceCount == 0) {
    return "无人脸";
  }
  if (viewModel.captureDetectedFaceCount == 1) {
    return "1 人脸";
  }
  return std::to_string(viewModel.captureDetectedFaceCount) + " 人脸";
}

StatusTone captureFaceTone(const ui::AppViewModel& viewModel) {
  if (viewModel.captureDetectedFaceCount == 1) {
    return StatusTone::Good;
  }
  if (viewModel.captureDetectedFaceCount == 0) {
    return StatusTone::Warn;
  }
  return StatusTone::Error;
}

void setCaptureSampleIndicators(LvglStatusDisplayData& data) {
  const std::size_t required = std::min<std::size_t>(
      data.lastViewModel.captureRequiredSamples,
      data.captureSampleDots.size());
  const std::size_t captured = std::min<std::size_t>(
      data.lastViewModel.captureCapturedSamples,
      required);

  for (std::size_t index = 0; index < data.captureSampleDots.size(); index += 1) {
    lv_obj_t* dot = data.captureSampleDots[index];
    if (dot == nullptr) {
      continue;
    }

    if (index >= required) {
      lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
      continue;
    }

    lv_obj_remove_flag(dot, LV_OBJ_FLAG_HIDDEN);
    const bool done = index < captured;
    const bool current = !done && index == captured && captured < required;
    applyPanelStyle(
        dot,
        done ? kGoodHex : (current ? kAccentHex : kPanelMutedHex),
        done ? kGoodHex : (current ? kAccentHex : kPanelBorderMutedHex),
        kCaptureSampleDotSize / 2,
        current ? 2 : 1);
  }
}

void refreshChrome(LvglStatusDisplayData& data) {
  if (data.sidebarTitleLabel != nullptr) {
    lv_label_set_text(data.sidebarTitleLabel, data.lastViewModel.title.c_str());
  }
  if (data.sidebarSubtitleLabel != nullptr) {
    lv_label_set_text(data.sidebarSubtitleLabel, data.lastViewModel.subtitle.c_str());
  }
  if (data.sidebarFooterLabel != nullptr) {
    lv_label_set_text(data.sidebarFooterLabel, data.lastViewModel.footer.c_str());
  }
}

void refreshHomePage(LvglStatusDisplayData& data) {
  lv_label_set_text(data.homePeriodLabel, data.lastViewModel.periodLine.c_str());
  lv_label_set_text(data.homeCameraLabel, data.lastViewModel.cameraHintLine.c_str());
  if (data.homeCameraFrame != nullptr) {
    const StatusTone cameraTone = genericSystemTone(data.lastViewModel.faceDetectLine);
    applyPanelStyle(
        data.homeCameraFrame,
        kCameraPlaceholderHex,
        cameraTone == StatusTone::Neutral ? kPanelBorderMutedHex : toneBorderHex(cameraTone),
        18);
  }
  if (data.homeCameraPreviewReady) {
    lv_obj_add_flag(data.homeCameraLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(data.homeCameraLabel, LV_ALIGN_BOTTOM_LEFT, 10, -8);
    lv_obj_set_style_text_align(data.homeCameraLabel, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_long_mode(data.homeCameraLabel, LV_LABEL_LONG_WRAP);
  } else {
    lv_obj_remove_flag(data.homeCameraLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_center(data.homeCameraLabel);
    lv_obj_set_style_text_align(data.homeCameraLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(data.homeCameraLabel, LV_LABEL_LONG_WRAP);
  }
}

void refreshEnrollPage(LvglStatusDisplayData& data) {
  const std::size_t taskCount = data.lastViewModel.enrollmentTasks.size();
  const std::size_t visibleCount = visibleEnrollmentTaskCount(data.lastViewModel);
  const bool hasTask = taskCount > 0;
  if (!hasTask || selectedEnrollmentTask(data) == nullptr) {
    data.selectedEnrollmentTaskId.clear();
    data.enrollmentRequestQueued = false;
  }

  lv_label_set_text(data.enrollSummaryLabel, data.lastViewModel.enrollmentTaskSummaryLine.c_str());
  if (data.enrollMetaLabel != nullptr) {
    const StatusTone syncTone = syncToneFromLine(data.lastViewModel.syncLine);
    lv_label_set_text(data.enrollMetaLabel, compactSyncValue(data.lastViewModel.syncLine).c_str());
    lv_obj_set_style_text_color(data.enrollMetaLabel, lv_color_hex(toneLedHex(syncTone)), 0);
  }

  if (data.enrollTaskList != nullptr) {
    lv_obj_clean(data.enrollTaskList);
    for (std::size_t index = 0; index < visibleCount; index += 1) {
      const auto& task = data.lastViewModel.enrollmentTasks[index];
      lv_obj_t* button = lv_button_create(data.enrollTaskList);
      lv_obj_set_size(button, kPageWidth - 6, kEnrollTaskHeight);
      lv_obj_set_style_radius(button, 14, 0);
      lv_obj_set_style_margin_bottom(button, kEnrollTaskGap, 0);
      lv_obj_set_style_pad_left(button, 10, 0);
      lv_obj_set_style_pad_right(button, 10, 0);
      lv_obj_set_style_pad_top(button, 6, 0);
      lv_obj_set_style_pad_bottom(button, 6, 0);
      lv_obj_add_event_cb(button, enrollTaskButtonEventCallback, LV_EVENT_CLICKED, &data);
      applyTaskButtonStyle(button, data.selectedEnrollmentTaskId == task.taskId, true);

      lv_obj_t* titleLabel = lv_label_create(button);
      applyLabelStyle(titleLabel, LV_TEXT_ALIGN_LEFT);
      lv_obj_set_width(titleLabel, kPageWidth - 26);
      lv_label_set_text(titleLabel, task.title.c_str());
      lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 0, 0);

      lv_obj_t* metaLabel = lv_label_create(button);
      applyCaptionStyle(metaLabel, LV_TEXT_ALIGN_LEFT);
      lv_obj_set_width(metaLabel, kPageWidth - 26);
      lv_label_set_text(metaLabel, task.meta.c_str());
      lv_obj_align(metaLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }
  }

  applyActionButtonStyle(data.enrollRefreshButton, true);
  applyActionButtonStyle(data.enrollStartButton, hasTask && selectedEnrollmentTask(data) != nullptr);

  const StatusTone selectionTone = enrollSelectionTone(data, hasTask);
  if (data.enrollSelectionCard != nullptr) {
    applyPanelStyle(data.enrollSelectionCard, tonePanelHex(selectionTone), toneBorderHex(selectionTone), 16);
  }
  setLedTone(data.enrollSelectionLed, selectionTone);
  if (data.enrollSelectionTitleLabel != nullptr) {
    lv_label_set_text(data.enrollSelectionTitleLabel, enrollSelectionTitle(data, hasTask).c_str());
  }
  if (data.enrollSelectionMetaLabel != nullptr) {
    const std::string detail = enrollSelectionMeta(data, hasTask) + "  |  " + enrollSelectionFooter(data);
    lv_label_set_text(data.enrollSelectionMetaLabel, detail.c_str());
  }
}

void refreshCapturePage(LvglStatusDisplayData& data) {
  if (data.captureHintLabel != nullptr) {
    lv_label_set_text(data.captureHintLabel, data.lastViewModel.captureTitleLine.c_str());
  }
  if (data.captureFaceStateLabel != nullptr) {
    const StatusTone faceTone = captureFaceTone(data.lastViewModel);
    lv_label_set_text(data.captureFaceStateLabel, captureFaceStateText(data.lastViewModel).c_str());
    lv_obj_set_style_text_color(data.captureFaceStateLabel, lv_color_hex(toneLedHex(faceTone)), 0);
  }
  if (data.captureStatusLabel != nullptr) {
    lv_label_set_text(data.captureStatusLabel, data.lastViewModel.captureStatusLine.c_str());
  }
  if (data.captureProgressLabel != nullptr) {
    lv_label_set_text(data.captureProgressLabel, data.lastViewModel.captureProgressLine.c_str());
  }
  setCaptureSampleIndicators(data);
  if (data.captureProgressBar != nullptr) {
    if (data.lastViewModel.captureRequiredSamples == 0) {
      lv_obj_add_flag(data.captureProgressBar, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_remove_flag(data.captureProgressBar, LV_OBJ_FLAG_HIDDEN);
      lv_bar_set_value(data.captureProgressBar, data.lastViewModel.captureProgressPercent, LV_ANIM_OFF);
    }
  }
  if (data.captureCancelButton != nullptr) {
    applyActionButtonStyle(data.captureCancelButton, true);
    if (lv_obj_t* label = lv_obj_get_child(data.captureCancelButton, 0); label != nullptr) {
      lv_label_set_text(label, data.lastViewModel.captureActionLabel.c_str());
    }
  }

  if (data.capturePreviewFrame != nullptr) {
    applyPanelStyle(
        data.capturePreviewFrame,
        kCameraPlaceholderHex,
        toneBorderHex(genericSystemTone(data.lastViewModel.captureStatusLine)),
        18);
  }
}

void refreshSystemPage(LvglStatusDisplayData& data) {
  const StatusTone activationTone = activationToneFromLine(data.lastViewModel.activationLine);
  const StatusTone syncTone = syncToneFromLine(data.lastViewModel.syncLine);
  if (data.systemSummaryCard != nullptr) {
    const StatusTone summaryTone =
        activationTone == StatusTone::Good && syncTone == StatusTone::Good ? StatusTone::Good : StatusTone::Warn;
    applyPanelStyle(data.systemSummaryCard, tonePanelHex(summaryTone), toneBorderHex(summaryTone), 16);
  }
  if (data.systemActivationLabel != nullptr) {
    lv_label_set_text(data.systemActivationLabel, data.lastViewModel.activationLine.c_str());
    lv_obj_set_style_text_color(data.systemActivationLabel, lv_color_hex(toneLedHex(activationTone)), 0);
  }
  if (data.systemSyncLabel != nullptr) {
    lv_label_set_text(data.systemSyncLabel, data.lastViewModel.syncLine.c_str());
    lv_obj_set_style_text_color(data.systemSyncLabel, lv_color_hex(toneLedHex(syncTone)), 0);
  }
  if (data.systemFooterLabel != nullptr) {
    const std::string footer = data.lastViewModel.subtitle + " | " + data.lastViewModel.footer;
    lv_label_set_text(data.systemFooterLabel, footer.c_str());
  }

  setSystemRowValue(data.systemRows[0], data.lastViewModel.credentialsLine, credentialsToneFromLine(data.lastViewModel.credentialsLine));
  setSystemRowValue(data.systemRows[1], data.lastViewModel.wifiLine, wifiToneFromLine(data.lastViewModel.wifiLine));
  setSystemRowValue(data.systemRows[2], data.lastViewModel.apiLine, apiToneFromLine(data.lastViewModel.apiLine));
  setSystemRowValue(data.systemRows[3], data.lastViewModel.faceLine, genericSystemTone(data.lastViewModel.faceLine));
  setSystemRowValue(data.systemRows[4], data.lastViewModel.faceDetectLine, genericSystemTone(data.lastViewModel.faceDetectLine));
  setSystemRowValue(data.systemRows[5], data.lastViewModel.storageLine, storageToneFromLine(data.lastViewModel.storageLine));
  setSystemRowValue(data.systemRows[6], data.lastViewModel.errorLine, genericSystemTone(data.lastViewModel.errorLine));
}

void refreshCurrentPage(LvglStatusDisplayData& data) {
  switch (data.currentPage) {
    case SidebarPage::Home:
      refreshHomePage(data);
      break;
    case SidebarPage::Enroll:
      refreshEnrollPage(data);
      break;
    case SidebarPage::Capture:
      refreshCapturePage(data);
      break;
    case SidebarPage::System:
      refreshSystemPage(data);
      break;
    default:
      break;
  }
}

void refreshUi(LvglStatusDisplayData& data) {
  if (!data.hasViewModel) {
    return;
  }

  refreshChrome(data);
  refreshNavButtons(data);

  if (data.lastViewModel.captureActive && data.currentPage != SidebarPage::Capture) {
    data.currentPage = SidebarPage::Capture;
  } else if (!data.lastViewModel.captureActive && data.currentPage == SidebarPage::Capture) {
    data.currentPage = SidebarPage::Enroll;
  }

  refreshCurrentPage(data);
  showCurrentPage(data);
  data.previewNeedsPresent =
      data.homeCameraPreviewReady &&
      (data.currentPage == SidebarPage::Home || data.currentPage == SidebarPage::Capture);
}

struct PreviewCropRect {
  uint32_t cropWidth = 0;
  uint32_t cropHeight = 0;
  uint32_t cropX = 0;
  uint32_t cropY = 0;
  bool usesScale = false;
};
PreviewCropRect computePreviewCrop(
    uint16_t sourceWidth,
    uint16_t sourceHeight,
    lv_coord_t targetWidth,
    lv_coord_t targetHeight);
void updateFaceBoxOverlays(
    const DisplayRgb565Frame& frame,
    const PreviewCropRect& crop,
    lv_coord_t targetWidth,
    lv_coord_t targetHeight,
    const std::array<lv_obj_t*, DisplayRgb565Frame::kMaxFaceBoxes>& overlays);

void presentPreview(LvglStatusDisplayData& data) {
  if (!data.homeCameraPreviewReady || data.latestPreviewFrame.data == nullptr) {
    return;
  }

  lv_obj_t* targetImage = data.homeCameraImage;
  auto overlays = data.homeFaceBoxOverlays;
  if (data.currentPage == SidebarPage::Capture) {
    targetImage = data.capturePreviewImage;
    overlays = data.captureFaceBoxOverlays;
  } else if (data.currentPage != SidebarPage::Home) {
    return;
  }

  const PreviewBuffer& activeBuffer = data.previewBuffers[data.activePreviewBufferIndex];
  if (activeBuffer.pixels.empty() || activeBuffer.width <= 0 || activeBuffer.height <= 0) {
    return;
  }

  const auto crop = computePreviewCrop(
      data.latestPreviewFrame.width,
      data.latestPreviewFrame.height,
      activeBuffer.width,
      activeBuffer.height);

  if (targetImage != nullptr) {
    data.previewImageDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    data.previewImageDsc.header.cf = LV_COLOR_FORMAT_RGB565;
    data.previewImageDsc.header.flags = 0;
    data.previewImageDsc.header.w = static_cast<uint32_t>(activeBuffer.width);
    data.previewImageDsc.header.h = static_cast<uint32_t>(activeBuffer.height);
    data.previewImageDsc.header.stride = static_cast<uint32_t>(activeBuffer.width) * sizeof(uint16_t);
    data.previewImageDsc.data_size = static_cast<uint32_t>(activeBuffer.pixels.size() * sizeof(uint16_t));
    data.previewImageDsc.data = reinterpret_cast<const uint8_t*>(activeBuffer.pixels.data());
    data.previewImageDsc.reserved = nullptr;
    data.previewImageDsc.reserved_2 = nullptr;

    lv_image_set_src(targetImage, &data.previewImageDsc);
    lv_obj_clear_flag(targetImage, LV_OBJ_FLAG_HIDDEN);
    updateFaceBoxOverlays(data.latestPreviewFrame, crop, activeBuffer.width, activeBuffer.height, overlays);
    data.previewNeedsPresent = false;
    data.lastPreviewPresentMs = millis();
  }

  if (!data.homeCameraPreviewReady || targetImage == nullptr) {
    for (auto* overlay : overlays) {
      if (overlay != nullptr) {
        lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
}

void sidebarButtonEventCallback(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  auto* binding = static_cast<SidebarBinding*>(lv_event_get_user_data(event));
  if (binding == nullptr || binding->owner == nullptr) {
    return;
  }

  binding->owner->currentPage = binding->page;
  refreshUi(*binding->owner);
}

void enrollTaskButtonEventCallback(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  auto* data = static_cast<LvglStatusDisplayData*>(lv_event_get_user_data(event));
  if (data == nullptr || !data->hasViewModel) {
    return;
  }

  lv_obj_t* button = static_cast<lv_obj_t*>(lv_event_get_target(event));
  if (button == nullptr) {
    return;
  }

  const int buttonIndex = lv_obj_get_index(button);
  if (buttonIndex < 0) {
    return;
  }

  const std::size_t index = static_cast<std::size_t>(buttonIndex);
  if (index >= data->lastViewModel.enrollmentTasks.size()) {
    return;
  }

  const auto& task = data->lastViewModel.enrollmentTasks[index];
  data->selectedEnrollmentTaskId = data->selectedEnrollmentTaskId == task.taskId ? "" : task.taskId;
  data->enrollmentRequestQueued = false;
  refreshEnrollPage(*data);
}

void enrollRefreshButtonEventCallback(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  auto* data = static_cast<LvglStatusDisplayData*>(lv_event_get_user_data(event));
  if (data == nullptr) {
    return;
  }

  if (!enqueueCommand(*data, DisplayCommand{
                               .type = DisplayCommandType::RefreshData,
                               .targetId = "",
                           })) {
    enqueueNotification(*data, DisplayNotification{
                                   .level = DisplayNotificationLevel::Warning,
                                   .text = "忙碌中，请重试",
                                   .durationMs = 2200,
                               });
    return;
  }

  data->enrollmentRequestQueued = false;
  enqueueNotification(*data, DisplayNotification{
                                 .level = DisplayNotificationLevel::Info,
                                 .text = "刷新中...",
                                 .durationMs = 1800,
                             });
  refreshEnrollPage(*data);
}

void enrollStartButtonEventCallback(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  auto* data = static_cast<LvglStatusDisplayData*>(lv_event_get_user_data(event));
  if (data == nullptr || !data->hasViewModel || selectedEnrollmentTask(*data) == nullptr) {
    return;
  }

  const auto* task = selectedEnrollmentTask(*data);
  if (task == nullptr) {
    return;
  }

  if (!enqueueCommand(*data, DisplayCommand{
                               .type = DisplayCommandType::StartEnrollmentTask,
                               .targetId = task->taskId,
                           })) {
    enqueueNotification(*data, DisplayNotification{
                                   .level = DisplayNotificationLevel::Warning,
                                   .text = "忙碌中，请重试",
                                   .durationMs = 2200,
                               });
    return;
  }

  data->enrollmentRequestQueued = true;
  enqueueNotification(*data, DisplayNotification{
                                 .level = DisplayNotificationLevel::Info,
                                 .text = "已开始采集",
                                 .durationMs = 2000,
                             });
  refreshEnrollPage(*data);
  data->currentPage = SidebarPage::Capture;
  refreshUi(*data);
}

void captureCancelButtonEventCallback(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }

  auto* data = static_cast<LvglStatusDisplayData*>(lv_event_get_user_data(event));
  if (data == nullptr || !data->hasViewModel || !data->lastViewModel.captureActive) {
    return;
  }

  const DisplayCommand command = data->lastViewModel.captureRunning
      ? DisplayCommand{
            .type = DisplayCommandType::CancelEnrollment,
            .targetId = "",
        }
      : DisplayCommand{
            .type = DisplayCommandType::DismissCapture,
            .targetId = "",
        };

  if (!enqueueCommand(*data, command)) {
    enqueueNotification(*data, DisplayNotification{
                                   .level = DisplayNotificationLevel::Warning,
                                   .text = "忙碌中，请重试",
                                   .durationMs = 1800,
                               });
    return;
  }

  enqueueNotification(*data, DisplayNotification{
                                 .level = data->lastViewModel.captureRunning ? DisplayNotificationLevel::Warning
                                                                            : DisplayNotificationLevel::Info,
                                 .text = data->lastViewModel.captureRunning ? "取消中..."
                                                                           : "返回任务列表...",
                                 .durationMs = 1600,
                             });
}

void createSidebarButton(LvglStatusDisplayData& data, lv_obj_t* parent, const SidebarPageSpec& spec, std::size_t index) {
  data.navBindings[index] = SidebarBinding{
      .owner = &data,
      .page = spec.page,
  };

  lv_obj_t* button = lv_button_create(parent);
  data.navButtons[index] = button;
  lv_obj_set_size(button, kNavButtonWidth, kNavButtonHeight);
  lv_obj_align(button, LV_ALIGN_TOP_MID, 0, kNavStartY + static_cast<lv_coord_t>(index) * kNavStepY);
  lv_obj_add_event_cb(button, sidebarButtonEventCallback, LV_EVENT_CLICKED, &data.navBindings[index]);

  lv_obj_t* label = lv_label_create(button);
  lv_obj_set_width(label, kNavButtonWidth - 8);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  applyNavLabelStyle(label);
  lv_label_set_text(label, spec.label);
  lv_obj_center(label);
}

void createHomePage(LvglStatusDisplayData& data, lv_obj_t* parent) {
  lv_obj_t* periodChip = lv_obj_create(parent);
  lv_obj_set_size(periodChip, kPageWidth, kHomePeriodHeight);
  lv_obj_align(periodChip, LV_ALIGN_TOP_LEFT, 0, kHomePeriodY);
  applyPanelStyle(periodChip, kAccentHex, kPanelBorderHex, 12);
  lv_obj_remove_flag(periodChip, LV_OBJ_FLAG_SCROLLABLE);
  data.homePeriodLabel = lv_label_create(periodChip);
  lv_obj_set_width(data.homePeriodLabel, kPageWidth - 16);
  lv_label_set_long_mode(data.homePeriodLabel, LV_LABEL_LONG_DOT);
  applyLabelStyle(data.homePeriodLabel, LV_TEXT_ALIGN_CENTER, kSidebarBackgroundHex);
  lv_obj_center(data.homePeriodLabel);

  lv_obj_t* cameraFrame = lv_obj_create(parent);
  data.homeCameraFrame = cameraFrame;
  lv_obj_set_size(cameraFrame, kPageWidth, kHomeCameraHeight);
  lv_obj_align(cameraFrame, LV_ALIGN_TOP_LEFT, 0, kHomeCameraY);
  applyPanelStyle(cameraFrame, kCameraPlaceholderHex, kPanelBorderMutedHex, 18);
  lv_obj_set_style_pad_all(cameraFrame, 0, 0);
  lv_obj_remove_flag(cameraFrame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(cameraFrame, LV_SCROLLBAR_MODE_OFF);
  data.homeCameraImage = lv_image_create(cameraFrame);
  lv_obj_set_size(data.homeCameraImage, kPageWidth, kHomeCameraHeight);
  lv_obj_align(data.homeCameraImage, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_flag(data.homeCameraImage, LV_OBJ_FLAG_HIDDEN);
  for (auto& overlay : data.homeFaceBoxOverlays) {
    overlay = lv_obj_create(cameraFrame);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(overlay, 2, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  }
  data.homeCameraLabel = lv_label_create(cameraFrame);
  lv_obj_set_width(data.homeCameraLabel, kPageWidth - 24);
  lv_label_set_long_mode(data.homeCameraLabel, LV_LABEL_LONG_WRAP);
  applyLabelStyle(data.homeCameraLabel, LV_TEXT_ALIGN_CENTER, kMutedTextHex);
  lv_obj_center(data.homeCameraLabel);
}

void createEnrollPage(LvglStatusDisplayData& data, lv_obj_t* parent) {
  lv_obj_t* header = lv_label_create(parent);
  applyCaptionStyle(header, LV_TEXT_ALIGN_LEFT);
  lv_label_set_text(header, "录脸任务");
  lv_obj_set_width(header, kPageWidth);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, kEnrollHeaderY);

  data.enrollSummaryLabel = createPageLabel(parent, kEnrollSummaryY, kPageWidth - 84, kMutedTextHex);
  lv_label_set_long_mode(data.enrollSummaryLabel, LV_LABEL_LONG_DOT);
  data.enrollMetaLabel = lv_label_create(parent);
  lv_obj_set_width(data.enrollMetaLabel, 80);
  lv_label_set_long_mode(data.enrollMetaLabel, LV_LABEL_LONG_DOT);
  applyCaptionStyle(data.enrollMetaLabel, LV_TEXT_ALIGN_RIGHT);
  lv_obj_align(data.enrollMetaLabel, LV_ALIGN_TOP_RIGHT, 0, kEnrollMetaY);
  data.enrollTaskList = lv_obj_create(parent);
  lv_obj_set_size(data.enrollTaskList, kPageWidth + 6, kEnrollTaskListHeight);
  lv_obj_align(data.enrollTaskList, LV_ALIGN_TOP_LEFT, 0, kEnrollTaskY);
  applyPanelStyle(data.enrollTaskList, kPanelMutedHex, kPanelBorderMutedHex, 16);
  lv_obj_set_style_pad_all(data.enrollTaskList, 0, 0);
  lv_obj_set_style_pad_right(data.enrollTaskList, 6, 0);
  lv_obj_set_scroll_dir(data.enrollTaskList, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(data.enrollTaskList, LV_SCROLLBAR_MODE_AUTO);

  data.enrollSelectionCard = lv_obj_create(parent);
  lv_obj_set_size(data.enrollSelectionCard, kPageWidth, kEnrollDetailHeight);
  lv_obj_align(data.enrollSelectionCard, LV_ALIGN_TOP_LEFT, 0, kEnrollDetailY);
  applyPanelStyle(data.enrollSelectionCard, kPanelMutedHex, kPanelBorderHex, 16);
  lv_obj_set_style_pad_left(data.enrollSelectionCard, 10, 0);
  lv_obj_set_style_pad_right(data.enrollSelectionCard, 10, 0);
  lv_obj_set_style_pad_top(data.enrollSelectionCard, 4, 0);
  lv_obj_set_style_pad_bottom(data.enrollSelectionCard, 4, 0);
  lv_obj_remove_flag(data.enrollSelectionCard, LV_OBJ_FLAG_SCROLLABLE);

  data.enrollSelectionLed = lv_led_create(data.enrollSelectionCard);
  lv_obj_set_size(data.enrollSelectionLed, 10, 10);
  lv_obj_align(data.enrollSelectionLed, LV_ALIGN_TOP_LEFT, 0, 2);

  data.enrollSelectionTitleLabel = lv_label_create(data.enrollSelectionCard);
  lv_obj_set_width(data.enrollSelectionTitleLabel, kPageWidth - 34);
  lv_label_set_long_mode(data.enrollSelectionTitleLabel, LV_LABEL_LONG_DOT);
  applyCompactValueStyle(data.enrollSelectionTitleLabel, LV_TEXT_ALIGN_LEFT, kTextColorHex);
  lv_obj_align(data.enrollSelectionTitleLabel, LV_ALIGN_TOP_LEFT, 18, 0);

  data.enrollSelectionMetaLabel = lv_label_create(data.enrollSelectionCard);
  lv_obj_set_width(data.enrollSelectionMetaLabel, kPageWidth - 20);
  lv_label_set_long_mode(data.enrollSelectionMetaLabel, LV_LABEL_LONG_DOT);
  applyCaptionStyle(data.enrollSelectionMetaLabel, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(data.enrollSelectionMetaLabel, LV_ALIGN_TOP_LEFT, 0, 18);

  const lv_coord_t actionButtonWidth = (kPageWidth - 20 - kEnrollActionGap) / 2;

  data.enrollRefreshButton = lv_button_create(data.enrollSelectionCard);
  lv_obj_set_size(data.enrollRefreshButton, actionButtonWidth, kEnrollActionHeight);
  lv_obj_align(data.enrollRefreshButton, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_add_event_cb(data.enrollRefreshButton, enrollRefreshButtonEventCallback, LV_EVENT_CLICKED, &data);
  lv_obj_set_style_radius(data.enrollRefreshButton, 14, 0);
  lv_obj_t* refreshLabel = lv_label_create(data.enrollRefreshButton);
  applyCompactValueStyle(refreshLabel, LV_TEXT_ALIGN_CENTER, kSidebarBackgroundHex);
  lv_label_set_text(refreshLabel, "刷新");
  lv_obj_center(refreshLabel);

  data.enrollStartButton = lv_button_create(data.enrollSelectionCard);
  lv_obj_set_size(data.enrollStartButton, actionButtonWidth, kEnrollActionHeight);
  lv_obj_align(data.enrollStartButton, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(data.enrollStartButton, enrollStartButtonEventCallback, LV_EVENT_CLICKED, &data);
  lv_obj_set_style_radius(data.enrollStartButton, 14, 0);
  lv_obj_t* startLabel = lv_label_create(data.enrollStartButton);
  applyCompactValueStyle(startLabel, LV_TEXT_ALIGN_CENTER, kSidebarBackgroundHex);
  lv_label_set_text(startLabel, "开始");
  lv_obj_center(startLabel);
}

void createCapturePage(LvglStatusDisplayData& data, lv_obj_t* parent) {
  lv_obj_t* header = lv_label_create(parent);
  applyCaptionStyle(header, LV_TEXT_ALIGN_LEFT);
  lv_label_set_text(header, "采集");
  lv_obj_set_width(header, kPageWidth);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, kCaptureHeaderY);

  lv_obj_t* previewFrame = lv_obj_create(parent);
  data.capturePreviewFrame = previewFrame;
  lv_obj_set_size(previewFrame, kPageWidth, kCapturePreviewHeight);
  lv_obj_align(previewFrame, LV_ALIGN_TOP_LEFT, 0, kCapturePreviewY);
  applyPanelStyle(previewFrame, kCameraPlaceholderHex, kPanelBorderMutedHex, 18);
  lv_obj_set_style_pad_all(previewFrame, 0, 0);
  lv_obj_remove_flag(previewFrame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(previewFrame, LV_SCROLLBAR_MODE_OFF);

  data.capturePreviewImage = lv_image_create(previewFrame);
  lv_obj_set_size(data.capturePreviewImage, kPageWidth, kCapturePreviewHeight);
  lv_obj_align(data.capturePreviewImage, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_flag(data.capturePreviewImage, LV_OBJ_FLAG_HIDDEN);
  for (auto& overlay : data.captureFaceBoxOverlays) {
    overlay = lv_obj_create(previewFrame);
    lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(overlay, 2, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  }

  data.captureHintLabel = lv_label_create(previewFrame);
  lv_obj_set_width(data.captureHintLabel, kPageWidth - 24);
  lv_label_set_long_mode(data.captureHintLabel, LV_LABEL_LONG_WRAP);
  applyLabelStyle(data.captureHintLabel, LV_TEXT_ALIGN_CENTER, kMutedTextHex);
  lv_obj_align(data.captureHintLabel, LV_ALIGN_TOP_MID, 0, 10);

  data.captureFaceStateLabel = lv_label_create(previewFrame);
  lv_obj_set_width(data.captureFaceStateLabel, kCaptureFacePillWidth);
  lv_label_set_long_mode(data.captureFaceStateLabel, LV_LABEL_LONG_DOT);
  applyCaptionStyle(data.captureFaceStateLabel, LV_TEXT_ALIGN_RIGHT, kMutedTextHex);
  lv_obj_align(data.captureFaceStateLabel, LV_ALIGN_TOP_RIGHT, -10, 10);

  const lv_coord_t sampleStripWidth =
      static_cast<lv_coord_t>(kCaptureSampleIndicatorCount) * kCaptureSampleDotSize +
      static_cast<lv_coord_t>(kCaptureSampleIndicatorCount - 1) * kCaptureSampleDotGap;
  const lv_coord_t sampleStartX = (kPageWidth - sampleStripWidth) / 2;
  for (std::size_t index = 0; index < data.captureSampleDots.size(); index += 1) {
    data.captureSampleDots[index] = lv_obj_create(previewFrame);
    lv_obj_set_size(data.captureSampleDots[index], kCaptureSampleDotSize, kCaptureSampleDotSize);
    lv_obj_align(
        data.captureSampleDots[index],
        LV_ALIGN_BOTTOM_LEFT,
        sampleStartX + static_cast<lv_coord_t>(index) * (kCaptureSampleDotSize + kCaptureSampleDotGap),
        -10);
    applyPanelStyle(
        data.captureSampleDots[index],
        kPanelMutedHex,
        kPanelBorderMutedHex,
        kCaptureSampleDotSize / 2);
    lv_obj_remove_flag(data.captureSampleDots[index], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(data.captureSampleDots[index], LV_OBJ_FLAG_HIDDEN);
  }

  data.captureStatusLabel = createPageLabel(parent, kCaptureStatusY, kPageWidth, kTextColorHex);
  lv_label_set_long_mode(data.captureStatusLabel, LV_LABEL_LONG_WRAP);

  data.captureProgressLabel = createPageLabel(parent, kCaptureProgressY, kPageWidth, kMutedTextHex);
  lv_label_set_long_mode(data.captureProgressLabel, LV_LABEL_LONG_WRAP);

  data.captureProgressBar = lv_bar_create(parent);
  lv_obj_set_size(data.captureProgressBar, kPageWidth, kCaptureBarHeight);
  lv_obj_align(data.captureProgressBar, LV_ALIGN_TOP_LEFT, 0, kCaptureBarY);
  lv_bar_set_range(data.captureProgressBar, 0, 100);
  lv_obj_set_style_bg_color(data.captureProgressBar, lv_color_hex(kPanelMutedHex), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(data.captureProgressBar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(data.captureProgressBar, lv_color_hex(kAccentHex), LV_PART_INDICATOR);
  lv_obj_set_style_radius(data.captureProgressBar, 10, LV_PART_MAIN);
  lv_obj_set_style_radius(data.captureProgressBar, 10, LV_PART_INDICATOR);
  lv_obj_add_flag(data.captureProgressBar, LV_OBJ_FLAG_HIDDEN);

  data.captureCancelButton = lv_button_create(parent);
  lv_obj_set_size(data.captureCancelButton, kPageWidth, kCaptureActionHeight);
  lv_obj_align(data.captureCancelButton, LV_ALIGN_TOP_LEFT, 0, kCaptureActionY);
  lv_obj_add_event_cb(data.captureCancelButton, captureCancelButtonEventCallback, LV_EVENT_CLICKED, &data);
  lv_obj_set_style_radius(data.captureCancelButton, 14, 0);
  lv_obj_t* cancelLabel = lv_label_create(data.captureCancelButton);
  applyCompactValueStyle(cancelLabel, LV_TEXT_ALIGN_CENTER, kSidebarBackgroundHex);
  lv_label_set_text(cancelLabel, "取消");
  lv_obj_center(cancelLabel);
}

void createSystemPage(LvglStatusDisplayData& data, lv_obj_t* parent) {
  data.systemSummaryCard = lv_obj_create(parent);
  lv_obj_set_size(data.systemSummaryCard, kPageWidth, kSystemSummaryHeight);
  lv_obj_align(data.systemSummaryCard, LV_ALIGN_TOP_LEFT, 0, kSystemSummaryY);
  applyPanelStyle(data.systemSummaryCard, kPanelMutedHex, kPanelBorderHex, 16);
  lv_obj_set_style_pad_left(data.systemSummaryCard, 8, 0);
  lv_obj_set_style_pad_right(data.systemSummaryCard, 8, 0);
  lv_obj_set_style_pad_top(data.systemSummaryCard, 6, 0);
  lv_obj_set_style_pad_bottom(data.systemSummaryCard, 6, 0);
  lv_obj_remove_flag(data.systemSummaryCard, LV_OBJ_FLAG_SCROLLABLE);

  data.systemActivationLabel = lv_label_create(data.systemSummaryCard);
  lv_obj_set_width(data.systemActivationLabel, kPageWidth - 20);
  lv_label_set_long_mode(data.systemActivationLabel, LV_LABEL_LONG_DOT);
  applyCaptionStyle(data.systemActivationLabel, LV_TEXT_ALIGN_LEFT, kTextColorHex);
  lv_obj_align(data.systemActivationLabel, LV_ALIGN_TOP_LEFT, 0, 0);

  data.systemSyncLabel = lv_label_create(data.systemSummaryCard);
  lv_obj_set_width(data.systemSyncLabel, kPageWidth - 20);
  lv_label_set_long_mode(data.systemSyncLabel, LV_LABEL_LONG_DOT);
  applyLabelStyle(data.systemSyncLabel, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(data.systemSyncLabel, LV_ALIGN_TOP_LEFT, 0, 18);

  data.systemFooterLabel = lv_label_create(data.systemSummaryCard);
  lv_obj_set_width(data.systemFooterLabel, kPageWidth - 20);
  lv_label_set_long_mode(data.systemFooterLabel, LV_LABEL_LONG_DOT);
  applyCaptionStyle(data.systemFooterLabel, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(data.systemFooterLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  data.systemRows[0] = createSystemRow(parent, kSystemRowsY, "凭据");
  data.systemRows[1] = createSystemRow(parent, kSystemRowsY + (kSystemRowHeight + kSystemRowGap), "网络");
  data.systemRows[2] = createSystemRow(parent, kSystemRowsY + (kSystemRowHeight + kSystemRowGap) * 2, "接口");
  data.systemRows[3] = createSystemRow(parent, kSystemRowsY + (kSystemRowHeight + kSystemRowGap) * 3, "识别");
  data.systemRows[4] = createSystemRow(parent, kSystemRowsY + (kSystemRowHeight + kSystemRowGap) * 4, "检测");
  data.systemRows[5] = createSystemRow(parent, kSystemRowsY + (kSystemRowHeight + kSystemRowGap) * 5, "存储");
  data.systemRows[6] = createSystemRow(parent, kSystemRowsY + (kSystemRowHeight + kSystemRowGap) * 6, "错误");
}

void createUi(LvglStatusDisplayData& data) {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(kPaneBackgroundHex), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);

  lv_obj_t* sidebar = lv_obj_create(screen);
  lv_obj_remove_style_all(sidebar);
  lv_obj_set_size(sidebar, kSidebarWidth, SzpiLvglDisplay::kVerticalResolution);
  lv_obj_align(sidebar, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_bg_color(sidebar, lv_color_hex(kSidebarBackgroundHex), 0);
  lv_obj_set_style_bg_opa(sidebar, LV_OPA_COVER, 0);
  lv_obj_remove_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(sidebar, LV_SCROLLBAR_MODE_OFF);

  data.sidebarTitleLabel = lv_label_create(sidebar);
  lv_obj_set_width(data.sidebarTitleLabel, kSidebarWidth - 8);
  lv_label_set_long_mode(data.sidebarTitleLabel, LV_LABEL_LONG_WRAP);
  applyLabelStyle(data.sidebarTitleLabel, LV_TEXT_ALIGN_CENTER);
  lv_obj_align(data.sidebarTitleLabel, LV_ALIGN_TOP_MID, 0, kSidebarTitleY);

  data.sidebarSubtitleLabel = lv_label_create(sidebar);
  lv_obj_set_width(data.sidebarSubtitleLabel, kSidebarWidth - 10);
  lv_label_set_long_mode(data.sidebarSubtitleLabel, LV_LABEL_LONG_DOT);
  applyCaptionStyle(data.sidebarSubtitleLabel, LV_TEXT_ALIGN_CENTER);
  lv_obj_align(data.sidebarSubtitleLabel, LV_ALIGN_TOP_MID, 0, kSidebarSubtitleY);

  data.sidebarFooterLabel = lv_label_create(sidebar);
  lv_obj_set_width(data.sidebarFooterLabel, kSidebarWidth - 10);
  lv_label_set_long_mode(data.sidebarFooterLabel, LV_LABEL_LONG_DOT);
  applyCaptionStyle(data.sidebarFooterLabel, LV_TEXT_ALIGN_CENTER);
  lv_obj_align(data.sidebarFooterLabel, LV_ALIGN_BOTTOM_MID, 0, kSidebarFooterY);

  for (std::size_t index = 0; index < kSidebarPages.size(); index += 1) {
    createSidebarButton(data, sidebar, kSidebarPages[index], index);
  }

  data.pageContainers[toIndex(SidebarPage::Home)] = createPageContainer(screen);
  createHomePage(data, data.pageContainers[toIndex(SidebarPage::Home)]);

  data.pageContainers[toIndex(SidebarPage::Enroll)] = createPageContainer(screen);
  createEnrollPage(data, data.pageContainers[toIndex(SidebarPage::Enroll)]);

  data.pageContainers[toIndex(SidebarPage::Capture)] = createPageContainer(screen);
  createCapturePage(data, data.pageContainers[toIndex(SidebarPage::Capture)]);

  data.pageContainers[toIndex(SidebarPage::System)] = createPageContainer(screen);
  lv_obj_add_flag(data.pageContainers[toIndex(SidebarPage::System)], LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(data.pageContainers[toIndex(SidebarPage::System)], LV_DIR_VER);
  lv_obj_set_scrollbar_mode(data.pageContainers[toIndex(SidebarPage::System)], LV_SCROLLBAR_MODE_AUTO);
  createSystemPage(data, data.pageContainers[toIndex(SidebarPage::System)]);

  refreshNavButtons(data);
  showCurrentPage(data);

  lv_obj_t* notificationPanel = lv_obj_create(lv_layer_top());
  data.notificationPanel = notificationPanel;
  lv_obj_remove_style_all(notificationPanel);
  lv_obj_set_size(notificationPanel, kNotificationWidth, LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(notificationPanel, kNotificationMinHeight, 0);
  lv_obj_set_style_radius(notificationPanel, 12, 0);
  lv_obj_set_style_pad_left(notificationPanel, 8, 0);
  lv_obj_set_style_pad_right(notificationPanel, 8, 0);
  lv_obj_set_style_pad_top(notificationPanel, 8, 0);
  lv_obj_set_style_pad_bottom(notificationPanel, 8, 0);
  lv_obj_set_style_bg_opa(notificationPanel, LV_OPA_90, 0);
  lv_obj_set_style_bg_color(notificationPanel, lv_color_hex(notificationColorHex(DisplayNotificationLevel::Info)), 0);
  lv_obj_set_style_border_width(notificationPanel, 0, 0);
  lv_obj_align(notificationPanel, LV_ALIGN_BOTTOM_LEFT, 6, kNotificationBottomY);
  lv_obj_add_flag(notificationPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(notificationPanel, LV_OBJ_FLAG_SCROLLABLE);

  data.notificationLabel = lv_label_create(notificationPanel);
  lv_obj_set_width(data.notificationLabel, kNotificationWidth - 16);
  lv_label_set_long_mode(data.notificationLabel, LV_LABEL_LONG_WRAP);
  applyCaptionStyle(data.notificationLabel, LV_TEXT_ALIGN_CENTER);
  lv_obj_set_style_text_color(data.notificationLabel, lv_color_hex(notificationTextHex(DisplayNotificationLevel::Info)), 0);
  lv_obj_center(data.notificationLabel);
}

}  // namespace

struct LvglStatusDisplay::Impl : LvglStatusDisplayData {};

namespace {

void hideNotification(LvglStatusDisplayData& data) {
  if (data.notificationPanel == nullptr) {
    return;
  }

  data.notificationVisible = false;
  data.notificationHideAtMs = 0;
  lv_obj_add_flag(data.notificationPanel, LV_OBJ_FLAG_HIDDEN);
}

void showNotificationNow(LvglStatusDisplayData& data, const DisplayNotification& notification, uint32_t nowMs) {
  if (data.notificationPanel == nullptr || data.notificationLabel == nullptr) {
    return;
  }

  lv_obj_set_style_bg_color(data.notificationPanel, lv_color_hex(notificationColorHex(notification.level)), 0);
  lv_obj_set_style_text_color(data.notificationLabel, lv_color_hex(notificationTextHex(notification.level)), 0);
  lv_label_set_text(data.notificationLabel, notification.text.c_str());
  lv_obj_update_layout(data.notificationPanel);
  lv_obj_align(data.notificationPanel, LV_ALIGN_BOTTOM_LEFT, 6, kNotificationBottomY);
  lv_obj_clear_flag(data.notificationPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(data.notificationPanel);
  data.notificationVisible = true;
  data.notificationHideAtMs = nowMs + (notification.durationMs == 0 ? 2200 : notification.durationMs);
}

void processNotificationQueue(LvglStatusDisplayData& data, uint32_t nowMs) {
  if (data.notificationVisible && data.notificationHideAtMs != 0 && nowMs >= data.notificationHideAtMs) {
    hideNotification(data);
  }

  if (data.notificationVisible || data.queuedNotificationCount == 0) {
    return;
  }

  const DisplayNotification notification = std::move(data.notificationQueue[data.notificationHead]);
  data.notificationHead = (data.notificationHead + 1) % data.notificationQueue.size();
  data.queuedNotificationCount -= 1;
  showNotificationNow(data, notification, nowMs);
}

PreviewCropRect computePreviewCrop(
    uint16_t sourceWidth,
    uint16_t sourceHeight,
    lv_coord_t targetWidth,
    lv_coord_t targetHeight) {
  PreviewCropRect crop = {
      .cropWidth = sourceWidth,
      .cropHeight = sourceHeight,
      .cropX = 0,
      .cropY = 0,
      .usesScale = false,
  };

  if (sourceWidth >= static_cast<uint16_t>(targetWidth) &&
      sourceHeight >= static_cast<uint16_t>(targetHeight)) {
    crop.cropWidth = static_cast<uint32_t>(targetWidth);
    crop.cropHeight = static_cast<uint32_t>(targetHeight);
    crop.cropX = (static_cast<uint32_t>(sourceWidth) - crop.cropWidth) / 2U;
    crop.cropY = (static_cast<uint32_t>(sourceHeight) - crop.cropHeight) / 2U;
    return crop;
  }

  crop.usesScale = true;
  const uint32_t sourceAspectScaled = static_cast<uint32_t>(sourceWidth) * static_cast<uint32_t>(targetHeight);
  const uint32_t targetAspectScaled = static_cast<uint32_t>(sourceHeight) * static_cast<uint32_t>(targetWidth);
  if (sourceAspectScaled == targetAspectScaled) {
    return crop;
  }
  if (sourceAspectScaled > targetAspectScaled) {
    crop.cropWidth = (static_cast<uint32_t>(sourceHeight) * static_cast<uint32_t>(targetWidth)) /
        static_cast<uint32_t>(targetHeight);
  } else if (sourceAspectScaled < targetAspectScaled) {
    crop.cropHeight = (static_cast<uint32_t>(sourceWidth) * static_cast<uint32_t>(targetHeight)) /
        static_cast<uint32_t>(targetWidth);
  }
  crop.cropX = (static_cast<uint32_t>(sourceWidth) - crop.cropWidth) / 2U;
  crop.cropY = (static_cast<uint32_t>(sourceHeight) - crop.cropHeight) / 2U;
  return crop;
}

void copyCroppedCameraPreview(
    std::vector<uint16_t>& destination,
    lv_coord_t targetWidth,
    lv_coord_t targetHeight,
    const uint8_t* source,
    uint16_t sourceWidth,
    uint16_t sourceHeight) {
  const auto crop = computePreviewCrop(sourceWidth, sourceHeight, targetWidth, targetHeight);
  if (crop.usesScale) {
    return;
  }

  auto readSourcePixel = [&](uint32_t x, uint32_t y) -> uint16_t {
    const std::size_t sourceIndex =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(sourceWidth) + static_cast<std::size_t>(x)) * 2U;
    const uint8_t first = source[sourceIndex];
    const uint8_t second = source[sourceIndex + 1];
    if (board::kCameraRgb565BigEndian) {
      return static_cast<uint16_t>((static_cast<uint16_t>(first) << 8) | second);
    }
    return static_cast<uint16_t>((static_cast<uint16_t>(second) << 8) | first);
  };

  for (lv_coord_t y = 0; y < targetHeight; ++y) {
    const uint32_t sourceY = crop.cropY + static_cast<uint32_t>(y);
    for (lv_coord_t x = 0; x < targetWidth; ++x) {
      const uint32_t sourceX = crop.cropX + static_cast<uint32_t>(x);
      destination[static_cast<std::size_t>(y) * static_cast<std::size_t>(targetWidth) + static_cast<std::size_t>(x)] =
          readSourcePixel(sourceX, sourceY);
    }
  }
}

void updateFaceBoxOverlays(
    const DisplayRgb565Frame& frame,
    const PreviewCropRect& crop,
    lv_coord_t targetWidth,
    lv_coord_t targetHeight,
    const std::array<lv_obj_t*, DisplayRgb565Frame::kMaxFaceBoxes>& overlays) {
  for (auto* overlay : overlays) {
    if (overlay != nullptr) {
      lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    }
  }

  const std::size_t cappedBoxCount = std::min<std::size_t>(frame.faceBoxCount, overlays.size());
  for (std::size_t index = 0; index < cappedBoxCount; index += 1) {
    auto* overlay = overlays[index];
    if (overlay == nullptr) {
      continue;
    }
    const auto& box = frame.faceBoxes[index];
    if (box.right <= box.left || box.bottom <= box.top) {
      continue;
    }

    const int32_t rawLeft =
        (static_cast<int32_t>(box.left) - static_cast<int32_t>(crop.cropX)) * targetWidth /
        static_cast<int32_t>(crop.cropWidth);
    const int32_t rawTop =
        (static_cast<int32_t>(box.top) - static_cast<int32_t>(crop.cropY)) * targetHeight /
        static_cast<int32_t>(crop.cropHeight);
    const int32_t rawRight =
        (static_cast<int32_t>(box.right) - static_cast<int32_t>(crop.cropX)) * targetWidth /
        static_cast<int32_t>(crop.cropWidth);
    const int32_t rawBottom =
        (static_cast<int32_t>(box.bottom) - static_cast<int32_t>(crop.cropY)) * targetHeight /
        static_cast<int32_t>(crop.cropHeight);
    const lv_coord_t left = static_cast<lv_coord_t>(std::clamp<int32_t>(rawLeft, 0, targetWidth));
    const lv_coord_t top = static_cast<lv_coord_t>(std::clamp<int32_t>(rawTop, 0, targetHeight));
    const lv_coord_t right = static_cast<lv_coord_t>(std::clamp<int32_t>(rawRight, 0, targetWidth));
    const lv_coord_t bottom = static_cast<lv_coord_t>(std::clamp<int32_t>(rawBottom, 0, targetHeight));
    if (right <= left || bottom <= top) {
      continue;
    }

    lv_obj_set_pos(overlay, left, top);
    lv_obj_set_size(overlay, right - left, bottom - top);
    lv_obj_set_style_border_color(
        overlay,
        lv_color_hex(frame.primaryFaceBoxIndex.has_value() && frame.primaryFaceBoxIndex.value() == index ? 0xFD20 : 0x07E0),
        0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
  }
}

void resampleCameraPreview(
    std::vector<uint16_t>& destination,
    lv_coord_t targetWidth,
    lv_coord_t targetHeight,
    const uint8_t* source,
    uint16_t sourceWidth,
    uint16_t sourceHeight) {
  auto readSourcePixel = [&](uint32_t x, uint32_t y) -> uint16_t {
    const std::size_t sourceIndex = (static_cast<std::size_t>(y) * static_cast<std::size_t>(sourceWidth) + static_cast<std::size_t>(x)) * 2U;
    const uint8_t first = source[sourceIndex];
    const uint8_t second = source[sourceIndex + 1];
    if (board::kCameraRgb565BigEndian) {
      return static_cast<uint16_t>((static_cast<uint16_t>(first) << 8) | second);
    }
    return static_cast<uint16_t>((static_cast<uint16_t>(second) << 8) | first);
  };

  uint32_t cropWidth = sourceWidth;
  uint32_t cropHeight = sourceHeight;
  uint32_t cropX = 0;
  uint32_t cropY = 0;
  const bool canCropWithoutScaling =
      sourceWidth >= static_cast<uint16_t>(targetWidth) &&
      sourceHeight >= static_cast<uint16_t>(targetHeight);

  if (canCropWithoutScaling) {
    cropWidth = static_cast<uint32_t>(targetWidth);
    cropHeight = static_cast<uint32_t>(targetHeight);
    cropX = (static_cast<uint32_t>(sourceWidth) - cropWidth) / 2U;
    cropY = (static_cast<uint32_t>(sourceHeight) - cropHeight) / 2U;

    for (lv_coord_t y = 0; y < targetHeight; ++y) {
      const uint32_t sourceY = cropY + static_cast<uint32_t>(y);
      for (lv_coord_t x = 0; x < targetWidth; ++x) {
        const uint32_t sourceX = cropX + static_cast<uint32_t>(x);
        destination[static_cast<std::size_t>(y) * static_cast<std::size_t>(targetWidth) + static_cast<std::size_t>(x)] =
            readSourcePixel(sourceX, sourceY);
      }
    }
  } else {
    const uint32_t sourceAspectScaled = static_cast<uint32_t>(sourceWidth) * static_cast<uint32_t>(targetHeight);
    const uint32_t targetAspectScaled = static_cast<uint32_t>(sourceHeight) * static_cast<uint32_t>(targetWidth);

    if (sourceAspectScaled > targetAspectScaled) {
      cropWidth = (static_cast<uint32_t>(sourceHeight) * static_cast<uint32_t>(targetWidth)) /
          static_cast<uint32_t>(targetHeight);
    } else if (sourceAspectScaled < targetAspectScaled) {
      cropHeight = (static_cast<uint32_t>(sourceWidth) * static_cast<uint32_t>(targetHeight)) /
          static_cast<uint32_t>(targetWidth);
    }

    cropX = (static_cast<uint32_t>(sourceWidth) - cropWidth) / 2U;
    cropY = (static_cast<uint32_t>(sourceHeight) - cropHeight) / 2U;
    for (lv_coord_t y = 0; y < targetHeight; ++y) {
      const uint32_t sourceY = cropY + (static_cast<uint32_t>(y) * cropHeight) / static_cast<uint32_t>(targetHeight);
      for (lv_coord_t x = 0; x < targetWidth; ++x) {
        const uint32_t sourceX = cropX + (static_cast<uint32_t>(x) * cropWidth) / static_cast<uint32_t>(targetWidth);
        destination[static_cast<std::size_t>(y) * static_cast<std::size_t>(targetWidth) + static_cast<std::size_t>(x)] =
            readSourcePixel(sourceX, sourceY);
      }
    }
  }

}

}  // namespace

LvglStatusDisplay::LvglStatusDisplay() : impl_(std::make_unique<Impl>()) {}

LvglStatusDisplay::~LvglStatusDisplay() {
  destroyUiFonts();
}

LvglStatusDisplay::LvglStatusDisplay(LvglStatusDisplay&&) noexcept = default;

LvglStatusDisplay& LvglStatusDisplay::operator=(LvglStatusDisplay&&) noexcept = default;

bool LvglStatusDisplay::init() {
  lv_init();
  lv_log_register_print_cb(printLvglLog);
  initUiFonts();

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

  impl_->lastViewModel = viewModel;
  impl_->hasViewModel = true;
  refreshUi(*impl_);
  lv_timer_handler();
  presentPreview(*impl_);
}

void LvglStatusDisplay::updateCameraPreview(const DisplayRgb565Frame& frame) {
  maybeLogDisplayPreviewPerf(millis());
  if (!ready() || frame.data == nullptr || frame.width == 0 || frame.height == 0 || impl_->homeCameraImage == nullptr) {
    return;
  }
  if (impl_->currentPage != SidebarPage::Home && impl_->currentPage != SidebarPage::Capture) {
    displayPreviewPerfWindow().skippedByPage += 1;
    return;
  }

  auto& perf = displayPreviewPerfWindow();
  const lv_coord_t targetWidth = kPageWidth;
  const lv_coord_t targetHeight =
      impl_->currentPage == SidebarPage::Capture ? kCapturePreviewHeight : kHomeCameraHeight;
  const std::size_t nextBufferIndex = (impl_->activePreviewBufferIndex + 1) % impl_->previewBuffers.size();
  PreviewBuffer& nextBuffer = impl_->previewBuffers[nextBufferIndex];
  const auto crop = computePreviewCrop(frame.width, frame.height, targetWidth, targetHeight);
  if (crop.usesScale) {
    const std::size_t bufferSize = static_cast<std::size_t>(targetWidth) * static_cast<std::size_t>(targetHeight);
    if (nextBuffer.pixels.size() != bufferSize) {
      nextBuffer.pixels.assign(bufferSize, 0);
    }

    const uint32_t resampleStartedUs = micros();
    resampleCameraPreview(
        nextBuffer.pixels,
        targetWidth,
        targetHeight,
        frame.data,
        frame.width,
        frame.height);
    perf.resampleUs += static_cast<uint64_t>(micros() - resampleStartedUs);
  } else {
    const std::size_t bufferSize = static_cast<std::size_t>(targetWidth) * static_cast<std::size_t>(targetHeight);
    if (nextBuffer.pixels.size() != bufferSize) {
      nextBuffer.pixels.assign(bufferSize, 0);
    }
    const uint32_t copyStartedUs = micros();
    copyCroppedCameraPreview(
        nextBuffer.pixels,
        targetWidth,
        targetHeight,
        frame.data,
        frame.width,
        frame.height);
    perf.resampleUs += static_cast<uint64_t>(micros() - copyStartedUs);
  }
  perf.previewUpdates += 1;

  nextBuffer.width = targetWidth;
  nextBuffer.height = targetHeight;
  impl_->activePreviewBufferIndex = nextBufferIndex;
  impl_->homeCameraPreviewReady = true;
  impl_->latestPreviewFrame = frame;
  impl_->previewNeedsPresent = true;
  if (impl_->currentPage == SidebarPage::Home && impl_->hasViewModel) {
    refreshHomePage(*impl_);
  } else if (impl_->currentPage == SidebarPage::Capture && impl_->hasViewModel) {
    refreshCapturePage(*impl_);
  }
  const uint32_t presentStartedUs = micros();
  presentPreview(*impl_);
  perf.lvglUs += static_cast<uint64_t>(micros() - presentStartedUs);
}

void LvglStatusDisplay::clearCameraPreview() {
  if (!ready() || impl_->homeCameraImage == nullptr) {
    return;
  }

  impl_->homeCameraPreviewReady = false;
  impl_->latestPreviewFrame = {};
  impl_->previewNeedsPresent = false;
  impl_->lastPreviewPresentMs = 0;
  impl_->activePreviewBufferIndex = 0;
  for (auto& buffer : impl_->previewBuffers) {
    buffer.pixels.clear();
    buffer.width = 0;
    buffer.height = 0;
  }
  for (auto* overlay : impl_->homeFaceBoxOverlays) {
    if (overlay != nullptr) {
      lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    }
  }
  for (auto* overlay : impl_->captureFaceBoxOverlays) {
    if (overlay != nullptr) {
      lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    }
  }
  lv_obj_add_flag(impl_->homeCameraImage, LV_OBJ_FLAG_HIDDEN);
  if (impl_->capturePreviewImage != nullptr) {
    lv_obj_add_flag(impl_->capturePreviewImage, LV_OBJ_FLAG_HIDDEN);
  }
  if (impl_->currentPage == SidebarPage::Home && impl_->hasViewModel) {
    refreshHomePage(*impl_);
  } else if (impl_->currentPage == SidebarPage::Capture && impl_->hasViewModel) {
    refreshCapturePage(*impl_);
  }
  const std::size_t pageIndex = toIndex(impl_->currentPage);
  if (pageIndex < impl_->pageContainers.size() && impl_->pageContainers[pageIndex] != nullptr) {
    lv_obj_invalidate(impl_->pageContainers[pageIndex]);
    lv_timer_handler();
  }
}

void LvglStatusDisplay::showNotification(const DisplayNotification& notification) {
  if (!ready() || notification.text.empty()) {
    return;
  }

  if (!impl_->notificationVisible && impl_->queuedNotificationCount == 0) {
    showNotificationNow(*impl_, notification, millis());
    return;
  }

  enqueueNotification(*impl_, notification);
}

void LvglStatusDisplay::tick(uint32_t nowMs) {
  if (!ready()) {
    return;
  }

  lv_tick_inc(nowMs - impl_->lastLvglTickMs);
  impl_->lastLvglTickMs = nowMs;
  processNotificationQueue(*impl_, nowMs);
  lv_timer_handler_run_in_period(board::kLvglHandlerPeriodMs);
  if (impl_->homeCameraPreviewReady &&
      (impl_->currentPage == SidebarPage::Home || impl_->currentPage == SidebarPage::Capture) &&
      (impl_->lastPreviewPresentMs == 0 || nowMs - impl_->lastPreviewPresentMs >= 50U)) {
    impl_->previewNeedsPresent = true;
  }
  if (impl_->previewNeedsPresent) {
    presentPreview(*impl_);
  }
}

std::optional<DisplayCommand> LvglStatusDisplay::consumeCommand() {
  if (!ready()) {
    return std::nullopt;
  }

  if (impl_->queuedCommandCount == 0) {
    return std::nullopt;
  }

  DisplayCommand command = std::move(impl_->commandQueue[impl_->commandHead]);
  impl_->commandHead = (impl_->commandHead + 1) % impl_->commandQueue.size();
  impl_->queuedCommandCount -= 1;
  return command;
}

}  // namespace infra
