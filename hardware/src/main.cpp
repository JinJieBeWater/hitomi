#include <Arduino.h>
#include <esp_ota_ops.h>
#include <lvgl.h>
#include <szpi_lvgl_display.hpp>

constexpr int kBootKeyPin = 0;
constexpr unsigned long kSerialWaitTimeoutMs = 2000;
constexpr unsigned long kButtonPollIntervalMs = 50;
constexpr unsigned long kLoopYieldDelayMs = 1;
constexpr uint32_t kLvglHandlerPeriodMs = 5;
constexpr char kFirmwareTag[] = "lvgl-esp-lcd-2026-03-31-01";

bool gLastButtonPressed = false;
lv_display_t* gDisplay = nullptr;
uint32_t gLastLvglTickMs = 0;
uint32_t gLastButtonPollMs = 0;
SzpiLvglDisplay gDisplayDriver;

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

void printLvglLog(lv_log_level_t level, const char* message) {
  static_cast<void>(level);
  Serial.printf("[LVGL] %s\n", message);
}

void initUi() {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x102542), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  lv_obj_t* title = lv_label_create(screen);
  lv_label_set_text(title, "LVGL OK");
  lv_obj_set_style_text_color(title, lv_color_hex(0xF4F1DE), 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -24);

  lv_obj_t* subtitle = lv_label_create(screen);
  lv_label_set_text(subtitle, "SZPI ESP32-S3\nESP_LCD flush active");
  lv_obj_set_style_text_color(subtitle, lv_color_hex(0xBFD7EA), 0);
  lv_obj_set_style_text_align(subtitle, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(subtitle, LV_ALIGN_CENTER, 0, 12);

  lv_obj_t* footer = lv_label_create(screen);
  lv_label_set_text(footer, kFirmwareTag);
  lv_obj_set_style_text_color(footer, lv_color_hex(0xE9C46A), 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void setup() {
  pinMode(kBootKeyPin, INPUT_PULLUP);

  Serial.begin(115200);
  waitForSerial();

  lv_init();
  lv_log_register_print_cb(printLvglLog);

  if (!gDisplayDriver.init()) {
    Serial.println("SZPI LVGL display init failed.");
  } else {
    gDisplay = gDisplayDriver.display();
    initUi();
    lv_timer_handler();
  }

  gLastLvglTickMs = millis();
  gLastButtonPollMs = gLastLvglTickMs;

  Serial.println("SZPI ESP32-S3 first flash test");
  Serial.printf("FW tag: %s\n", kFirmwareTag);
  Serial.println("BOOT key is on GPIO0.");
  Serial.printf("LVGL initialized: %d.%d.%d\n", lv_version_major(), lv_version_minor(), lv_version_patch());
  Serial.printf("LVGL display ready: %s\n", gDisplay != nullptr ? "yes" : "no");
  printRuntimeCheck();

  gLastButtonPressed = digitalRead(kBootKeyPin) == LOW;
  printButtonState(gLastButtonPressed);
}

void loop() {
  uint32_t now = millis();

  if (gDisplay != nullptr) {
    lv_tick_inc(now - gLastLvglTickMs);
    gLastLvglTickMs = now;
    lv_timer_handler_run_in_period(kLvglHandlerPeriodMs);
  }

  if (now - gLastButtonPollMs >= kButtonPollIntervalMs) {
    gLastButtonPollMs = now;
    bool buttonPressed = digitalRead(kBootKeyPin) == LOW;

    if (buttonPressed != gLastButtonPressed) {
      gLastButtonPressed = buttonPressed;
      printButtonState(buttonPressed);
    }
  }

  delay(kLoopYieldDelayMs);
}
