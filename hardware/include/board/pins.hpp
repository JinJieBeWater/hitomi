#pragma once

#include <driver/i2c.h>
#include <driver/ledc.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>

namespace board {

constexpr int kBootKeyPin = 0;

constexpr i2c_port_t kI2cPort = I2C_NUM_0;
constexpr gpio_num_t kI2cSdaPin = GPIO_NUM_1;
constexpr gpio_num_t kI2cSclPin = GPIO_NUM_2;

constexpr spi_host_device_t kLcdSpiHost = SPI3_HOST;
constexpr gpio_num_t kLcdDcPin = GPIO_NUM_39;
constexpr gpio_num_t kLcdMosiPin = GPIO_NUM_40;
constexpr gpio_num_t kLcdSclkPin = GPIO_NUM_41;
constexpr gpio_num_t kLcdBacklightPin = GPIO_NUM_42;
constexpr uint32_t kLcdPixelClockHz = 80 * 1000 * 1000;
constexpr ledc_channel_t kBacklightChannel = LEDC_CHANNEL_0;
constexpr ledc_timer_t kBacklightTimer = LEDC_TIMER_1;

}  // namespace board
