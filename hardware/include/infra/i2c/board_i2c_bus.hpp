#pragma once

#include <cstddef>
#include <cstdint>

namespace infra {

bool ensureBoardI2cBusInitialized();
bool ensureBoardIoExpanderInitialized();
bool probeBoardI2cDevice(uint8_t address, int timeoutMs);
bool readBoardI2cRegisters(uint8_t address, uint8_t startReg, uint8_t* buffer, std::size_t length, int timeoutMs);
bool writeBoardI2cRegister(uint8_t address, uint8_t reg, uint8_t value, int timeoutMs);
bool updateBoardI2cRegisterBit(
    uint8_t address,
    uint8_t reg,
    uint8_t bitMask,
    bool level,
    int timeoutMs,
    uint8_t* newValue = nullptr);

}  // namespace infra
