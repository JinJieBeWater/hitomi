#include "infra/display/lvgl_status_display.hpp"

#include <Arduino.h>
#include <lvgl.h>

#include <array>
#include <memory>
#include <string>
#include <utility>

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

enum class SidebarPage : std::size_t {
  Home = 0,
  Enroll,
  System,
};

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

struct EnrollTaskWidgets {
  lv_obj_t* button = nullptr;
  lv_obj_t* titleLabel = nullptr;
  lv_obj_t* metaLabel = nullptr;
};

struct LvglStatusDisplayData {
  SzpiLvglDisplay driver;
  lv_display_t* display = nullptr;
  std::array<lv_obj_t*, kSidebarPages.size()> navButtons = {};
  std::array<SidebarBinding, kSidebarPages.size()> navBindings = {};
  std::array<lv_obj_t*, kSidebarPages.size()> pageContainers = {};

  lv_obj_t* homePeriodLabel = nullptr;
  lv_obj_t* homeCameraLabel = nullptr;
  lv_obj_t* homeAttendanceLabel = nullptr;

  lv_obj_t* enrollSummaryLabel = nullptr;
  lv_obj_t* enrollTaskList = nullptr;
  lv_obj_t* enrollRefreshButton = nullptr;
  lv_obj_t* enrollStartButton = nullptr;
  lv_obj_t* enrollStatusLabel = nullptr;

  std::array<StatusRow, 5> systemRows = {};

  ui::AppViewModel lastViewModel = {};
  uint32_t lastLvglTickMs = 0;
  SidebarPage currentPage = SidebarPage::Home;
  bool hasViewModel = false;
  std::string selectedEnrollmentTaskId;
  bool enrollmentRequestQueued = false;
  std::array<DisplayCommand, kDisplayCommandQueueCapacity> commandQueue = {};
  std::size_t commandHead = 0;
  std::size_t queuedCommandCount = 0;
};

void enrollTaskButtonEventCallback(lv_event_t* event);
void enrollRefreshButtonEventCallback(lv_event_t* event);
bool enqueueCommand(LvglStatusDisplayData& data, DisplayCommand command);

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
  if (!hasTask) {
    statusText = "No enrollment tasks. Tap Refresh to sync now.";
  } else if (data.enrollmentRequestQueued) {
    const auto* task = selectedEnrollmentTask(data);
    statusText = task == nullptr ? "Please reselect a task" : "Enrollment requested for " + task->title;
  } else if (selectedEnrollmentTask(data) != nullptr) {
    statusText = "Task selected. Tap Start to request enrollment, or Refresh to sync now.";
  } else {
    statusText = "Tap a task card to select it, or Refresh to sync now.";
  }
  lv_label_set_text(data.enrollStatusLabel, statusText.c_str());
}

void refreshSystemPage(LvglStatusDisplayData& data) {
  setSystemRowValue(data.systemRows[0], data.lastViewModel.credentialsLine);
  setSystemRowValue(data.systemRows[1], data.lastViewModel.wifiLine);
  setSystemRowValue(data.systemRows[2], data.lastViewModel.apiLine);
  setSystemRowValue(data.systemRows[3], data.lastViewModel.storageLine);
  setSystemRowValue(data.systemRows[4], data.lastViewModel.errorLine);
}

void refreshCurrentPage(LvglStatusDisplayData& data) {
  switch (data.currentPage) {
    case SidebarPage::Home:
      refreshHomePage(data);
      break;
    case SidebarPage::Enroll:
      refreshEnrollPage(data);
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
                           })) {
    if (data->enrollStatusLabel != nullptr) {
      lv_label_set_text(data->enrollStatusLabel, "Refresh is busy. Try again.");
    }
    return;
  }

  data->enrollmentRequestQueued = false;
  if (data->enrollStatusLabel != nullptr) {
    lv_label_set_text(data->enrollStatusLabel, "Refreshing enrollment tasks...");
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
    if (data->enrollStatusLabel != nullptr) {
      lv_label_set_text(data->enrollStatusLabel, "Enrollment action is busy. Try again.");
    }
    return;
  }

  data->enrollmentRequestQueued = true;
  refreshEnrollPage(*data);
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
  data.homeCameraLabel = lv_label_create(cameraFrame);
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
  lv_label_set_text(attendanceCaption, "Last Check-in");
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
  lv_label_set_text(header, "Enrollment tasks for this device");
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
  lv_label_set_text(startLabel, "Start Enrollment");
  lv_obj_center(startLabel);

  data.enrollStatusLabel = createPageLabel(parent, kEnrollStatusY, kPageWidth, kMutedTextHex);
  lv_label_set_long_mode(data.enrollStatusLabel, LV_LABEL_LONG_DOT);
}

void createSystemPage(LvglStatusDisplayData& data, lv_obj_t* parent) {
  data.systemRows[0] = createSystemRow(parent, 0, "Credentials");
  data.systemRows[1] = createSystemRow(parent, kSystemRowHeight + kSystemRowGap, "WiFi");
  data.systemRows[2] = createSystemRow(parent, (kSystemRowHeight + kSystemRowGap) * 2, "API");
  data.systemRows[3] = createSystemRow(parent, (kSystemRowHeight + kSystemRowGap) * 3, "Storage");
  data.systemRows[4] = createSystemRow(parent, (kSystemRowHeight + kSystemRowGap) * 4, "Error");
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

  data.pageContainers[toIndex(SidebarPage::System)] = createPageContainer(screen);
  createSystemPage(data, data.pageContainers[toIndex(SidebarPage::System)]);

  refreshNavButtons(data);
  showCurrentPage(data);
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

  impl_->lastViewModel = viewModel;
  impl_->hasViewModel = true;
  refreshUi(*impl_);
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
