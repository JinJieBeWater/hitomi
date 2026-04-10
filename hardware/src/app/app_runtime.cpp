#include "app/app_runtime.hpp"

#include <Arduino.h>

#include <memory>

#include "app/runtime_camera_ops.hpp"
#include "app/runtime_lifecycle_service.hpp"
#include "app/runtime_network_executor.hpp"
#include "app/runtime_network_ops.hpp"
#include "app/runtime_provisioning_ops.hpp"
#include "app/runtime_state.hpp"
#include "app/runtime_storage_ops.hpp"
#include "app/runtime_view_ops.hpp"
#include "board/app_config.hpp"
#include "board/pins.hpp"
#include "face/ports.hpp"
#include "infra/display_port.hpp"
#include "infra/local_store.hpp"
#include "infra/template_store_port.hpp"

namespace app {

AppRuntime::AppRuntime(
    infra::DisplayPort& display,
    infra::LocalStore& localStore,
    RuntimeNetworkExecutor& networkExecutor,
    infra::TemplateStorePort& templateStore,
    face::CameraPort& camera,
    face::EnrollmentServicePort& enrollmentService,
    face::RecognitionServicePort& recognitionService)
    : context_(std::make_unique<RuntimeContext>(RuntimeContext{
          .display = display,
          .localStore = localStore,
          .networkExecutor = networkExecutor,
          .templateStore = templateStore,
          .camera = camera,
          .enrollmentService = enrollmentService,
          .recognitionService = recognitionService,
      })),
      state_(std::make_unique<RuntimeState>()) {}

AppRuntime::~AppRuntime() = default;

void AppRuntime::setup() {
  RuntimeContext& context = *context_;
  RuntimeState& state = *state_;

  pinMode(board::kBootKeyPin, INPUT_PULLUP);

  Serial.begin(board::kSerialBaudRate);
  waitForSerial();

  initializeLocalStore(context, state);
  loadPersistedState(context, state);
  seedDeviceConfigFromBoardDefaults(context, state);
  if (!context.networkExecutor.start()) {
    Serial.println("[APP] network executor init failed");
    state.lastErrorCode = "NETWORK_EXECUTOR_INIT_FAILED";
  }
  if (board::kEnableTemplateStore) {
    initializeTemplateStore(context, state);
    runTemplateStoreSelfTest(context, state);
  } else {
    state.storageAux = {};
    state.storageAux.templateStoreHealth.statusCode = infra::kTemplateStoreDisabled;
    state.storageAux.templateStoreHealth.checkedAt = static_cast<uint64_t>(millis());
    state.storageAux.templateStoreHealth.detail = "disabled by board config";
    state.templateStoreReady = false;
    state.faceModuleEnabled = false;
    persistStorageAux(context, state);
    state.renderDirty = true;
    Serial.println("[APP] template store disabled on current runtime");
  }
  initWifi();

  state.displayReady = context.display.init();
  if (!state.displayReady) {
    Serial.println("[APP] display init failed");
  }
  initializeCamera(context, state);

  const uint32_t nowMs = millis();
  state.lastButtonPollMs = nowMs;
  state.lastCameraPollMs = nowMs;
  state.lastCameraStatusRenderMs = nowMs;
  state.lastNetworkProbeMs = nowMs;
  state.lastTemplateStoreProbeMs = nowMs;
  state.lastButtonPressed = digitalRead(board::kBootKeyPin) == LOW;

  processWifiDriverEvents(context, state, nowMs);
  ensureWifiConnection(context, state, nowMs);
  probeConnectivity(context, state, nowMs);
  printRuntimeCheck(context, state);
  updateView(context, state);
}

void AppRuntime::tick(uint32_t nowMs) {
  RuntimeContext& context = *context_;
  RuntimeState& state = *state_;

  context.display.tick(nowMs);
  processUsbProvisioning(context, state, nowMs);
  pollBootButton(state, nowMs);
  processWifiDriverEvents(context, state, nowMs);
  ensureWifiConnection(context, state, nowMs);
  consumeCompletedNetworkRequest(context, state, nowMs);

  while (const auto command = context.display.consumeCommand()) {
    handleDisplayCommand(context, state, *command, nowMs);
  }

  pollCamera(context, state, nowMs);

  if (nowMs - state.lastNetworkProbeMs >= board::kNetworkProbeIntervalMs) {
    probeConnectivity(context, state, nowMs);
  }

  if (board::kEnableTemplateStore) {
    probeTemplateStore(context, state, nowMs);
  }

  dispatchNetworkRequest(context, state, nowMs);

  if (state.renderDirty) {
    updateView(context, state);
  }
}

}  // namespace app
