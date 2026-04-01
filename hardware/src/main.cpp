#include <Arduino.h>

#include "app/app_runtime.hpp"
#include "board/app_config.hpp"
#include "face/ports.hpp"
#include "infra/display/lvgl_status_display.hpp"
#include "infra/network/http_device_api_client.hpp"
#include "infra/storage/json_local_store.hpp"
#include "infra/template_store_port.hpp"

namespace {

infra::LvglStatusDisplay gDisplay;
infra::JsonLocalStore gLocalStore;
infra::HttpDeviceApiClient gDeviceApiClient(board::apiBaseUrl());
infra::NoopTemplateStorePort gTemplateStore;
face::NoopCameraPort gCamera;
face::NoopEnrollmentServicePort gEnrollmentService;
face::NoopRecognitionServicePort gRecognitionService;
app::AppRuntime gRuntime(
    gDisplay,
    gLocalStore,
    gDeviceApiClient,
    gTemplateStore,
    gCamera,
    gEnrollmentService,
    gRecognitionService);

}  // namespace

void setup() {
  gRuntime.setup();
}

void loop() {
  gRuntime.tick(millis());
  delay(board::kLoopYieldDelayMs);
}
