#include "infra/display/szpi_lvgl_display.hpp"

#include <Arduino.h>

#include <driver/ledc.h>
#include <driver/spi_master.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <freertos/FreeRTOS.h>
#include <lvgl.h>

#include <memory>
#include <utility>

#include "board/pins.hpp"
#include "infra/display/touch_transform.hpp"
#include "infra/board/board_control_bus.hpp"

namespace {

constexpr int kI2cTimeoutMs = 1000;
constexpr int kTouchI2cTimeoutMs = 20;

constexpr uint8_t kFt6336Address = 0x38;
constexpr uint8_t kFt6336TouchStatusReg = 0x02;
constexpr size_t kFt6336PointPayloadSize = 5;
constexpr uint8_t kFt6336TouchCountMask = 0x0F;
constexpr uint8_t kFt6336CoordHighMask = 0x0F;

constexpr infra::TouchTransformConfig kTouchTransform = infra::makeTouchTransform(
    static_cast<int16_t>(SzpiLvglDisplay::kHorizontalResolution),
    static_cast<int16_t>(SzpiLvglDisplay::kVerticalResolution),
    infra::kSzpiTouchOrientation);

struct Ft6336Sample {
  bool pressed = false;
  uint16_t rawX = 0;
  uint16_t rawY = 0;
};

struct SzpiLvglDisplayImpl {
  esp_lcd_panel_io_handle_t io = nullptr;
  esp_lcd_panel_handle_t panel = nullptr;
  lv_display_t* display = nullptr;
  lv_indev_t* touchIndev = nullptr;
  uint16_t* drawBuffer = nullptr;
  infra::TouchReadState touchState = {};
  uint32_t touchTapCount = 0;
  uint32_t lastTouchErrorLogMs = 0;
  bool spiInitialized = false;
  bool touchReady = false;
  bool ready = false;
};

bool logEspError(esp_err_t err, const char* action) {
  if (err == ESP_OK) {
    return true;
  }

  Serial.printf("[SZPI.ESP_LCD] %s failed: %s\n", action, esp_err_to_name(err));
  return false;
}

lv_point_t toLvPoint(const infra::TouchPoint& point) {
  return lv_point_t{point.x, point.y};
}

void flushCallback(lv_display_t* display, const lv_area_t* area, uint8_t* pxMap) {
  auto* impl = static_cast<SzpiLvglDisplayImpl*>(lv_display_get_driver_data(display));
  if (impl == nullptr || impl->panel == nullptr) {
    lv_display_flush_ready(display);
    return;
  }

  esp_err_t err =
      esp_lcd_panel_draw_bitmap(impl->panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pxMap);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] draw_bitmap failed: %s\n", esp_err_to_name(err));
    lv_display_flush_ready(display);
  }
}

bool onColorTransferDone(
    esp_lcd_panel_io_handle_t panelIo, esp_lcd_panel_io_event_data_t* eventData, void* userCtx) {
  static_cast<void>(panelIo);
  static_cast<void>(eventData);

  auto* impl = static_cast<SzpiLvglDisplayImpl*>(userCtx);
  if (impl != nullptr && impl->display != nullptr) {
    lv_display_flush_ready(impl->display);
  }

  return false;
}

bool readI2cRegisters(uint8_t address, uint8_t startReg, uint8_t* buffer, size_t length, int timeoutMs) {
  return infra::readBoardControlRegisters(address, startReg, buffer, length, timeoutMs);
}

bool probeFt6336() {
  return infra::probeBoardControlDevice(kFt6336Address, kTouchI2cTimeoutMs);
}

bool readFt6336Sample(Ft6336Sample& sample) {
  uint8_t payload[kFt6336PointPayloadSize] = {};
  if (!readI2cRegisters(
          kFt6336Address,
          kFt6336TouchStatusReg,
          payload,
          sizeof(payload),
          kTouchI2cTimeoutMs)) {
    return false;
  }

  const uint8_t touchCount = payload[0] & kFt6336TouchCountMask;
  if (touchCount == 0) {
    sample.pressed = false;
    sample.rawX = 0;
    sample.rawY = 0;
    return true;
  }

  sample.pressed = true;
  sample.rawX = static_cast<uint16_t>(((payload[1] & kFt6336CoordHighMask) << 8) | payload[2]);
  sample.rawY = static_cast<uint16_t>(((payload[3] & kFt6336CoordHighMask) << 8) | payload[4]);
  return true;
}

