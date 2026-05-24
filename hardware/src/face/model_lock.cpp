#include "face/model_lock.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace face {
namespace {

SemaphoreHandle_t modelMutex() {
  static SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
  return mutex;
}

}  // namespace

bool tryLockModel(uint32_t timeoutMs) {
  SemaphoreHandle_t mutex = modelMutex();
  if (mutex == nullptr) {
    return false;
  }
  return xSemaphoreTake(mutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void unlockModel() {
  SemaphoreHandle_t mutex = modelMutex();
  if (mutex != nullptr) {
    xSemaphoreGive(mutex);
  }
}

}  // namespace face
