#include <Arduino.h>
#include <esp_ota_ops.h>
#include <lvgl.h>
#include <szpi_lvgl_display.hpp>

namespace {

constexpr int kBootKeyPin = 0;
constexpr unsigned long kSerialWaitTimeoutMs = 2000;
constexpr unsigned long kButtonPollIntervalMs = 50;
constexpr unsigned long kLoopYieldDelayMs = 1;
constexpr uint32_t kLvglHandlerPeriodMs = 5;
constexpr char kFirmwareTag[] = "lvgl-esp-lcd-2026-03-31-01";

struct AppState {
  bool lastButtonPressed = false;
  lv_display_t* display = nullptr;
  uint32_t lastLvglTickMs = 0;
  uint32_t lastButtonPollMs = 0;
  SzpiLvglDisplay displayDriver;
};

AppState gApp;

void waitForSerial() {
  unsigned long startedAt = millis();

  while (!Serial && millis() - startedAt < kSerialWaitTimeoutMs) {
    delay(10);
  }
}

void printRuntimeCheck() {
  const esp_partition_t* partition = esp_ota_get_running_partition();

  Serial.println();
  Serial.println("=== SZPI ESP32-S3 runtime check ===");
  Serial.printf("Chip model: %s\n", ESP.getChipModel());
  Serial.printf("Chip revision: %d\n", ESP.getChipRevision());
  Serial.printf("SDK version: %s\n", ESP.getSdkVersion());
  Serial.printf("CPU frequency: %lu MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash size: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));

#if ARDUINO_USB_CDC_ON_BOOT
  Serial.println("USB CDC on boot: enabled");
#else
  Serial.println("USB CDC on boot: disabled");
#endif

#if defined(BOARD_HAS_PSRAM)
  Serial.println("BOARD_HAS_PSRAM: defined");
#else
  Serial.println("BOARD_HAS_PSRAM: not defined");
#endif

  Serial.printf("PSRAM detected: %s\n", psramFound() ? "yes" : "no");
  Serial.printf("PSRAM size: %u MB\n", ESP.getPsramSize() / (1024 * 1024));
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());

  if (partition != nullptr) {
    Serial.printf("Running partition: %s (%u bytes)\n", partition->label, partition->size);
  }

  Serial.println("===============================");
}

void printButtonState(bool pressed) {
  Serial.println(pressed ? "BOOT pressed" : "BOOT released");
}

const char* getLvglLogLevelName(lv_log_level_t level) {
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
  Serial.printf("[LVGL][%s] %s\n", getLvglLogLevelName(level), message);
}

void createScreenLabel(
    const char* text,
    uint32_t color,
    lv_align_t align,
    lv_coord_t x,
    lv_coord_t y,
    lv_text_align_t textAlign) {
  lv_obj_t* label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(label, textAlign, 0);
  lv_obj_align(label, align, x, y);
}

void initUi() {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x102542), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  createScreenLabel("LVGL OK", 0xF4F1DE, LV_ALIGN_CENTER, 0, -24, LV_TEXT_ALIGN_LEFT);
  createScreenLabel(
      "SZPI ESP32-S3\nESP_LCD flush active",
      0xBFD7EA,
      LV_ALIGN_CENTER,
      0,
      12,
      LV_TEXT_ALIGN_CENTER);
  createScreenLabel(
      kFirmwareTag, 0xE9C46A, LV_ALIGN_BOTTOM_MID, 0, -10, LV_TEXT_ALIGN_LEFT);
}

void initDisplay() {
  if (!gApp.displayDriver.init()) {
    Serial.println("SZPI LVGL display init failed.");
    return;
  }

  gApp.display = gApp.displayDriver.display();
  initUi();
  lv_timer_handler();
}

void pollBootButton(uint32_t now) {
  if (now - gApp.lastButtonPollMs < kButtonPollIntervalMs) {
    return;
  }

  gApp.lastButtonPollMs = now;
  bool buttonPressed = digitalRead(kBootKeyPin) == LOW;
  if (buttonPressed == gApp.lastButtonPressed) {
    return;
  }

  gApp.lastButtonPressed = buttonPressed;
  printButtonState(buttonPressed);
}

void runLvglLoop(uint32_t now) {
  if (gApp.display == nullptr) {
    return;
  }

  lv_tick_inc(now - gApp.lastLvglTickMs);
  gApp.lastLvglTickMs = now;
  lv_timer_handler_run_in_period(kLvglHandlerPeriodMs);
}

}  // namespace

void setup() {
  pinMode(kBootKeyPin, INPUT_PULLUP);

  Serial.begin(115200);
  waitForSerial();

  lv_init();
  lv_log_register_print_cb(printLvglLog);

  initDisplay();

  uint32_t now = millis();
  gApp.lastLvglTickMs = now;
  gApp.lastButtonPollMs = now;

  Serial.println("SZPI ESP32-S3 first flash test");
  Serial.printf("FW tag: %s\n", kFirmwareTag);
  Serial.println("BOOT key is on GPIO0.");
  Serial.printf(
      "LVGL initialized: %d.%d.%d\n",
      lv_version_major(),
      lv_version_minor(),
      lv_version_patch());
  Serial.printf("LVGL display ready: %s\n", gApp.display != nullptr ? "yes" : "no");
  printRuntimeCheck();

  gApp.lastButtonPressed = digitalRead(kBootKeyPin) == LOW;
  printButtonState(gApp.lastButtonPressed);
}

void loop() {
  uint32_t now = millis();

  runLvglLoop(now);
  pollBootButton(now);

  delay(kLoopYieldDelayMs);
}