bool setPca9557OutputState(uint8_t gpioBit, bool level) {
  return infra::updateBoardControlRegisterBit(
      board::kIoExpanderAddress,
      board::kIoExpanderOutputReg,
      gpioBit,
      level,
      kI2cTimeoutMs);
}

bool initIoExpander() {
  return infra::ensureBoardIoExpanderReady();
}

bool initBacklightPwm() {
  ledc_channel_config_t channelCfg = {};
  channelCfg.gpio_num = board::kLcdBacklightPin;
  channelCfg.speed_mode = LEDC_LOW_SPEED_MODE;
  channelCfg.channel = board::kBacklightChannel;
  channelCfg.intr_type = LEDC_INTR_DISABLE;
  channelCfg.timer_sel = board::kBacklightTimer;
  channelCfg.duty = 0;
  channelCfg.hpoint = 0;
  channelCfg.flags.output_invert = true;

  ledc_timer_config_t timerCfg = {};
  timerCfg.speed_mode = LEDC_LOW_SPEED_MODE;
  timerCfg.duty_resolution = LEDC_TIMER_10_BIT;
  timerCfg.timer_num = board::kBacklightTimer;
  timerCfg.freq_hz = 5000;
  timerCfg.clk_cfg = LEDC_AUTO_CLK;

  return logEspError(ledc_timer_config(&timerCfg), "ledc_timer_config") &&
         logEspError(ledc_channel_config(&channelCfg), "ledc_channel_config");
}

bool setBacklightPercent(int percent) {
  if (percent < 0) {
    percent = 0;
  } else if (percent > 100) {
    percent = 100;
  }

  uint32_t duty = (1023U * static_cast<uint32_t>(percent)) / 100U;
  esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, board::kBacklightChannel, duty);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] ledc_set_duty failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = ledc_update_duty(LEDC_LOW_SPEED_MODE, board::kBacklightChannel);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] ledc_update_duty failed: %s\n", esp_err_to_name(err));
    return false;
  }

  return true;
}

bool initPanel(SzpiLvglDisplayImpl& impl) {
  spi_bus_config_t busCfg = {};
  busCfg.sclk_io_num = board::kLcdSclkPin;
  busCfg.mosi_io_num = board::kLcdMosiPin;
  busCfg.miso_io_num = GPIO_NUM_NC;
  busCfg.quadwp_io_num = GPIO_NUM_NC;
  busCfg.quadhd_io_num = GPIO_NUM_NC;
  busCfg.max_transfer_sz =
      SzpiLvglDisplay::kHorizontalResolution * SzpiLvglDisplay::kBufferRows * sizeof(uint16_t);

  esp_err_t err = spi_bus_initialize(board::kLcdSpiHost, &busCfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    logEspError(err, "spi_bus_initialize");
    return false;
  }
  impl.spiInitialized = true;

  esp_lcd_panel_io_spi_config_t ioCfg = {};
  ioCfg.dc_gpio_num = board::kLcdDcPin;
  ioCfg.cs_gpio_num = GPIO_NUM_NC;
  ioCfg.pclk_hz = board::kLcdPixelClockHz;
  ioCfg.lcd_cmd_bits = 8;
  ioCfg.lcd_param_bits = 8;
  ioCfg.spi_mode = 2;
  ioCfg.trans_queue_depth = 1;
  ioCfg.on_color_trans_done = onColorTransferDone;
  ioCfg.user_ctx = &impl;

  err = esp_lcd_new_panel_io_spi(
      static_cast<esp_lcd_spi_bus_handle_t>(board::kLcdSpiHost), &ioCfg, &impl.io);
  if (!logEspError(err, "esp_lcd_new_panel_io_spi")) {
    return false;
  }

  esp_lcd_panel_dev_config_t panelCfg = {};
  panelCfg.reset_gpio_num = GPIO_NUM_NC;
  panelCfg.color_space = ESP_LCD_COLOR_SPACE_RGB;
  panelCfg.bits_per_pixel = 16;

  if (!logEspError(esp_lcd_new_panel_st7789(impl.io, &panelCfg, &impl.panel), "esp_lcd_new_panel_st7789")) {
    return false;
  }
  if (!logEspError(esp_lcd_panel_reset(impl.panel), "esp_lcd_panel_reset")) {
    return false;
  }
  if (!setPca9557OutputState(board::kIoExpanderLcdCsBit, false)) {
    Serial.println("[SZPI.ESP_LCD] Failed to pull LCD_CS low");
    return false;
  }
  if (!logEspError(esp_lcd_panel_init(impl.panel), "esp_lcd_panel_init")) {
    return false;
  }
  if (!logEspError(esp_lcd_panel_invert_color(impl.panel, true), "esp_lcd_panel_invert_color")) {
    return false;
  }
  if (!logEspError(
          esp_lcd_panel_swap_xy(impl.panel, infra::kSzpiPanelOrientation.swapXY),
          "esp_lcd_panel_swap_xy")) {
    return false;
  }
  if (!logEspError(
          esp_lcd_panel_mirror(
              impl.panel,
              infra::kSzpiPanelOrientation.mirrorX,
              infra::kSzpiPanelOrientation.mirrorY),
          "esp_lcd_panel_mirror")) {
    return false;
  }
  return logEspError(esp_lcd_panel_disp_on_off(impl.panel, true), "esp_lcd_panel_disp_on_off");
}

