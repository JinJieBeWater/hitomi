#include "app/app_runtime.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_ota_ops.h>

#include <algorithm>
#include <utility>

#include "app/runtime_diagnostics.hpp"
#include "app/runtime_status.hpp"
#include "board/app_config.hpp"
#include "board/pins.hpp"
#include "core/use_cases.hpp"
#include "ui/status_screen_presenter.hpp"

namespace app {
namespace {

bool facePortsReady(
    const face::CameraPort& camera,
    const face::EnrollmentServicePort& enrollmentService,
    const face::RecognitionServicePort& recognitionService) {
  return camera.available() && enrollmentService.available() && recognitionService.available();
}

}  // namespace

AppRuntime::AppRuntime(
    infra::DisplayPort& display,
    infra::LocalStore& localStore,
    infra::DeviceApiClient& deviceApiClient,
    infra::TemplateStorePort& templateStore,
    face::CameraPort& camera,
    face::EnrollmentServicePort& enrollmentService,
    face::RecognitionServicePort& recognitionService)
    : display_(display),
      localStore_(localStore),
      deviceApiClient_(deviceApiClient),
      templateStore_(templateStore),
      camera_(camera),
      enrollmentService_(enrollmentService),
      recognitionService_(recognitionService) {}

void AppRuntime::setup() {
  pinMode(board::kBootKeyPin, INPUT_PULLUP);

  Serial.begin(board::kSerialBaudRate);
  waitForSerial();

  initStorage();
  loadPersistedState();
  initWifi();

  faceModuleEnabled_ = templateStoreReady_ && facePortsReady(camera_, enrollmentService_, recognitionService_);

  displayReady_ = display_.init();
  if (!displayReady_) {
    Serial.println("[APP] display init failed");
  }

  const uint32_t nowMs = millis();
  lastButtonPollMs_ = nowMs;
  lastNetworkProbeMs_ = nowMs;
  lastButtonPressed_ = digitalRead(board::kBootKeyPin) == LOW;

  probeConnectivity(nowMs);
  printRuntimeCheck();
  updateView();
}

void AppRuntime::initStorage() {
  const infra::LocalStoreInitStatus initStatus = localStore_.begin();
  credentialsReady_ = initStatus.credentialsReady;
  filesystemReady_ = initStatus.filesystemReady;
  templateStoreReady_ = templateStore_.begin();

  if (!credentialsReady_) {
    Serial.println("[APP] credentials store unavailable");
  }
  if (!filesystemReady_) {
    Serial.println("[APP] LittleFS unavailable");
  }
  if (!templateStoreReady_) {
    Serial.println("[APP] template store unavailable");
  }
}

void AppRuntime::tick(uint32_t nowMs) {
  display_.tick(nowMs);
  pollBootButton(nowMs);

  if (nowMs - lastNetworkProbeMs_ >= board::kNetworkProbeIntervalMs) {
    probeConnectivity(nowMs);
  }

  if (shouldSync(nowMs)) {
    performSync(nowMs);
  }

  if (shouldUpload(nowMs)) {
    performUpload(nowMs);
  }

  if (renderDirty_) {
    updateView();
  }
}

void AppRuntime::waitForSerial() {
  const unsigned long startedAt = millis();
  while (!Serial && millis() - startedAt < board::kSerialWaitTimeoutMs) {
    delay(10);
  }
}

void AppRuntime::printRuntimeCheck() {
  const esp_partition_t* partition = esp_ota_get_running_partition();
  const RuntimeDiagnostics diagnostics = buildRuntimeDiagnostics(buildRuntimeStatus());

  Serial.println();
  Serial.println("=== Hitomi Device Runtime ===");
  Serial.printf("FW tag: %s\n", board::kFirmwareTag);
  Serial.printf("Chip model: %s\n", ESP.getChipModel());
  Serial.printf("Chip revision: %d\n", ESP.getChipRevision());
  Serial.printf("SDK version: %s\n", ESP.getSdkVersion());
  Serial.printf("CPU frequency: %lu MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash size: %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("PSRAM detected: %s\n", psramFound() ? "yes" : "no");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
  Serial.printf("Credentials store ready: %s\n", credentialsReady_ ? "yes" : "no");
  Serial.printf("LittleFS ready: %s\n", filesystemReady_ ? "yes" : "no");
  Serial.printf("Template store ready: %s\n", templateStoreReady_ ? "yes" : "no");
  Serial.printf("Display ready: %s\n", displayReady_ ? "yes" : "no");
  Serial.printf("WiFi configured: %s\n", board::wifiConfigured() ? "yes" : "no");
  Serial.printf("API configured: %s\n", board::apiConfigured() ? "yes" : "no");
  Serial.printf("Device credentials configured: %s\n", credentials_.configured() ? "yes" : "no");
  Serial.println(diagnostics.credentialsLine.c_str());
  Serial.println(diagnostics.snapshotLine.c_str());
  Serial.println(diagnostics.queueLine.c_str());
  Serial.println(diagnostics.faceLine.c_str());

  if (partition != nullptr) {
    Serial.printf("Running partition: %s (%u bytes)\n", partition->label, partition->size);
  }

  Serial.println("==============================");
}

void AppRuntime::loadPersistedState() {
  infra::StoredRuntimeState state = localStore_.load();
  credentials_ = state.credentials;
  snapshots_ = state.snapshots;
  pendingAttendanceRecords_ = state.pendingAttendanceRecords;
  failureLogs_ = state.failureLogs;
}

void AppRuntime::initWifi() {
  WiFi.mode(WIFI_STA);
  if (!board::wifiConfigured()) {
    Serial.println("[APP] WiFi credentials not configured");
    return;
  }

  Serial.printf("[APP] connecting WiFi SSID=%s\n", board::wifiSsid());
  WiFi.begin(board::wifiSsid(), board::wifiPassword());
}

void AppRuntime::pollBootButton(uint32_t nowMs) {
  if (nowMs - lastButtonPollMs_ < board::kButtonPollIntervalMs) {
    return;
  }

  lastButtonPollMs_ = nowMs;
  const bool pressed = digitalRead(board::kBootKeyPin) == LOW;
  if (pressed == lastButtonPressed_) {
    return;
  }

  lastButtonPressed_ = pressed;
  Serial.println(pressed ? "BOOT pressed" : "BOOT released");
}

void AppRuntime::probeConnectivity(uint32_t nowMs) {
  lastNetworkProbeMs_ = nowMs;
  const ConnectivityState next =
      WiFi.status() == WL_CONNECTED ? ConnectivityState::Connected : ConnectivityState::Disconnected;
  if (next != connectivity_) {
    connectivity_ = next;
    renderDirty_ = true;

    if (connectivity_ == ConnectivityState::Connected) {
      resetNetworkTaskSchedule();
    }
  }
}

void AppRuntime::resetNetworkTaskSchedule() {
  lastSyncAttemptMs_ = 0;
  lastUploadAttemptMs_ = 0;
}

bool AppRuntime::shouldSync(uint32_t nowMs) const {
  if (syncInFlight_ || connectivity_ != ConnectivityState::Connected) {
    return false;
  }
  if (!credentials_.configured() || !deviceApiClient_.configured()) {
    return false;
  }
  if (lastSyncAttemptMs_ == 0) {
    return true;
  }
  return nowMs - lastSyncAttemptMs_ >= board::kSyncIntervalMs;
}

bool AppRuntime::shouldUpload(uint32_t nowMs) const {
  if (uploadInFlight_ || connectivity_ != ConnectivityState::Connected) {
    return false;
  }
  if (pendingAttendanceRecords_.empty()) {
    return false;
  }
  if (!credentials_.configured() || !deviceApiClient_.configured()) {
    return false;
  }
  if (!snapshots_.attendanceConfig.has_value()) {
    return false;
  }
  if (lastUploadAttemptMs_ == 0) {
    return true;
  }
  return nowMs - lastUploadAttemptMs_ >= board::kUploadRetryIntervalMs;
}

void AppRuntime::performSync(uint32_t nowMs) {
  syncInFlight_ = true;
  lastSyncAttemptMs_ = nowMs;
  renderDirty_ = true;

  auto result = deviceApiClient_.sync(credentials_);

  syncInFlight_ = false;
  if (!result.success || !result.data.has_value()) {
    setLastError(result.error);
    if (result.error.has_value()) {
      appendApiFailureLog("/api/device/sync", nowMs, result.error.value(), std::nullopt);
    }
    renderDirty_ = true;
    return;
  }

  snapshots_ = core::applySyncSnapshot(snapshots_, result.data.value(), nowMs);
  persistSnapshots();
  setLastError(std::nullopt);
  renderDirty_ = true;
}

void AppRuntime::performUpload(uint32_t nowMs) {
  uploadInFlight_ = true;
  lastUploadAttemptMs_ = nowMs;
  renderDirty_ = true;

  const std::size_t batchSize =
      std::min<std::size_t>(pendingAttendanceRecords_.size(), board::kUploadBatchSize);
  std::vector<core::PendingAttendanceRecord> batch(
      pendingAttendanceRecords_.begin(), pendingAttendanceRecords_.begin() + batchSize);

  auto result = deviceApiClient_.uploadAttendance(credentials_, batch);

  uploadInFlight_ = false;
  if (!result.success || !result.data.has_value()) {
    setLastError(result.error);
    if (result.error.has_value()) {
      markUploadAttemptFailure(batch, nowMs, result.error->code);
      appendApiFailureLog("/api/device/attendance/upload", nowMs, result.error.value(), std::nullopt);
    }
    renderDirty_ = true;
    return;
  }

  auto uploadState =
      core::applyUploadResults(pendingAttendanceRecords_, failureLogs_, result.data->results, nowMs);
  pendingAttendanceRecords_ = std::move(uploadState.queue);
  failureLogs_ = std::move(uploadState.logs);
  core::pruneFailureLogs(failureLogs_, board::kFailureLogLimit);
  persistPendingAttendanceRecords();
  persistFailureLogs();

  std::optional<core::ApiError> latestRejectedError;
  for (const auto& item : result.data->results) {
    if (item.status == core::AttendanceUploadStatus::Rejected && item.error.has_value()) {
      latestRejectedError = item.error;
    }
  }
  setLastError(latestRejectedError);
  renderDirty_ = true;
}

void AppRuntime::markUploadAttemptFailure(
    const std::vector<core::PendingAttendanceRecord>& batch,
    uint64_t occurredAt,
    const std::string& errorCode) {
  for (const auto& submitted : batch) {
    auto it = std::find_if(
        pendingAttendanceRecords_.begin(),
        pendingAttendanceRecords_.end(),
        [&](const core::PendingAttendanceRecord& item) {
          return item.clientRecordId == submitted.clientRecordId;
        });
    if (it == pendingAttendanceRecords_.end()) {
      continue;
    }

    it->lastAttemptAt = occurredAt;
    it->lastResultCode = errorCode;
  }

  persistPendingAttendanceRecords();
}

void AppRuntime::persistSnapshots() {
  if (!filesystemReady_) {
    return;
  }
  localStore_.saveSnapshots(snapshots_);
}

void AppRuntime::persistPendingAttendanceRecords() {
  if (!filesystemReady_) {
    return;
  }
  localStore_.savePendingAttendanceRecords(pendingAttendanceRecords_);
}

void AppRuntime::persistFailureLogs() {
  if (!filesystemReady_) {
    return;
  }
  localStore_.saveFailureLogs(failureLogs_);
}

void AppRuntime::appendApiFailureLog(
    const char* api,
    uint64_t occurredAt,
    const core::ApiError& error,
    std::optional<std::string> relatedId) {
  core::appendFailureLog(
      failureLogs_,
      core::FailureLogEntry{
          .id = core::buildFailureLogId(occurredAt, failureLogs_.size()),
          .api = api,
          .occurredAt = occurredAt,
          .code = error.code,
          .message = error.message,
          .relatedId = std::move(relatedId),
      },
      board::kFailureLogLimit);

  persistFailureLogs();
}

RuntimeStatus AppRuntime::buildRuntimeStatus() const {
  RuntimeStatus status = {};
  status.firmwareTag = board::kFirmwareTag;
  status.credentials = credentials_;
  status.snapshots = snapshots_;
  status.pendingQueueSize = pendingAttendanceRecords_.size();
  status.failureLogCount = failureLogs_.size();
  status.connectivity = connectivity_;
  status.credentialsReady = credentialsReady_;
  status.filesystemReady = filesystemReady_;
  status.templateStoreReady = templateStoreReady_;
  status.wifiConfigured = board::wifiConfigured();
  status.syncInFlight = syncInFlight_;
  status.uploadInFlight = uploadInFlight_;
  status.faceModuleEnabled = faceModuleEnabled_;
  status.lastErrorCode = lastErrorCode_;
  return status;
}

void AppRuntime::updateView() {
  display_.render(ui::StatusScreenPresenter::build(buildRuntimeStatus()));
  renderDirty_ = false;
}

void AppRuntime::setLastError(const std::optional<core::ApiError>& error) {
  lastErrorCode_ = error.has_value() ? std::optional<std::string>(error->code) : std::nullopt;
}

}  // namespace app
