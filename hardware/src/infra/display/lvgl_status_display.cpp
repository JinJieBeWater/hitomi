#include "infra/display/lvgl_status_display.hpp"

#include <Arduino.h>
#include <lvgl.h>

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "board/app_config.hpp"
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
constexpr lv_coord_t kSidebarTitleY = 12;
constexpr lv_coord_t kNavStartY = 40;
constexpr lv_coord_t kNavStepY = 38;

constexpr lv_coord_t kHomePeriodY = 0;
constexpr lv_coord_t kHomeCameraY = 36;
constexpr lv_coord_t kHomeCameraHeight = 118;
constexpr lv_coord_t kHomeAttendanceY = 164;
constexpr lv_coord_t kHomeAttendanceHeight = 48;

constexpr lv_coord_t kEnrollHeaderY = 0;
constexpr lv_coord_t kEnrollTaskY = 34;
constexpr lv_coord_t kEnrollTaskHeight = 44;
constexpr lv_coord_t kEnrollTaskGap = 8;
constexpr lv_coord_t kEnrollTaskListHeight = 132;
constexpr lv_coord_t kEnrollActionY = 184;
constexpr lv_coord_t kEnrollActionHeight = 34;
constexpr lv_coord_t kEnrollActionGap = 10;
constexpr lv_coord_t kEnrollStatusY = 228;

constexpr lv_coord_t kCaptureHeaderY = 0;
constexpr lv_coord_t kCapturePreviewY = 26;
constexpr lv_coord_t kCapturePreviewHeight = 136;
constexpr lv_coord_t kCaptureStatusY = 170;
constexpr lv_coord_t kCaptureProgressY = 196;
constexpr lv_coord_t kCaptureActionY = 218;
constexpr lv_coord_t kCaptureActionHeight = 34;

constexpr lv_coord_t kSystemRowHeight = 34;
constexpr lv_coord_t kSystemRowGap = 8;

constexpr uint32_t kSidebarBackgroundHex = 0x0D3B2A;
constexpr uint32_t kPaneBackgroundHex = 0x1F8A3B;
constexpr uint32_t kPanelHex = 0x2A9D50;
constexpr uint32_t kPanelMutedHex = 0x247E47;
constexpr uint32_t kCameraPlaceholderHex = 0x113F2C;
constexpr uint32_t kTextColorHex = 0xFFFFFF;
constexpr uint32_t kMutedTextHex = 0xDCE8DD;
constexpr uint32_t kNavButtonHex = 0x145A32;
constexpr uint32_t kNavButtonActiveHex = 0xF0F3BD;
constexpr uint32_t kNavButtonActiveTextHex = 0x0D3B2A;
constexpr uint32_t kAccentHex = 0xF4A261;
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
    {.page = SidebarPage::Home, .label = "Home"},
    {.page = SidebarPage::Enroll, .label = "Enroll"},
    {.page = SidebarPage::System, .label = "System"},
}};

struct LvglStatusDisplayData;

struct SidebarBinding {
  LvglStatusDisplayData* owner = nullptr;
  SidebarPage page = SidebarPage::Home;
};

struct StatusRow {
  lv_obj_t* panel = nullptr;
  lv_obj_t* valueLabel = nullptr;
};

struct LvglStatusDisplayData {
  SzpiLvglDisplay driver;
  lv_display_t* display = nullptr;
  std::array<lv_obj_t*, kSidebarPages.size()> navButtons = {};
  std::array<SidebarBinding, kSidebarPages.size()> navBindings = {};
  std::array<lv_obj_t*, kPageCount> pageContainers = {};

  lv_obj_t* homePeriodLabel = nullptr;
  lv_obj_t* homeCameraImage = nullptr;
  lv_obj_t* homeCameraLabel = nullptr;
  lv_obj_t* homeAttendanceLabel = nullptr;
  lv_image_dsc_t homeCameraImageDsc = {};
  std::vector<uint16_t> homeCameraPreviewBuffer = {};
  bool homeCameraPreviewReady = false;