bool initLvglDisplay(SzpiLvglDisplayImpl& impl) {
  impl.drawBuffer = static_cast<uint16_t*>(heap_caps_malloc(
      SzpiLvglDisplay::kHorizontalResolution * SzpiLvglDisplay::kBufferRows * sizeof(uint16_t),
      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (impl.drawBuffer == nullptr) {
    Serial.println("[SZPI.ESP_LCD] Failed to allocate LVGL draw buffer");
    return false;
  }

  impl.display = lv_display_create(SzpiLvglDisplay::kHorizontalResolution, SzpiLvglDisplay::kVerticalResolution);
  if (impl.display == nullptr) {
    Serial.println("[SZPI.ESP_LCD] lv_display_create failed");
    return false;
  }

  lv_display_set_driver_data(impl.display, &impl);
  lv_display_set_color_format(impl.display, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_flush_cb(impl.display, flushCallback);
  lv_display_set_buffers(
      impl.display,
      impl.drawBuffer,
      nullptr,
      SzpiLvglDisplay::kHorizontalResolution * SzpiLvglDisplay::kBufferRows * sizeof(uint16_t),
      LV_DISPLAY_RENDER_MODE_PARTIAL);

  return true;
}

void applyTouchReadResult(SzpiLvglDisplayImpl& impl, const infra::TouchReadResult& result, lv_indev_data_t* data) {
  impl.touchState = result.nextState;
  data->state = result.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->point = toLvPoint(result.point);
}

void touchReadCallback(lv_indev_t* indev, lv_indev_data_t* data) {
  auto* impl = static_cast<SzpiLvglDisplayImpl*>(lv_indev_get_driver_data(indev));
  if (impl == nullptr) {
    data->state = LV_INDEV_STATE_RELEASED;
    data->point = toLvPoint(infra::TouchPoint{});
    return;
  }

  Ft6336Sample sample = {};
  const bool wasPressed = impl->touchState.pressed;
  if (!readFt6336Sample(sample)) {
    const uint32_t nowMs = millis();
    if (nowMs - impl->lastTouchErrorLogMs >= 1000U) {
      Serial.println("[SZPI.TOUCH] FT6336 read failed; preserving previous touch state");
      impl->lastTouchErrorLogMs = nowMs;
    }
    const infra::TouchReadResult resolved = infra::resolveTouchReadState(
        impl->touchState,
        false,
        false,
        infra::TouchPoint{});
    applyTouchReadResult(*impl, resolved, data);
    return;
  }

  const infra::TouchPoint transformedPoint = infra::applyTouchTransform(sample.rawX, sample.rawY, kTouchTransform);
  const infra::TouchReadResult resolved = infra::resolveTouchReadState(
      impl->touchState,
      true,
      sample.pressed,
      transformedPoint);
  if (wasPressed && !resolved.nextState.pressed) {
    impl->touchTapCount += 1;
  }
  impl->lastTouchErrorLogMs = 0;

  applyTouchReadResult(*impl, resolved, data);
}

bool initTouchInput(SzpiLvglDisplayImpl& impl) {
  if (impl.display == nullptr) {
    return false;
  }

  if (!probeFt6336()) {
    Serial.println("[SZPI.TOUCH] FT6336 probe failed; touch input disabled");
    return false;
  }

  impl.touchIndev = lv_indev_create();
  if (impl.touchIndev == nullptr) {
    Serial.println("[SZPI.TOUCH] lv_indev_create failed");
    return false;
  }

  lv_indev_set_type(impl.touchIndev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_display(impl.touchIndev, impl.display);
  lv_indev_set_driver_data(impl.touchIndev, &impl);
  lv_indev_set_read_cb(impl.touchIndev, touchReadCallback);
  impl.touchReady = true;
  Serial.println("[SZPI.TOUCH] FT6336 touch input ready");
  return true;
}

void releaseDisplayResources(SzpiLvglDisplayImpl& impl) {
  if (impl.touchIndev != nullptr) {
    lv_indev_delete(impl.touchIndev);
    impl.touchIndev = nullptr;
  }

  if (impl.display != nullptr) {
    lv_display_delete(impl.display);
    impl.display = nullptr;
  }

  if (impl.drawBuffer != nullptr) {
    heap_caps_free(impl.drawBuffer);
    impl.drawBuffer = nullptr;
  }

  if (impl.panel != nullptr) {
    esp_lcd_panel_del(impl.panel);
    impl.panel = nullptr;
  }

  if (impl.io != nullptr) {
    esp_lcd_panel_io_del(impl.io);
    impl.io = nullptr;
  }

  if (impl.spiInitialized) {
    spi_bus_free(board::kLcdSpiHost);
    impl.spiInitialized = false;
  }

}

}  // namespace

struct SzpiLvglDisplay::Impl : SzpiLvglDisplayImpl {};

SzpiLvglDisplay::SzpiLvglDisplay() : impl_(std::make_unique<Impl>()) {}

SzpiLvglDisplay::~SzpiLvglDisplay() {
  if (impl_ == nullptr) {
    return;
  }
  releaseDisplayResources(*impl_);
}

SzpiLvglDisplay::SzpiLvglDisplay(SzpiLvglDisplay&&) noexcept = default;

SzpiLvglDisplay& SzpiLvglDisplay::operator=(SzpiLvglDisplay&&) noexcept = default;

bool SzpiLvglDisplay::init() {
  if (impl_->ready) {
    return true;
  }

  if (!initIoExpander()) {
    Serial.println("[SZPI.ESP_LCD] PCA9557 init failed");
    return false;
  }
  if (!initBacklightPwm()) {
    Serial.println("[SZPI.ESP_LCD] Backlight PWM init failed");
    return false;
  }
  if (!initPanel(*impl_)) {
    Serial.println("[SZPI.ESP_LCD] Panel init failed");
    return false;
  }
  if (!initLvglDisplay(*impl_)) {
    Serial.println("[SZPI.ESP_LCD] LVGL display init failed");
    return false;
  }
  initTouchInput(*impl_);
  if (!setBacklightPercent(100)) {
    Serial.println("[SZPI.ESP_LCD] Failed to enable backlight");
    return false;
  }

  impl_->ready = true;
  return true;
}

lv_display_t* SzpiLvglDisplay::display() const {
  return impl_->display;
}

bool SzpiLvglDisplay::touchReady() const {
  return impl_->touchReady;
}

uint32_t SzpiLvglDisplay::touchTapCount() const {
  return impl_->touchTapCount;
}

infra::TouchPoint SzpiLvglDisplay::lastTouchPoint() const {
  return infra::TouchPoint{
      .x = impl_->touchState.lastPoint.x,
      .y = impl_->touchState.lastPoint.y,
  };
}

bool SzpiLvglDisplay::touchPressed() const {
  return impl_->touchState.pressed;
}
