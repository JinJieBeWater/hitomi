#pragma once

#include <cstdint>

namespace face {

bool tryLockModel(uint32_t timeoutMs);
void unlockModel();

}  // namespace face
