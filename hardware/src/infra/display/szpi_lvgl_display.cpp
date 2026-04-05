#include "infra/display/szpi_lvgl_display.hpp"

#include <Arduino.h>

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

#include <memory>
#include <utility>

#include "board/pins.hpp"

namespace {

constexpr TickType_t kI2cTimeout = pdMS_TO_TICKS(1000);

constexpr uint8_t kPca9557Address = 0x19;
constexpr uint8_t kPca9557OutputReg = 0x01;
constexpr uint8_t kPca9557ConfigReg = 0x03;
constexpr uint8_t kPca9557DefaultOutput = 0x05;
constexpr uint8_t kPca9557DefaultConfig = 0xF8;
constexpr uint8_t kLcdCsBit = 1u << 0;

struct SzpiLvglDisplayImpl {
  esp_lcd_panel_io_handle_t io = nullptr;
  esp_lcd_panel_handle_t panel = nullptr;
  lv_display_t* display = nullptr;
  uint16_t* drawBuffer = nullptr;
  bool ready = false;
};

bool logEspError(esp_err_t err, const char* action) {
  if (err == ESP_OK) {
    return true;
  }

  Serial.printf("[SZPI.ESP_LCD] %s failed: %s\n", action, esp_err_to_name(err));
  return false;
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

bool writePca9557Register(uint8_t reg, uint8_t value) {
  uint8_t payload[] = {reg, value};
  esp_err_t err =
      i2c_master_write_to_device(board::kI2cPort, kPca9557Address, payload, sizeof(payload), kI2cTimeout);
  if (err != ESP_OK) {
    Serial.printf(
        "[SZPI.ESP_LCD] PCA9557 write reg=0x%02X value=0x%02X failed: %s\n",
        reg,
        value,
        esp_err_to_name(err));
    return false;
  }

  return true;
}

bool setPca9557OutputState(uint8_t gpioBit, bool level) {
  uint8_t output = kPca9557DefaultOutput;
  if (level) {
    output |= gpioBit;
  } else {
    output &= static_cast<uint8_t>(~gpioBit);
  }

  return writePca9557Register(kPca9557OutputReg, output);
}

bool initIoExpander() {
  i2c_config_t cfg = {};
  cfg.mode = I2C_MODE_MASTER;
  cfg.sda_io_num = board::kI2cSdaPin;
  cfg.scl_io_num = board::kI2cSclPin;
  cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
  cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
  cfg.master.clk_speed = 100000;

  if (!logEspError(i2c_param_config(board::kI2cPort, &cfg), "i2c_param_config")) {
    return false;
  }

  esp_err_t err = i2c_driver_install(board::kI2cPort, cfg.mode, 0, 0, 0);
  if (err == ESP_ERR_INVALID_STATE) {
    i2c_driver_delete(board::kI2cPort);
    err = i2c_driver_install(board::kI2cPort, cfg.mode, 0, 0, 0);
  }

  if (!logEspError(err, "i2c_driver_install")) {
    return false;
  }

  return writePca9557Register(kPca9557OutputReg, kPca9557DefaultOutput) &&
         writePca9557Register(kPca9557ConfigReg, kPca9557DefaultConfig);
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
      reinterpret_cast<esp_lcd_spi_bus_handle_t>(board::kLcdSpiHost), &ioCfg, &impl.io);
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
  if (!setPca9557OutputState(kLcdCsBit, false)) {
    Serial.println("[SZPI.ESP_LCD] Failed to pull LCD_CS low");
    return false;
  }
  if (!logEspError(esp_lcd_panel_init(impl.panel), "esp_lcd_panel_init")) {
    return false;
  }
  if (!logEspError(esp_lcd_panel_invert_color(impl.panel, true), "esp_lcd_panel_invert_color")) {
    return false;
  }
  if (!logEspError(esp_lcd_panel_swap_xy(impl.panel, true), "esp_lcd_panel_swap_xy")) {
    return false;
  }
  if (!logEspError(esp_lcd_panel_mirror(impl.panel, true, false), "esp_lcd_panel_mirror")) {
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

}  // namespace

struct SzpiLvglDisplay::Impl : SzpiLvglDisplayImpl {};

SzpiLvglDisplay::SzpiLvglDisplay() : impl_(std::make_unique<Impl>()) {}

SzpiLvglDisplay::~SzpiLvglDisplay() = default;

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
