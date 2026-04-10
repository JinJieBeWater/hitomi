#pragma once

#include <cstddef>
#include <cstdint>

namespace infra {

bool ensureBoardControlBusReady();
bool ensureBoardIoExpanderReady();
bool probeBoardControlDevice(uint8_t address, int timeoutMs);
bool readBoardControlRegisters(uint8_t address, uint8_t startReg, uint8_t* buffer, std::size_t length, int timeoutMs);
bool writeBoardControlRegister(uint8_t address, uint8_t reg, uint8_t value, int timeoutMs);
bool updateBoardControlRegisterBit(
    uint8_t address,
    uint8_t reg,
    uint8_t bitMask,
    bool level,
    int timeoutMs,
    uint8_t* newValue = nullptr);

}  // namespace infra
