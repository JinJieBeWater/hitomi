#include <Arduino.h>

#include "app/bootstrap.hpp"
#include "board/app_config.hpp"

void setup() {
  app::setupRuntime();
}

void loop() {
  app::tickRuntime(millis());
  delay(board::kLoopYieldDelayMs);
}
