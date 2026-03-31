#include <Arduino.h>

#include <szpi_lvgl_display.hpp>

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

bool SzpiLvglDisplay::ready() const {
  return _ready;
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
  esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, kBacklightChannel, duty);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] ledc_set_duty failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = ledc_update_duty(LEDC_LOW_SPEED_MODE, kBacklightChannel);
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
  cfg.sda_io_num = kI2cSdaPin;
  cfg.scl_io_num = kI2cSclPin;
  cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
  cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
  cfg.master.clk_speed = 100000;

  esp_err_t err = i2c_param_config(kI2cPort, &cfg);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] i2c_param_config failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = i2c_driver_install(kI2cPort, cfg.mode, 0, 0, 0);
  if (err == ESP_ERR_INVALID_STATE) {
    i2c_driver_delete(kI2cPort);
    err = i2c_driver_install(kI2cPort, cfg.mode, 0, 0, 0);
  }

  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] i2c_driver_install failed: %s\n", esp_err_to_name(err));
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
  channelCfg.gpio_num = kLcdBacklightPin;
  channelCfg.speed_mode = LEDC_LOW_SPEED_MODE;
  channelCfg.channel = kBacklightChannel;
  channelCfg.intr_type = LEDC_INTR_DISABLE;
  channelCfg.timer_sel = kBacklightTimer;
  channelCfg.duty = 0;
  channelCfg.hpoint = 0;
  channelCfg.flags.output_invert = true;

  ledc_timer_config_t timerCfg = {};
  timerCfg.speed_mode = LEDC_LOW_SPEED_MODE;
  timerCfg.duty_resolution = LEDC_TIMER_10_BIT;
  timerCfg.timer_num = kBacklightTimer;
  timerCfg.freq_hz = 5000;
  timerCfg.clk_cfg = LEDC_AUTO_CLK;

  esp_err_t err = ledc_timer_config(&timerCfg);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] ledc_timer_config failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = ledc_channel_config(&channelCfg);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] ledc_channel_config failed: %s\n", esp_err_to_name(err));
    return false;
  }

  return true;
}

bool SzpiLvglDisplay::initPanel() {
  spi_bus_config_t busCfg = {};
  busCfg.sclk_io_num = kLcdSclkPin;
  busCfg.mosi_io_num = kLcdMosiPin;
  busCfg.miso_io_num = GPIO_NUM_NC;
  busCfg.quadwp_io_num = GPIO_NUM_NC;
  busCfg.quadhd_io_num = GPIO_NUM_NC;
  busCfg.max_transfer_sz =
      kHorizontalResolution * kBufferRows * sizeof(uint16_t);

  esp_err_t err = spi_bus_initialize(kLcdSpiHost, &busCfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    Serial.printf("[SZPI.ESP_LCD] spi_bus_initialize failed: %s\n", esp_err_to_name(err));
    return false;
  }

  esp_lcd_panel_io_spi_config_t ioCfg = {};
  ioCfg.dc_gpio_num = kLcdDcPin;
  ioCfg.cs_gpio_num = GPIO_NUM_NC;
  ioCfg.pclk_hz = kPixelClockHz;
  ioCfg.lcd_cmd_bits = 8;
  ioCfg.lcd_param_bits = 8;
  ioCfg.spi_mode = 2;
  ioCfg.trans_queue_depth = 1;
  ioCfg.on_color_trans_done = onColorTransferDone;
  ioCfg.user_ctx = this;

  err = esp_lcd_new_panel_io_spi(
      reinterpret_cast<esp_lcd_spi_bus_handle_t>(kLcdSpiHost), &ioCfg, &_io);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] esp_lcd_new_panel_io_spi failed: %s\n",
                  esp_err_to_name(err));
    return false;
  }

  esp_lcd_panel_dev_config_t panelCfg = {};
  panelCfg.reset_gpio_num = GPIO_NUM_NC;
  panelCfg.color_space = ESP_LCD_COLOR_SPACE_RGB;
  panelCfg.bits_per_pixel = 16;

  err = esp_lcd_new_panel_st7789(_io, &panelCfg, &_panel);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] esp_lcd_new_panel_st7789 failed: %s\n",
                  esp_err_to_name(err));
    return false;
  }

  err = esp_lcd_panel_reset(_panel);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] esp_lcd_panel_reset failed: %s\n", esp_err_to_name(err));
    return false;
  }

  if (!setPca9557OutputState(kLcdCsBit, false)) {
    Serial.println("[SZPI.ESP_LCD] Failed to pull LCD_CS low");
    return false;
  }

  err = esp_lcd_panel_init(_panel);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] esp_lcd_panel_init failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = esp_lcd_panel_invert_color(_panel, true);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] esp_lcd_panel_invert_color failed: %s\n",
                  esp_err_to_name(err));
    return false;
  }

  err = esp_lcd_panel_swap_xy(_panel, true);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] esp_lcd_panel_swap_xy failed: %s\n",
                  esp_err_to_name(err));
    return false;
  }

  err = esp_lcd_panel_mirror(_panel, true, false);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] esp_lcd_panel_mirror failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = esp_lcd_panel_disp_on_off(_panel, true);
  if (err != ESP_OK) {
    Serial.printf("[SZPI.ESP_LCD] esp_lcd_panel_disp_on_off failed: %s\n",
                  esp_err_to_name(err));
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
  lv_display_set_color_format(_display, LV_COLOR_FORMAT_RGB565);
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
      kI2cPort, kPca9557Address, payload, sizeof(payload), kI2cTimeout);
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
