#pragma once

#include <driver/i2c.h>
#include <driver/ledc.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <freertos/FreeRTOS.h>
#include <lvgl.h>

class SzpiLvglDisplay {
 public:
  static constexpr uint32_t kHorizontalResolution = 320;
  static constexpr uint32_t kVerticalResolution = 240;
  static constexpr uint32_t kBufferRows = 20;

  bool init();
  lv_display_t* display() const;

 private:
  static void flushCallback(lv_display_t* display, const lv_area_t* area, uint8_t* pxMap);
  static bool onColorTransferDone(
      esp_lcd_panel_io_handle_t panelIo, esp_lcd_panel_io_event_data_t* eventData, void* userCtx);

  bool initIoExpander();
  bool initBacklightPwm();
  bool initPanel();
  bool initLvglDisplay();
  bool writePca9557Register(uint8_t reg, uint8_t value);
  bool setPca9557OutputState(uint8_t gpioBit, bool level);
  bool setBacklightPercent(int percent);
  void flush(const lv_area_t* area, uint8_t* pxMap);

  static constexpr TickType_t kI2cTimeout = pdMS_TO_TICKS(1000);

  static constexpr uint8_t kPca9557Address = 0x19;
  static constexpr uint8_t kPca9557OutputReg = 0x01;
  static constexpr uint8_t kPca9557ConfigReg = 0x03;
  static constexpr uint8_t kPca9557DefaultOutput = 0x05;
  static constexpr uint8_t kPca9557DefaultConfig = 0xF8;
  static constexpr uint8_t kLcdCsBit = 1u << 0;

  esp_lcd_panel_io_handle_t _io = nullptr;
  esp_lcd_panel_handle_t _panel = nullptr;
  lv_display_t* _display = nullptr;
  uint16_t* _drawBuffer = nullptr;
  bool _ready = false;
};
