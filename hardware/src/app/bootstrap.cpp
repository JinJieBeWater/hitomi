#include "app/bootstrap.hpp"

#include "app/app_runtime.hpp"
#include "app/runtime_network_executor.hpp"
#include "board/app_config.hpp"
#include "face/ports.hpp"
#include "infra/display/lvgl_status_display.hpp"
#include "infra/network/http_device_api_client.hpp"
#include "infra/storage/json_local_store.hpp"
#include "infra/storage/sd_mmc_template_store.hpp"

namespace app {

namespace {

AppRuntime& runtime() {
  static infra::LvglStatusDisplay display;
  static infra::JsonLocalStore localStore;
  static infra::HttpDeviceApiClient deviceApiClient(board::apiBaseUrl());
  static std::unique_ptr<RuntimeNetworkExecutor> networkExecutor = createRuntimeNetworkExecutor(deviceApiClient);
  static infra::SdMmcTemplateStore templateStore;
  static face::NoopCameraPort camera;
  static face::NoopEnrollmentServicePort enrollmentService;
  static face::NoopRecognitionServicePort recognitionService;
  static AppRuntime appRuntime(
      display,
      localStore,
      *networkExecutor,
      templateStore,
      camera,
      enrollmentService,
      recognitionService);
  return appRuntime;
}

}  // namespace

void setupRuntime() {
  runtime().setup();
}

void tickRuntime(uint32_t nowMs) {
  runtime().tick(nowMs);
}

}  // namespace app
