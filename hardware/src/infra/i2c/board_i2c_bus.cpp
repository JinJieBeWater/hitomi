#include "infra/i2c/board_i2c_bus.hpp"

#include <driver/i2c_master.h>
#include <esp_err.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "board/pins.hpp"

namespace infra {
namespace {

constexpr uint32_t kBoardI2cClockHz = 100000;
constexpr uint8_t kMaxBoardI2cDevices = 4;

struct DeviceSlot {
  uint8_t address = 0;
  i2c_master_dev_handle_t handle = nullptr;
};

struct BoardI2cBusState {
  i2c_master_bus_handle_t busHandle = nullptr;
  std::array<DeviceSlot, kMaxBoardI2cDevices> devices = {};
  bool busReady = false;
  bool ioExpanderReady = false;
};

BoardI2cBusState& boardI2cBusState() {
  static auto* state = new BoardI2cBusState();
  return *state;
}

DeviceSlot* findDeviceSlot(BoardI2cBusState& state, uint8_t address) {
  for (auto& slot : state.devices) {
    if (slot.handle != nullptr && slot.address == address) {
      return &slot;
    }
  }
  return nullptr;
}

DeviceSlot* allocateDeviceSlot(BoardI2cBusState& state) {
  for (auto& slot : state.devices) {
    if (slot.handle == nullptr) {
      return &slot;
    }
  }
  return nullptr;
}

i2c_master_dev_handle_t deviceHandle(BoardI2cBusState& state, uint8_t address) {
  if (!state.busReady || state.busHandle == nullptr) {
    return nullptr;
  }

  if (auto* existing = findDeviceSlot(state, address); existing != nullptr) {
    return existing->handle;
  }

  auto* freeSlot = allocateDeviceSlot(state);
  if (freeSlot == nullptr) {
    return nullptr;
  }

  i2c_device_config_t deviceConfig = {};
  deviceConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  deviceConfig.device_address = address;
  deviceConfig.scl_speed_hz = kBoardI2cClockHz;

  if (i2c_master_bus_add_device(state.busHandle, &deviceConfig, &freeSlot->handle) != ESP_OK) {
    freeSlot->handle = nullptr;
    freeSlot->address = 0;
    return nullptr;
  }

  freeSlot->address = address;
  return freeSlot->handle;
}

}  // namespace

bool ensureBoardI2cBusInitialized() {
  BoardI2cBusState& state = boardI2cBusState();
  if (state.busReady && state.busHandle != nullptr) {
    return true;
  }

  i2c_master_bus_config_t busConfig = {};
  busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  busConfig.i2c_port = board::kI2cPort;
  busConfig.sda_io_num = board::kI2cSdaPin;
  busConfig.scl_io_num = board::kI2cSclPin;
  busConfig.glitch_ignore_cnt = 7;
  busConfig.flags.enable_internal_pullup = true;

  esp_err_t err = i2c_new_master_bus(&busConfig, &state.busHandle);
  if (err == ESP_ERR_INVALID_STATE) {
    err = i2c_master_get_bus_handle(board::kI2cPort, &state.busHandle);
  }
  if (err != ESP_OK || state.busHandle == nullptr) {
    return false;
  }

  state.busReady = true;
  return true;
}

bool ensureBoardIoExpanderInitialized() {
  BoardI2cBusState& state = boardI2cBusState();
  if (state.ioExpanderReady) {
    return true;
  }
  if (!ensureBoardI2cBusInitialized()) {
    return false;
  }

  if (!writeBoardI2cRegister(
          board::kIoExpanderAddress,
          board::kIoExpanderOutputReg,
          board::kIoExpanderDefaultOutput,
          1000)) {
    return false;
  }
  if (!writeBoardI2cRegister(
          board::kIoExpanderAddress,
          board::kIoExpanderConfigReg,
          board::kIoExpanderDefaultConfig,
          1000)) {
    return false;
  }

  state.ioExpanderReady = true;
  return true;
}

bool probeBoardI2cDevice(uint8_t address, int timeoutMs) {
  BoardI2cBusState& state = boardI2cBusState();
  if (!ensureBoardI2cBusInitialized() || state.busHandle == nullptr) {
    return false;
  }

  return i2c_master_probe(state.busHandle, address, timeoutMs) == ESP_OK;
}

bool readBoardI2cRegisters(uint8_t address, uint8_t startReg, uint8_t* buffer, std::size_t length, int timeoutMs) {
  BoardI2cBusState& state = boardI2cBusState();
  if (!ensureBoardI2cBusInitialized()) {
    return false;
  }

  i2c_master_dev_handle_t handle = deviceHandle(state, address);
  if (handle == nullptr) {
    return false;
  }

  return i2c_master_transmit_receive(handle, &startReg, sizeof(startReg), buffer, length, timeoutMs) == ESP_OK;
}

bool writeBoardI2cRegister(uint8_t address, uint8_t reg, uint8_t value, int timeoutMs) {
  BoardI2cBusState& state = boardI2cBusState();
  if (!ensureBoardI2cBusInitialized()) {
    return false;
  }

  i2c_master_dev_handle_t handle = deviceHandle(state, address);
  if (handle == nullptr) {
    return false;
  }

  uint8_t payload[] = {reg, value};
  return i2c_master_transmit(handle, payload, sizeof(payload), timeoutMs) == ESP_OK;
}

bool updateBoardI2cRegisterBit(
    uint8_t address,
    uint8_t reg,
    uint8_t bitMask,
    bool level,
    int timeoutMs,
    uint8_t* newValue) {
  uint8_t currentValue = 0;
  if (!readBoardI2cRegisters(address, reg, &currentValue, sizeof(currentValue), timeoutMs)) {
    return false;
  }

  if (level) {
    currentValue |= bitMask;
  } else {
    currentValue &= static_cast<uint8_t>(~bitMask);
  }

  if (!writeBoardI2cRegister(address, reg, currentValue, timeoutMs)) {
    return false;
  }

  if (newValue != nullptr) {
    *newValue = currentValue;
  }
  return true;
}

}  // namespace infra