  lv_obj_t* enrollSummaryLabel = nullptr;
  lv_obj_t* enrollTaskList = nullptr;
  lv_obj_t* enrollRefreshButton = nullptr;
  lv_obj_t* enrollStartButton = nullptr;
  lv_obj_t* enrollStatusLabel = nullptr;

  lv_obj_t* capturePreviewImage = nullptr;
  lv_obj_t* captureHintLabel = nullptr;
  lv_obj_t* captureStatusLabel = nullptr;
  lv_obj_t* captureProgressLabel = nullptr;
  lv_obj_t* captureCancelButton = nullptr;

  std::array<StatusRow, 7> systemRows = {};

  ui::AppViewModel lastViewModel = {};
  uint32_t lastLvglTickMs = 0;
  SidebarPage currentPage = SidebarPage::Home;
  bool hasViewModel = false;
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

void applyLabelStyle(lv_obj_t* label, lv_text_align_t align, uint32_t colorHex = kTextColorHex) {
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(colorHex), 0);
  lv_obj_set_style_text_align(label, align, 0);
}

void applyCaptionStyle(lv_obj_t* label, lv_text_align_t align) {
  lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(kMutedTextHex), 0);
  lv_obj_set_style_text_align(label, align, 0);
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
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_radius(button, 10, 0);

  lv_obj_t* label = lv_obj_get_child(button, 0);
  if (label != nullptr) {
    lv_obj_set_style_text_color(label, lv_color_hex(active ? kNavButtonActiveTextHex : kTextColorHex), 0);
  }
}

void applyActionButtonStyle(lv_obj_t* button, bool enabled) {
  lv_obj_set_style_bg_color(button, lv_color_hex(enabled ? kAccentHex : kPanelMutedHex), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_radius(button, 12, 0);
  lv_obj_set_style_text_color(button, lv_color_hex(kSidebarBackgroundHex), 0);
}

void applyTaskButtonStyle(lv_obj_t* button, bool selected, bool enabled) {
  const uint32_t color = !enabled ? kPanelMutedHex : (selected ? kAccentHex : kPanelHex);
  lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 0, 0);
  lv_obj_set_style_radius(button, 12, 0);
}

