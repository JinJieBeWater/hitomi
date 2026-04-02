#include <Arduino.h>

#include "board/pins.hpp"
#include "infra/display/szpi_lvgl_display.hpp"

namespace {

bool logEspError(esp_err_t err, const char* action) {
  if (err == ESP_OK) {
    return true;
  }

  Serial.printf("[SZPI.ESP_LCD] %s failed: %s\n", action, esp_err_to_name(err));
  return false;
}

}  // namespace

bool SzpiLvglDisplay::init() {
  if (_ready) {
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

  if (!initPanel()) {
    Serial.println("[SZPI.ESP_LCD] Panel init failed");
    return false;
  }

  if (!initLvglDisplay()) {
    Serial.println("[SZPI.ESP_LCD] LVGL display init failed");
    return false;
  }

  if (!setBacklightPercent(100)) {
    Serial.println("[SZPI.ESP_LCD] Failed to enable backlight");
    return false;
  }

  _ready = true;
  return true;
}

lv_display_t* SzpiLvglDisplay::display() const {
  return _display;
}

bool SzpiLvglDisplay::setBacklightPercent(int percent) {
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

void SzpiLvglDisplay::flushCallback(lv_display_t* display, const lv_area_t* area, uint8_t* pxMap) {
  auto* driver = static_cast<SzpiLvglDisplay*>(lv_display_get_driver_data(display));
  if (driver == nullptr) {
    lv_display_flush_ready(display);
    return;
  }

  driver->flush(area, pxMap);
}

bool SzpiLvglDisplay::onColorTransferDone(
    esp_lcd_panel_io_handle_t panelIo, esp_lcd_panel_io_event_data_t* eventData, void* userCtx) {
  static_cast<void>(panelIo);
  static_cast<void>(eventData);

  auto* driver = static_cast<SzpiLvglDisplay*>(userCtx);
  if (driver != nullptr && driver->_display != nullptr) {
    lv_display_flush_ready(driver->_display);
  }

  return false;
}

bool SzpiLvglDisplay::initIoExpander() {
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

  if (!writePca9557Register(kPca9557OutputReg, kPca9557DefaultOutput)) {
    return false;
  }

  if (!writePca9557Register(kPca9557ConfigReg, kPca9557DefaultConfig)) {
    return false;
  }

  return true;
}

bool SzpiLvglDisplay::initBacklightPwm() {
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

  if (!logEspError(ledc_timer_config(&timerCfg), "ledc_timer_config")) {
    return false;
  }

  if (!logEspError(ledc_channel_config(&channelCfg), "ledc_channel_config")) {
    return false;
  }

  return true;
}

bool SzpiLvglDisplay::initPanel() {
  spi_bus_config_t busCfg = {};
  busCfg.sclk_io_num = board::kLcdSclkPin;
  busCfg.mosi_io_num = board::kLcdMosiPin;
  busCfg.miso_io_num = GPIO_NUM_NC;
  busCfg.quadwp_io_num = GPIO_NUM_NC;
  busCfg.quadhd_io_num = GPIO_NUM_NC;
  busCfg.max_transfer_sz = kHorizontalResolution * kBufferRows * sizeof(uint16_t);

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
  ioCfg.user_ctx = this;

  err = esp_lcd_new_panel_io_spi(
      reinterpret_cast<esp_lcd_spi_bus_handle_t>(board::kLcdSpiHost), &ioCfg, &_io);
  if (!logEspError(err, "esp_lcd_new_panel_io_spi")) {
    return false;
  }

  esp_lcd_panel_dev_config_t panelCfg = {};
  panelCfg.reset_gpio_num = GPIO_NUM_NC;
  panelCfg.color_space = ESP_LCD_COLOR_SPACE_RGB;
  panelCfg.bits_per_pixel = 16;

  if (!logEspError(esp_lcd_new_panel_st7789(_io, &panelCfg, &_panel), "esp_lcd_new_panel_st7789")) {
    return false;
  }

  if (!logEspError(esp_lcd_panel_reset(_panel), "esp_lcd_panel_reset")) {
    return false;
  }

  if (!setPca9557OutputState(kLcdCsBit, false)) {
    Serial.println("[SZPI.ESP_LCD] Failed to pull LCD_CS low");
    return false;
  }

  if (!logEspError(esp_lcd_panel_init(_panel), "esp_lcd_panel_init")) {
    return false;
  }

  if (!logEspError(esp_lcd_panel_invert_color(_panel, true), "esp_lcd_panel_invert_color")) {
    return false;
  }

  if (!logEspError(esp_lcd_panel_swap_xy(_panel, true), "esp_lcd_panel_swap_xy")) {
    return false;
  }

  if (!logEspError(esp_lcd_panel_mirror(_panel, true, false), "esp_lcd_panel_mirror")) {
    return false;
  }

  if (!logEspError(esp_lcd_panel_disp_on_off(_panel, true), "esp_lcd_panel_disp_on_off")) {
    return false;
  }

  return true;
}

bool SzpiLvglDisplay::initLvglDisplay() {
  _drawBuffer = static_cast<uint16_t*>(heap_caps_malloc(
      kHorizontalResolution * kBufferRows * sizeof(uint16_t),
      MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (_drawBuffer == nullptr) {
    Serial.println("[SZPI.ESP_LCD] Failed to allocate LVGL draw buffer");
    return false;
  }

  _display = lv_display_create(kHorizontalResolution, kVerticalResolution);
  if (_display == nullptr) {
    Serial.println("[SZPI.ESP_LCD] lv_display_create failed");
    return false;
  }

  lv_display_set_driver_data(_display, this);
  lv_display_set_color_format(_display, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_flush_cb(_display, flushCallback);
  lv_display_set_buffers(
      _display,
      _drawBuffer,
      nullptr,
      kHorizontalResolution * kBufferRows * sizeof(uint16_t),
      LV_DISPLAY_RENDER_MODE_PARTIAL);

  return true;
}

bool SzpiLvglDisplay::writePca9557Register(uint8_t reg, uint8_t value) {
  uint8_t payload[] = {reg, value};
  esp_err_t err = i2c_master_write_to_device(
      board::kI2cPort, kPca9557Address, payload, sizeof(payload), kI2cTimeout);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] PCA9557 write reg=0x%02X value=0x%02X failed: %s\n", reg,
                  value, esp_err_to_name(err));
    return false;
  }

  return true;
}

bool SzpiLvglDisplay::setPca9557OutputState(uint8_t gpioBit, bool level) {
  uint8_t output = kPca9557DefaultOutput;
  if (level) {
    output |= gpioBit;
  } else {
    output &= static_cast<uint8_t>(~gpioBit);
  }

  return writePca9557Register(kPca9557OutputReg, output);
}

void SzpiLvglDisplay::flush(const lv_area_t* area, uint8_t* pxMap) {
  esp_err_t err = esp_lcd_panel_draw_bitmap(
      _panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, pxMap);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] draw_bitmap failed: %s\n", esp_err_to_name(err));
    lv_display_flush_ready(_display);
  }
}
