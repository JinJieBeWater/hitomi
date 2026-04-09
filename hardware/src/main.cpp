#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/bootstrap.hpp"
#include "board/app_config.hpp"

namespace {

// Boot brings up SD/MMC, LVGL, WiFi, and the runtime state machine on one task.
constexpr uint32_t kRuntimeTaskStackSize = 24 * 1024;
constexpr UBaseType_t kRuntimeTaskPriority = 1;
constexpr BaseType_t kRuntimeTaskCore = 1;

[[noreturn]] void runRuntimeLoop() {
  while (true) {
    app::tickRuntime(millis());
    delay(board::kLoopYieldDelayMs);
  }
}

void runtimeTask(void* /*unused*/) {
  app::setupRuntime();
  runRuntimeLoop();
}

}  // namespace

extern "C" void app_main() {
  initArduino();

  const BaseType_t created = xTaskCreatePinnedToCore(
      runtimeTask,
      "hitomi_runtime",
      kRuntimeTaskStackSize,
      nullptr,
      kRuntimeTaskPriority,
      nullptr,
      kRuntimeTaskCore);
  if (created != pdPASS) {
    Serial.println("[APP] failed to create hitomi_runtime task");
    abort();
  }

  vTaskDelete(nullptr);
}