lv_obj_t* createPageContainer(lv_obj_t* parent) {
  lv_obj_t* page = lv_obj_create(parent);
  lv_obj_remove_style_all(page);
  lv_obj_set_size(page, kPageWidth, kPageHeight);
  lv_obj_align(page, LV_ALIGN_TOP_LEFT, kPageX, kPageY);
  lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
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
  row.panel = lv_button_create(parent);
  lv_obj_set_size(row.panel, kPageWidth, kSystemRowHeight);
  lv_obj_align(row.panel, LV_ALIGN_TOP_LEFT, 0, y);
  lv_obj_set_style_bg_color(row.panel, lv_color_hex(kPanelHex), 0);
  lv_obj_set_style_bg_opa(row.panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(row.panel, 0, 0);
  lv_obj_set_style_radius(row.panel, 12, 0);
  lv_obj_set_style_pad_left(row.panel, 10, 0);
  lv_obj_set_style_pad_right(row.panel, 10, 0);
  lv_obj_set_style_pad_top(row.panel, 4, 0);
  lv_obj_set_style_pad_bottom(row.panel, 4, 0);

  lv_obj_t* titleLabel = lv_label_create(row.panel);
  applyCaptionStyle(titleLabel, LV_TEXT_ALIGN_LEFT);
  lv_label_set_text(titleLabel, title);
  lv_obj_align(titleLabel, LV_ALIGN_TOP_LEFT, 0, 0);

  row.valueLabel = lv_label_create(row.panel);
  lv_obj_set_width(row.valueLabel, kPageWidth - 20);
  applyLabelStyle(row.valueLabel, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(row.valueLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  return row;
}

void setSystemRowValue(const StatusRow& row, const std::string& value) {
  if (row.valueLabel != nullptr) {
    lv_label_set_text(row.valueLabel, value.c_str());
  }
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

void refreshHomePage(LvglStatusDisplayData& data) {
  lv_label_set_text(data.homePeriodLabel, data.lastViewModel.periodLine.c_str());
  lv_label_set_text(data.homeCameraLabel, data.lastViewModel.cameraHintLine.c_str());
  lv_label_set_text(data.homeAttendanceLabel, data.lastViewModel.attendanceResultLine.c_str());
  if (data.homeCameraImage != nullptr) {
    if (data.homeCameraPreviewReady) {
      lv_obj_clear_flag(data.homeCameraImage, LV_OBJ_FLAG_HIDDEN);
      lv_obj_align(data.homeCameraLabel, LV_ALIGN_BOTTOM_LEFT, 10, -8);
      lv_obj_set_style_text_align(data.homeCameraLabel, LV_TEXT_ALIGN_LEFT, 0);
      lv_label_set_long_mode(data.homeCameraLabel, LV_LABEL_LONG_WRAP);
    } else {
      lv_obj_add_flag(data.homeCameraImage, LV_OBJ_FLAG_HIDDEN);
      lv_obj_center(data.homeCameraLabel);
      lv_obj_set_style_text_align(data.homeCameraLabel, LV_TEXT_ALIGN_CENTER, 0);
      lv_label_set_long_mode(data.homeCameraLabel, LV_LABEL_LONG_WRAP);
    }
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

  if (data.enrollTaskList != nullptr) {
    lv_obj_clean(data.enrollTaskList);
    for (std::size_t index = 0; index < visibleCount; index += 1) {
      const auto& task = data.lastViewModel.enrollmentTasks[index];
      lv_obj_t* button = lv_button_create(data.enrollTaskList);
      lv_obj_set_size(button, kPageWidth - 6, kEnrollTaskHeight);
      lv_obj_set_style_border_width(button, 0, 0);
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

  std::string statusText;
  if (data.lastViewModel.periodLine.rfind("Enroll:", 0) == 0) {
    statusText = "Capture running.";
  } else if (!hasTask) {
    statusText = "No tasks. Tap Refresh.";
  } else if (data.enrollmentRequestQueued) {
    const auto* task = selectedEnrollmentTask(data);
    statusText = task == nullptr ? "Reselect task." : "Starting " + task->title;
  } else if (selectedEnrollmentTask(data) != nullptr) {
    statusText = "Task selected. Tap Start.";
  } else {
    statusText = "Pick a task.";
  }
  lv_label_set_text(data.enrollStatusLabel, statusText.c_str());
}

void refreshCapturePage(LvglStatusDisplayData& data) {
  if (data.captureHintLabel != nullptr) {
    lv_label_set_text(data.captureHintLabel, data.lastViewModel.captureTitleLine.c_str());
  }
  if (data.captureStatusLabel != nullptr) {
    lv_label_set_text(data.captureStatusLabel, data.lastViewModel.captureStatusLine.c_str());
  }
  if (data.captureProgressLabel != nullptr) {
    lv_label_set_text(data.captureProgressLabel, data.lastViewModel.captureProgressLine.c_str());
  }
  if (data.captureCancelButton != nullptr) {
    applyActionButtonStyle(data.captureCancelButton, true);
    if (lv_obj_t* label = lv_obj_get_child(data.captureCancelButton, 0); label != nullptr) {
      lv_label_set_text(label, data.lastViewModel.captureActionLabel.c_str());
    }
  }

  if (data.capturePreviewImage != nullptr) {
    if (data.homeCameraPreviewReady) {
      lv_obj_clear_flag(data.capturePreviewImage, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(data.capturePreviewImage, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void refreshSystemPage(LvglStatusDisplayData& data) {
  setSystemRowValue(data.systemRows[0], data.lastViewModel.credentialsLine);
  setSystemRowValue(data.systemRows[1], data.lastViewModel.wifiLine);
  setSystemRowValue(data.systemRows[2], data.lastViewModel.apiLine);
  setSystemRowValue(data.systemRows[3], data.lastViewModel.faceLine);
  setSystemRowValue(data.systemRows[4], data.lastViewModel.faceDetectLine);
  setSystemRowValue(data.systemRows[5], data.lastViewModel.storageLine);
  setSystemRowValue(data.systemRows[6], data.lastViewModel.errorLine);
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

  refreshNavButtons(data);

  if (data.lastViewModel.captureActive && data.currentPage != SidebarPage::Capture) {
    data.currentPage = SidebarPage::Capture;
  } else if (!data.lastViewModel.captureActive && data.currentPage == SidebarPage::Capture) {
    data.currentPage = SidebarPage::Enroll;
  }

  refreshCurrentPage(data);
  showCurrentPage(data);
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
                                   .text = "Busy. Retry.",
                                   .durationMs = 2200,
                               });
    if (data->enrollStatusLabel != nullptr) {
      lv_label_set_text(data->enrollStatusLabel, "Busy. Retry.");
    }
    return;
  }

  data->enrollmentRequestQueued = false;
  enqueueNotification(*data, DisplayNotification{
                                 .level = DisplayNotificationLevel::Info,
                                 .text = "Refreshing...",
                                 .durationMs = 1800,
                             });
  if (data->enrollStatusLabel != nullptr) {
    lv_label_set_text(data->enrollStatusLabel, "Refreshing...");
  }
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
                                   .text = "Busy. Retry.",
                                   .durationMs = 2200,
                               });
    if (data->enrollStatusLabel != nullptr) {
      lv_label_set_text(data->enrollStatusLabel, "Busy. Retry.");
    }
    return;
  }

  data->enrollmentRequestQueued = true;
  enqueueNotification(*data, DisplayNotification{
                                 .level = DisplayNotificationLevel::Info,
                                 .text = "Capture started.",
                                 .durationMs = 2000,
                             });
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
                                   .text = "Busy. Retry.",
                                   .durationMs = 1800,
                               });
    return;
  }

  enqueueNotification(*data, DisplayNotification{
                                 .level = data->lastViewModel.captureRunning ? DisplayNotificationLevel::Warning
                                                                            : DisplayNotificationLevel::Info,
                                 .text = data->lastViewModel.captureRunning ? "Cancelling..."
                                                                           : "Back to tasks...",
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
  applyLabelStyle(label, LV_TEXT_ALIGN_CENTER);
  lv_label_set_text(label, spec.label);
  lv_obj_center(label);
}

void createHomePage(LvglStatusDisplayData& data, lv_obj_t* parent) {
  lv_obj_t* periodChip = lv_button_create(parent);
  lv_obj_set_size(periodChip, kPageWidth, 30);
  lv_obj_align(periodChip, LV_ALIGN_TOP_LEFT, 0, kHomePeriodY);
  lv_obj_set_style_bg_color(periodChip, lv_color_hex(kAccentHex), 0);
  lv_obj_set_style_bg_opa(periodChip, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(periodChip, 0, 0);
  lv_obj_set_style_radius(periodChip, 12, 0);
  data.homePeriodLabel = lv_label_create(periodChip);
  applyLabelStyle(data.homePeriodLabel, LV_TEXT_ALIGN_CENTER, kSidebarBackgroundHex);
  lv_obj_center(data.homePeriodLabel);

  lv_obj_t* cameraFrame = lv_obj_create(parent);
  lv_obj_set_size(cameraFrame, kPageWidth, kHomeCameraHeight);
  lv_obj_align(cameraFrame, LV_ALIGN_TOP_LEFT, 0, kHomeCameraY);
  lv_obj_set_style_bg_color(cameraFrame, lv_color_hex(kCameraPlaceholderHex), 0);
  lv_obj_set_style_bg_opa(cameraFrame, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(cameraFrame, 0, 0);
  lv_obj_set_style_radius(cameraFrame, 18, 0);
  lv_obj_remove_flag(cameraFrame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(cameraFrame, LV_SCROLLBAR_MODE_OFF);
  data.homeCameraImage = lv_image_create(cameraFrame);
  lv_obj_add_flag(data.homeCameraImage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_center(data.homeCameraImage);
  data.homeCameraLabel = lv_label_create(cameraFrame);
  lv_obj_set_width(data.homeCameraLabel, kPageWidth - 24);
  lv_label_set_long_mode(data.homeCameraLabel, LV_LABEL_LONG_WRAP);
  applyLabelStyle(data.homeCameraLabel, LV_TEXT_ALIGN_CENTER, kMutedTextHex);
  lv_obj_center(data.homeCameraLabel);

  lv_obj_t* attendanceCard = lv_button_create(parent);
  lv_obj_set_size(attendanceCard, kPageWidth, kHomeAttendanceHeight);
  lv_obj_align(attendanceCard, LV_ALIGN_TOP_LEFT, 0, kHomeAttendanceY);
  lv_obj_set_style_bg_color(attendanceCard, lv_color_hex(kPanelHex), 0);
  lv_obj_set_style_bg_opa(attendanceCard, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(attendanceCard, 0, 0);
  lv_obj_set_style_radius(attendanceCard, 14, 0);
  lv_obj_set_style_pad_all(attendanceCard, 10, 0);

  lv_obj_t* attendanceCaption = lv_label_create(attendanceCard);
  applyCaptionStyle(attendanceCaption, LV_TEXT_ALIGN_LEFT);
  lv_label_set_text(attendanceCaption, "Status");
  lv_obj_align(attendanceCaption, LV_ALIGN_TOP_LEFT, 0, 0);

  data.homeAttendanceLabel = lv_label_create(attendanceCard);
  lv_obj_set_width(data.homeAttendanceLabel, kPageWidth - 20);
  lv_label_set_long_mode(data.homeAttendanceLabel, LV_LABEL_LONG_DOT);
  applyLabelStyle(data.homeAttendanceLabel, LV_TEXT_ALIGN_LEFT);
  lv_obj_align(data.homeAttendanceLabel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void createEnrollPage(LvglStatusDisplayData& data, lv_obj_t* parent) {
  lv_obj_t* header = lv_label_create(parent);
  applyCaptionStyle(header, LV_TEXT_ALIGN_LEFT);
  lv_label_set_text(header, "Tasks");
  lv_obj_set_width(header, kPageWidth);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, kEnrollHeaderY);

  data.enrollSummaryLabel = createPageLabel(parent, 16, kPageWidth, kMutedTextHex);
  data.enrollTaskList = lv_obj_create(parent);
  lv_obj_set_size(data.enrollTaskList, kPageWidth + 6, kEnrollTaskListHeight);
  lv_obj_align(data.enrollTaskList, LV_ALIGN_TOP_LEFT, 0, kEnrollTaskY);
  lv_obj_set_style_bg_opa(data.enrollTaskList, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(data.enrollTaskList, 0, 0);
  lv_obj_set_style_pad_all(data.enrollTaskList, 0, 0);
  lv_obj_set_style_pad_right(data.enrollTaskList, 6, 0);
  lv_obj_set_scroll_dir(data.enrollTaskList, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(data.enrollTaskList, LV_SCROLLBAR_MODE_AUTO);

  const lv_coord_t actionButtonWidth = (kPageWidth - kEnrollActionGap) / 2;

  data.enrollRefreshButton = lv_button_create(parent);
  lv_obj_set_size(data.enrollRefreshButton, actionButtonWidth, kEnrollActionHeight);
  lv_obj_align(data.enrollRefreshButton, LV_ALIGN_TOP_LEFT, 0, kEnrollActionY);
  lv_obj_add_event_cb(data.enrollRefreshButton, enrollRefreshButtonEventCallback, LV_EVENT_CLICKED, &data);
  lv_obj_set_style_border_width(data.enrollRefreshButton, 0, 0);
  lv_obj_set_style_radius(data.enrollRefreshButton, 14, 0);
  lv_obj_t* refreshLabel = lv_label_create(data.enrollRefreshButton);
  applyLabelStyle(refreshLabel, LV_TEXT_ALIGN_CENTER, kSidebarBackgroundHex);
  lv_label_set_text(refreshLabel, "Refresh");
  lv_obj_center(refreshLabel);

  data.enrollStartButton = lv_button_create(parent);
  lv_obj_set_size(data.enrollStartButton, actionButtonWidth, kEnrollActionHeight);
  lv_obj_align(data.enrollStartButton, LV_ALIGN_TOP_LEFT, actionButtonWidth + kEnrollActionGap, kEnrollActionY);
  lv_obj_add_event_cb(data.enrollStartButton, enrollStartButtonEventCallback, LV_EVENT_CLICKED, &data);
  lv_obj_set_style_border_width(data.enrollStartButton, 0, 0);
  lv_obj_set_style_radius(data.enrollStartButton, 14, 0);
  lv_obj_t* startLabel = lv_label_create(data.enrollStartButton);
  applyLabelStyle(startLabel, LV_TEXT_ALIGN_CENTER, kSidebarBackgroundHex);
  lv_label_set_text(startLabel, "Start");
  lv_obj_center(startLabel);

  data.enrollStatusLabel = createPageLabel(parent, kEnrollStatusY, kPageWidth, kMutedTextHex);
  lv_label_set_long_mode(data.enrollStatusLabel, LV_LABEL_LONG_DOT);
}

void createCapturePage(LvglStatusDisplayData& data, lv_obj_t* parent) {
  lv_obj_t* header = lv_label_create(parent);
  applyCaptionStyle(header, LV_TEXT_ALIGN_LEFT);
  lv_label_set_text(header, "Capture");
  lv_obj_set_width(header, kPageWidth);
  lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, kCaptureHeaderY);

  lv_obj_t* previewFrame = lv_obj_create(parent);
  lv_obj_set_size(previewFrame, kPageWidth, kCapturePreviewHeight);
  lv_obj_align(previewFrame, LV_ALIGN_TOP_LEFT, 0, kCapturePreviewY);
  lv_obj_set_style_bg_color(previewFrame, lv_color_hex(kCameraPlaceholderHex), 0);
  lv_obj_set_style_bg_opa(previewFrame, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(previewFrame, 0, 0);
  lv_obj_set_style_radius(previewFrame, 18, 0);
  lv_obj_remove_flag(previewFrame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(previewFrame, LV_SCROLLBAR_MODE_OFF);

  data.capturePreviewImage = lv_image_create(previewFrame);
  lv_obj_add_flag(data.capturePreviewImage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_center(data.capturePreviewImage);

  data.captureHintLabel = lv_label_create(previewFrame);
  lv_obj_set_width(data.captureHintLabel, kPageWidth - 24);
  lv_label_set_long_mode(data.captureHintLabel, LV_LABEL_LONG_WRAP);
  applyLabelStyle(data.captureHintLabel, LV_TEXT_ALIGN_CENTER, kMutedTextHex);
  lv_obj_align(data.captureHintLabel, LV_ALIGN_TOP_MID, 0, 10);

  data.captureStatusLabel = createPageLabel(parent, kCaptureStatusY, kPageWidth, kTextColorHex);
  lv_label_set_long_mode(data.captureStatusLabel, LV_LABEL_LONG_WRAP);

  data.captureProgressLabel = createPageLabel(parent, kCaptureProgressY, kPageWidth, kMutedTextHex);
  lv_label_set_long_mode(data.captureProgressLabel, LV_LABEL_LONG_WRAP);

  data.captureCancelButton = lv_button_create(parent);
  lv_obj_set_size(data.captureCancelButton, kPageWidth, kCaptureActionHeight);
  lv_obj_align(data.captureCancelButton, LV_ALIGN_TOP_LEFT, 0, kCaptureActionY);
  lv_obj_add_event_cb(data.captureCancelButton, captureCancelButtonEventCallback, LV_EVENT_CLICKED, &data);
  lv_obj_set_style_border_width(data.captureCancelButton, 0, 0);
  lv_obj_set_style_radius(data.captureCancelButton, 14, 0);
  lv_obj_t* cancelLabel = lv_label_create(data.captureCancelButton);
  applyLabelStyle(cancelLabel, LV_TEXT_ALIGN_CENTER, kSidebarBackgroundHex);
  lv_label_set_text(cancelLabel, "Cancel");
  lv_obj_center(cancelLabel);
}

void createSystemPage(LvglStatusDisplayData& data, lv_obj_t* parent) {
  data.systemRows[0] = createSystemRow(parent, 0, "Credentials");
  data.systemRows[1] = createSystemRow(parent, kSystemRowHeight + kSystemRowGap, "WiFi");
  data.systemRows[2] = createSystemRow(parent, (kSystemRowHeight + kSystemRowGap) * 2, "API");
  data.systemRows[3] = createSystemRow(parent, (kSystemRowHeight + kSystemRowGap) * 3, "Face");
  data.systemRows[4] = createSystemRow(parent, (kSystemRowHeight + kSystemRowGap) * 4, "Detect");
  data.systemRows[5] = createSystemRow(parent, (kSystemRowHeight + kSystemRowGap) * 5, "Storage");
  data.systemRows[6] = createSystemRow(parent, (kSystemRowHeight + kSystemRowGap) * 6, "Error");
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

  lv_obj_t* sidebarTitle = lv_label_create(sidebar);
  applyLabelStyle(sidebarTitle, LV_TEXT_ALIGN_CENTER);
  lv_label_set_text(sidebarTitle, "Hitomi");
  lv_obj_set_width(sidebarTitle, kSidebarWidth);
  lv_obj_align(sidebarTitle, LV_ALIGN_TOP_MID, 0, kSidebarTitleY);

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

void updatePreviewDescriptor(LvglStatusDisplayData& data, lv_coord_t width, lv_coord_t height) {
  data.homeCameraImageDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  data.homeCameraImageDsc.header.cf = LV_COLOR_FORMAT_RGB565;
  data.homeCameraImageDsc.header.flags = 0;
  data.homeCameraImageDsc.header.w = static_cast<uint16_t>(width);
  data.homeCameraImageDsc.header.h = static_cast<uint16_t>(height);
  data.homeCameraImageDsc.header.stride = static_cast<uint16_t>(width * sizeof(uint16_t));
  data.homeCameraImageDsc.data_size = static_cast<uint32_t>(width * height * sizeof(uint16_t));
  data.homeCameraImageDsc.data = reinterpret_cast<const uint8_t*>(data.homeCameraPreviewBuffer.data());
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

  const uint32_t sourceAspectScaled = static_cast<uint32_t>(sourceWidth) * static_cast<uint32_t>(targetHeight);
  const uint32_t targetAspectScaled = static_cast<uint32_t>(sourceHeight) * static_cast<uint32_t>(targetWidth);

  uint32_t cropWidth = sourceWidth;
  uint32_t cropHeight = sourceHeight;
  if (sourceAspectScaled > targetAspectScaled) {
    cropWidth = (static_cast<uint32_t>(sourceHeight) * static_cast<uint32_t>(targetWidth)) / static_cast<uint32_t>(targetHeight);
  } else if (sourceAspectScaled < targetAspectScaled) {
    cropHeight = (static_cast<uint32_t>(sourceWidth) * static_cast<uint32_t>(targetHeight)) / static_cast<uint32_t>(targetWidth);
  }

  const uint32_t cropX = (static_cast<uint32_t>(sourceWidth) - cropWidth) / 2U;
  const uint32_t cropY = (static_cast<uint32_t>(sourceHeight) - cropHeight) / 2U;
  for (lv_coord_t y = 0; y < targetHeight; ++y) {
    const uint32_t sourceY = cropY + (static_cast<uint32_t>(y) * cropHeight) / static_cast<uint32_t>(targetHeight);
    for (lv_coord_t x = 0; x < targetWidth; ++x) {
      const uint32_t sourceX = cropX + (static_cast<uint32_t>(x) * cropWidth) / static_cast<uint32_t>(targetWidth);
      destination[static_cast<std::size_t>(y) * static_cast<std::size_t>(targetWidth) + static_cast<std::size_t>(x)] =
          readSourcePixel(sourceX, sourceY);
    }
  }
}

}  // namespace

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

  impl_->lastViewModel = viewModel;
  impl_->hasViewModel = true;
  refreshUi(*impl_);
  lv_timer_handler();
}

void LvglStatusDisplay::updateCameraPreview(const DisplayRgb565Frame& frame) {
  if (!ready() || frame.data == nullptr || frame.width == 0 || frame.height == 0 || impl_->homeCameraImage == nullptr) {
    return;
  }
  if (impl_->currentPage != SidebarPage::Home && impl_->currentPage != SidebarPage::Capture) {
    return;
  }

  const lv_coord_t targetWidth = kPageWidth;
  const lv_coord_t targetHeight = kHomeCameraHeight;
  const std::size_t bufferSize = static_cast<std::size_t>(targetWidth) * static_cast<std::size_t>(targetHeight);
  if (impl_->homeCameraPreviewBuffer.size() != bufferSize) {
    impl_->homeCameraPreviewBuffer.assign(bufferSize, 0);
  }

  resampleCameraPreview(
      impl_->homeCameraPreviewBuffer,
      targetWidth,
      targetHeight,
      frame.data,
      frame.width,
      frame.height);
  updatePreviewDescriptor(*impl_, targetWidth, targetHeight);
  impl_->homeCameraPreviewReady = true;

  lv_image_set_src(impl_->homeCameraImage, &impl_->homeCameraImageDsc);
  lv_obj_set_size(impl_->homeCameraImage, targetWidth, targetHeight);
  lv_obj_center(impl_->homeCameraImage);
  lv_obj_clear_flag(impl_->homeCameraImage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(impl_->homeCameraImage);
  if (impl_->capturePreviewImage != nullptr) {
    lv_image_set_src(impl_->capturePreviewImage, &impl_->homeCameraImageDsc);
    lv_obj_set_size(impl_->capturePreviewImage, targetWidth, targetHeight);
    lv_obj_center(impl_->capturePreviewImage);
    lv_obj_clear_flag(impl_->capturePreviewImage, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(impl_->capturePreviewImage);
  }
  if (impl_->currentPage == SidebarPage::Home && impl_->hasViewModel) {
    refreshHomePage(*impl_);
  } else if (impl_->currentPage == SidebarPage::Capture && impl_->hasViewModel) {
    refreshCapturePage(*impl_);
  }
}

void LvglStatusDisplay::clearCameraPreview() {
  if (!ready() || impl_->homeCameraImage == nullptr) {
    return;
  }

  impl_->homeCameraPreviewReady = false;
  lv_obj_add_flag(impl_->homeCameraImage, LV_OBJ_FLAG_HIDDEN);
  if (impl_->capturePreviewImage != nullptr) {
    lv_obj_add_flag(impl_->capturePreviewImage, LV_OBJ_FLAG_HIDDEN);
  }
  if (impl_->currentPage == SidebarPage::Home && impl_->hasViewModel) {
    refreshHomePage(*impl_);
  } else if (impl_->currentPage == SidebarPage::Capture && impl_->hasViewModel) {
    refreshCapturePage(*impl_);
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
