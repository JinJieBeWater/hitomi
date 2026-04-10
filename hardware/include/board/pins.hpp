#pragma once

#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/ledc.h>
#include <driver/spi_master.h>

namespace board {

constexpr int kBootKeyPin = 0;

constexpr i2c_port_t kI2cPort = I2C_NUM_0;
constexpr gpio_num_t kI2cSdaPin = GPIO_NUM_1;
constexpr gpio_num_t kI2cSclPin = GPIO_NUM_2;
constexpr uint8_t kIoExpanderAddress = 0x19;
constexpr uint8_t kIoExpanderOutputReg = 0x01;
constexpr uint8_t kIoExpanderConfigReg = 0x03;
constexpr uint8_t kIoExpanderDefaultOutput = 0x05;
constexpr uint8_t kIoExpanderDefaultConfig = 0xF8;
constexpr uint8_t kIoExpanderLcdCsBit = 1u << 0;
constexpr uint8_t kIoExpanderCameraPwdnBit = 1u << 2;

constexpr spi_host_device_t kLcdSpiHost = SPI3_HOST;
constexpr gpio_num_t kLcdDcPin = GPIO_NUM_39;
constexpr gpio_num_t kLcdMosiPin = GPIO_NUM_40;
constexpr gpio_num_t kLcdSclkPin = GPIO_NUM_41;
constexpr gpio_num_t kLcdBacklightPin = GPIO_NUM_42;
constexpr uint32_t kLcdPixelClockHz = 80 * 1000 * 1000;
constexpr ledc_channel_t kBacklightChannel = LEDC_CHANNEL_0;
constexpr ledc_timer_t kBacklightTimer = LEDC_TIMER_1;

constexpr gpio_num_t kCameraData0Pin = GPIO_NUM_16;
constexpr gpio_num_t kCameraData1Pin = GPIO_NUM_18;
constexpr gpio_num_t kCameraData2Pin = GPIO_NUM_8;
constexpr gpio_num_t kCameraData3Pin = GPIO_NUM_17;
constexpr gpio_num_t kCameraData4Pin = GPIO_NUM_15;
constexpr gpio_num_t kCameraData5Pin = GPIO_NUM_6;
constexpr gpio_num_t kCameraData6Pin = GPIO_NUM_4;
constexpr gpio_num_t kCameraData7Pin = GPIO_NUM_9;
constexpr gpio_num_t kCameraXclkPin = GPIO_NUM_5;
constexpr gpio_num_t kCameraPclkPin = GPIO_NUM_7;
constexpr gpio_num_t kCameraVsyncPin = GPIO_NUM_3;
constexpr gpio_num_t kCameraHrefPin = GPIO_NUM_46;
constexpr gpio_num_t kCameraSccbSdaPin = kI2cSdaPin;
constexpr gpio_num_t kCameraSccbSclPin = kI2cSclPin;
constexpr ledc_channel_t kCameraXclkChannel = LEDC_CHANNEL_1;
constexpr ledc_timer_t kCameraXclkTimer = LEDC_TIMER_0;

constexpr gpio_num_t kSdMmcClkPin = GPIO_NUM_47;
constexpr gpio_num_t kSdMmcCmdPin = GPIO_NUM_48;
constexpr gpio_num_t kSdMmcD0Pin = GPIO_NUM_21;

}  // namespace board
