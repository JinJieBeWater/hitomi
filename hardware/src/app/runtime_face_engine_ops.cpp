#include "app/runtime_face_engine_ops.hpp"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <list>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "board/app_config.hpp"
#include "core/use_cases.hpp"
#include "dl_detect_define.hpp"
#include "dl_image_define.hpp"
#include "face/model_lock.hpp"
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"
#include "app/runtime_storage_ops.hpp"
#include "infra/display_port.hpp"

namespace app {
namespace {

constexpr bool kFaceComponentsLinked =
    sizeof(HumanFaceDetect) > 0 && sizeof(HumanFaceRecognizer) > 0;

struct FacePerfWindow {
  uint32_t startedAtMs = 0;
  uint32_t detectRuns = 0;
  uint32_t detectHits = 0;
  uint32_t detectMisses = 0;
  uint32_t recognitionRuns = 0;
  uint32_t recognitionHits = 0;
  uint64_t detectUs = 0;
  uint64_t recognitionUs = 0;
};

struct StoredFaceTemplate {
  std::string employeeId;
  std::string employeeCode;
  std::string employeeName;
  std::vector<float> feature;
};

struct RecognitionSnapshotKey {
  uint64_t attendanceConfigSyncedAt = 0;
  uint64_t employeesSyncedAt = 0;
  uint64_t templateManifestUpdatedAt = 0;
  std::size_t templateCount = 0;
};

struct PendingRecognitionRequest {
  face::CameraFrameInfo frameInfo = {};
  RecognitionSnapshotKey snapshotKey = {};
  std::optional<uint64_t> capturedWallClockMs;
  uint32_t generation = 0;
  std::list<dl::detect::result_t> results;
  std::vector<StoredFaceTemplate> templates;
};

struct PendingRecognitionResult {
  StoredFaceTemplate matched;
  float similarity = 0.0F;
  uint64_t capturedAtMs = 0;
  uint64_t recognizedAt = 0;
  RecognitionSnapshotKey snapshotKey = {};
  uint32_t generation = 0;
};

struct FaceDetectionRuntime {
  std::unique_ptr<HumanFaceDetect> detector;
  std::unique_ptr<HumanFaceFeat> featureModel;
  std::vector<StoredFaceTemplate> templates;
  SemaphoreHandle_t recognitionMutex = nullptr;
  TaskHandle_t recognitionTask = nullptr;
  std::optional<PendingRecognitionRequest> recognitionRequest;
  std::optional<PendingRecognitionResult> recognitionResult;
  uint8_t* recognitionFrameBuffer = nullptr;
  std::size_t recognitionFrameBufferSize = 0;
  bool recognitionBusy = false;
  uint32_t recognitionGeneration = 1;
  uint64_t templateManifestUpdatedAt = 0;
  uint64_t employeesSyncedAt = 0;
  std::size_t templateCount = 0;
};

FaceDetectionRuntime& faceDetectionRuntime() {
  static FaceDetectionRuntime runtime;
  return runtime;
}

FacePerfWindow& facePerfWindow() {
  static FacePerfWindow window;
  return window;
}

RecognitionSnapshotKey recognitionSnapshotKey(const RuntimeState& state) {
  return RecognitionSnapshotKey{
      .attendanceConfigSyncedAt = state.snapshots.attendanceConfigSyncedAt,
      .employeesSyncedAt = state.snapshots.employeesSyncedAt,
      .templateManifestUpdatedAt = state.storageAux.templateLibrarySummary.manifestUpdatedAt,
      .templateCount = state.storageAux.templateLibrarySummary.templateCount,
  };
}

bool sameSnapshotKey(const RecognitionSnapshotKey& left, const RecognitionSnapshotKey& right) {
  return left.attendanceConfigSyncedAt == right.attendanceConfigSyncedAt &&
      left.employeesSyncedAt == right.employeesSyncedAt &&
      left.templateManifestUpdatedAt == right.templateManifestUpdatedAt &&
      left.templateCount == right.templateCount;
}

bool employeeExists(const RuntimeState& state, const std::string& employeeId) {
  return std::any_of(
      state.snapshots.employees.begin(),
      state.snapshots.employees.end(),
      [&](const core::EmployeeSnapshot& employee) { return employee.id == employeeId; });
}

void maybeLogFacePerf(uint32_t nowMs) {
  auto& window = facePerfWindow();
  if (window.startedAtMs == 0) {
    window.startedAtMs = nowMs;
    return;
  }
  if (nowMs - window.startedAtMs < 1000) {
    return;
  }

  Serial.printf(
      "[PERF][FACE] detect=%u hit=%u miss=%u detect_avg_us=%llu recognition=%u hit=%u recognition_avg_us=%llu\n",
      static_cast<unsigned>(window.detectRuns),
      static_cast<unsigned>(window.detectHits),
      static_cast<unsigned>(window.detectMisses),
      static_cast<unsigned long long>(
          window.detectRuns == 0 ? 0ULL : window.detectUs / window.detectRuns),
      static_cast<unsigned>(window.recognitionRuns),
      static_cast<unsigned>(window.recognitionHits),
      static_cast<unsigned long long>(
          window.recognitionRuns == 0 ? 0ULL : window.recognitionUs / window.recognitionRuns));

  window = FacePerfWindow{.startedAtMs = nowMs};
}

void markFaceDetectUnavailable(RuntimeState& state, const char* detail) {
  state.faceDetectReady = false;
  state.faceDetectStatusDetail = detail;
  state.faceDetected = false;
  state.detectedFaceCount = 0;
  state.faceTopScore.reset();
  state.faceBoxCount = 0;
  state.primaryFaceBoxIndex.reset();
  state.renderDirty = true;
}

void clearFaceBoxes(RuntimeState& state) {
  state.faceBoxCount = 0;
  state.primaryFaceBoxIndex.reset();
}

void storeFaceBoxes(RuntimeState& state, const std::list<dl::detect::result_t>& results) {
  clearFaceBoxes(state);

  std::size_t index = 0;
  int bestArea = -1;
  std::optional<std::size_t> bestIndex;
  for (const auto& result : results) {
    if (index >= state.faceBoxes.size() || result.box.size() < 4) {
      break;
    }

    state.faceBoxes[index] = face::FaceBox{
        .left = static_cast<uint16_t>(std::max(result.box[0], 0)),
        .top = static_cast<uint16_t>(std::max(result.box[1], 0)),
        .right = static_cast<uint16_t>(std::max(result.box[2], 0)),
        .bottom = static_cast<uint16_t>(std::max(result.box[3], 0)),
    };
    const int area = result.box_area();
    if (area > bestArea) {
      bestArea = area;
      bestIndex = index;
    }
    index += 1;
  }

  state.faceBoxCount = index;
  state.primaryFaceBoxIndex = bestIndex;
}

void applyFaceDetectionResult(
    RuntimeState& state,
    bool detected,
    std::size_t count,
    std::optional<float> score) {
  const bool changed =
      detected != state.faceDetected ||
      count != state.detectedFaceCount ||
      score != state.faceTopScore;

  state.faceDetected = detected;
  state.detectedFaceCount = count;
  state.faceTopScore = score;
  state.faceDetectStatusDetail = detected ? std::optional<std::string>("Detected")
                                          : std::optional<std::string>("No face");
  if (changed) {
    state.renderDirty = true;
  }
}

bool ensureFaceDetector(RuntimeState& state) {
  auto& runtime = faceDetectionRuntime();
  if (runtime.detector != nullptr) {
    if (!state.faceDetectReady) {
      state.faceDetectReady = true;
      state.faceDetectStatusDetail = "Ready";
      state.renderDirty = true;
    }
    return true;
  }

  runtime.detector = std::make_unique<HumanFaceDetect>(
      static_cast<HumanFaceDetect::model_type_t>(CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL),
      false);
  state.faceDetectReady = runtime.detector != nullptr;
  state.faceDetectStatusDetail = state.faceDetectReady
      ? std::optional<std::string>("Ready")
      : std::optional<std::string>("Detector init failed");
  state.renderDirty = true;
  return state.faceDetectReady;
}

bool ensureFeatureModel() {
  auto& runtime = faceDetectionRuntime();
  if (runtime.featureModel == nullptr) {
    runtime.featureModel = std::make_unique<HumanFaceFeat>(
        static_cast<HumanFaceFeat::model_type_t>(CONFIG_DEFAULT_HUMAN_FACE_FEAT_MODEL),
        false);
  }
  return runtime.featureModel != nullptr;
}

float topScore(const std::list<dl::detect::result_t>& results) {
  float top = 0.0f;
  for (const auto& result : results) {
    if (result.score > top) {
      top = result.score;
    }
  }
  return top;
}

std::optional<std::vector<float>> parseTemplateFeature(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < (sizeof(uint16_t) * 3U)) {
    return std::nullopt;
  }

  auto readU16 = [&](std::size_t offset) -> uint16_t {
    uint16_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(uint16_t));
    return value;
  };

  const uint16_t numFeatsTotal = readU16(0);
  const uint16_t featLen = readU16(sizeof(uint16_t) * 2U);
  std::size_t offset = sizeof(uint16_t) * 3U;

  for (uint16_t index = 0; index < numFeatsTotal; index += 1) {
    if (offset + sizeof(uint16_t) > bytes.size()) {
      return std::nullopt;
    }

    const uint16_t featId = readU16(offset);
    offset += sizeof(uint16_t);
    const std::size_t featureBytes = static_cast<std::size_t>(featLen) * sizeof(float);
    if (offset + featureBytes > bytes.size()) {
      return std::nullopt;
    }
    if (featId == 0) {
      offset += featureBytes;
      continue;
    }

    std::vector<float> feature(featLen, 0.0F);
    std::memcpy(feature.data(), bytes.data() + offset, featureBytes);
    return feature;
  }

  return std::nullopt;
}

void reloadRecognitionTemplates(const RuntimeContext& context, RuntimeState& state) {
  auto& runtime = faceDetectionRuntime();
  const uint64_t manifestUpdatedAt = state.storageAux.templateLibrarySummary.manifestUpdatedAt;
  const uint64_t employeesSyncedAt = state.snapshots.employeesSyncedAt;
  const std::size_t templateCount = state.storageAux.templateLibrarySummary.templateCount;
  const bool unchanged =
      runtime.templateManifestUpdatedAt == manifestUpdatedAt &&
      runtime.employeesSyncedAt == employeesSyncedAt &&
      runtime.templateCount == templateCount;
  if (unchanged) {
    return;
  }

  runtime.templates.clear();
  runtime.templateManifestUpdatedAt = manifestUpdatedAt;
  runtime.employeesSyncedAt = employeesSyncedAt;
  runtime.templateCount = templateCount;
  if (!state.templateStoreReady || templateCount == 0 || state.snapshots.employees.empty()) {
    return;
  }

  std::size_t loadedCount = 0;
  for (const auto& employee : state.snapshots.employees) {
    const auto blob = context.templateStore.readTemplate(employee.id);
    if (!blob.has_value()) {
      continue;
    }

    const auto feature = parseTemplateFeature(blob->bytes);
    if (!feature.has_value() || feature->empty()) {
      Serial.printf("[FACE] skip invalid template employee=%s\n", employee.id.c_str());
      continue;
    }

    runtime.templates.push_back(StoredFaceTemplate{
        .employeeId = employee.id,
        .employeeCode = employee.code,
        .employeeName = employee.name,
        .feature = feature.value(),
    });
    loadedCount += 1;
  }

  Serial.printf(
      "[FACE] recognition templates loaded=%u manifestUpdatedAt=%llu\n",
      static_cast<unsigned>(loadedCount),
      static_cast<unsigned long long>(manifestUpdatedAt));
}

float compareFeatureSimilarity(const dl::TensorBase* feature, const std::vector<float>& stored) {
  if (feature == nullptr || feature->dtype != dl::DATA_TYPE_FLOAT || feature->size != static_cast<int>(stored.size())) {
    return 0.0F;
  }

  const auto* current = static_cast<const float*>(feature->data);
  float similarity = 0.0F;
  for (std::size_t index = 0; index < stored.size(); index += 1) {
    similarity += current[index] * stored[index];
  }
  return similarity;
}

const dl::detect::result_t* primaryFaceResult(const std::list<dl::detect::result_t>& results) {
  if (results.empty()) {
    return nullptr;
  }

  return &(*std::max_element(
      results.begin(),
      results.end(),
      [](const dl::detect::result_t& left, const dl::detect::result_t& right) {
        return left.box_area() < right.box_area();
      }));
}

std::optional<std::pair<StoredFaceTemplate, float>> recognizeEmployeeFromLoadedTemplates(
    const dl::image::img_t& image,
    const std::list<dl::detect::result_t>& results,
    const std::vector<StoredFaceTemplate>& templates) {
  if (templates.empty() || !ensureFeatureModel()) {
    return std::nullopt;
  }

  const auto* primaryFace = primaryFaceResult(results);
  if (primaryFace == nullptr) {
    return std::nullopt;
  }

  auto& runtime = faceDetectionRuntime();
  if (!face::tryLockModel(20)) {
    return std::nullopt;
  }
  dl::TensorBase* feature = runtime.featureModel->run(image, primaryFace->keypoint);
  if (feature == nullptr) {
    face::unlockModel();
    return std::nullopt;
  }

  const StoredFaceTemplate* bestTemplate = nullptr;
  float bestSimilarity = board::kFaceRecognitionThreshold;
  for (const auto& templateItem : templates) {
    const float similarity = compareFeatureSimilarity(feature, templateItem.feature);
    if (similarity > bestSimilarity) {
      bestSimilarity = similarity;
      bestTemplate = &templateItem;
    }
  }
  face::unlockModel();

  if (bestTemplate == nullptr) {
    return std::nullopt;
  }
  return std::make_pair(*bestTemplate, bestSimilarity);
}

void handleAttendanceRecognition(
    const RuntimeContext& context,
    RuntimeState& state,
    const StoredFaceTemplate& matched,
    float similarity,
    uint64_t recognizedAt,
    uint32_t nowMs);

void recognitionTaskMain(void*) {
  auto& runtime = faceDetectionRuntime();
  while (true) {
    std::optional<PendingRecognitionRequest> request;
    if (runtime.recognitionMutex != nullptr && xSemaphoreTake(runtime.recognitionMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      if (runtime.recognitionRequest.has_value()) {
        request = std::move(runtime.recognitionRequest);
        runtime.recognitionRequest.reset();
      }
      xSemaphoreGive(runtime.recognitionMutex);
    }

    if (!request.has_value()) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    const uint32_t startedUs = micros();
    dl::image::img_t image = {
        .data = runtime.recognitionFrameBuffer,
        .width = request->frameInfo.width,
        .height = request->frameInfo.height,
        .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
    };
    const auto match = recognizeEmployeeFromLoadedTemplates(image, request->results, request->templates);

    if (runtime.recognitionMutex != nullptr && xSemaphoreTake(runtime.recognitionMutex, portMAX_DELAY) == pdTRUE) {
      if (match.has_value()) {
        runtime.recognitionResult = PendingRecognitionResult{
            .matched = match->first,
            .similarity = match->second,
            .capturedAtMs = request->frameInfo.capturedAtMs,
            .recognizedAt = request->capturedWallClockMs.value_or(0),
            .snapshotKey = request->snapshotKey,
            .generation = request->generation,
        };
      }
      runtime.recognitionBusy = false;
      xSemaphoreGive(runtime.recognitionMutex);
    }

    Serial.printf(
        "[FACE] recognition async matched=%s elapsed_us=%lu\n",
        match.has_value() ? "yes" : "no",
        static_cast<unsigned long>(micros() - startedUs));
  }
}

bool ensureRecognitionTask() {
  auto& runtime = faceDetectionRuntime();
  if (runtime.recognitionTask != nullptr) {
    return true;
  }
  if (runtime.recognitionMutex == nullptr) {
    runtime.recognitionMutex = xSemaphoreCreateMutex();
  }
  if (runtime.recognitionMutex == nullptr) {
    return false;
  }
  return xTaskCreatePinnedToCore(
             recognitionTaskMain,
             "face_recognition",
             8 * 1024,
             nullptr,
             1,
             &runtime.recognitionTask,
             0) == pdPASS;
}

bool ensureRecognitionFrameBuffer(FaceDetectionRuntime& runtime, std::size_t bytes) {
  constexpr std::size_t kMaxRecognitionFrameBytes = 320U * 240U * 2U;
  if (bytes == 0 || bytes > kMaxRecognitionFrameBytes) {
    return false;
  }
  if (runtime.recognitionFrameBuffer != nullptr && runtime.recognitionFrameBufferSize >= bytes) {
    return true;
  }
  if (runtime.recognitionFrameBuffer != nullptr) {
    heap_caps_free(runtime.recognitionFrameBuffer);
    runtime.recognitionFrameBuffer = nullptr;
    runtime.recognitionFrameBufferSize = 0;
  }

  runtime.recognitionFrameBuffer = static_cast<uint8_t*>(heap_caps_malloc(bytes, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM));
  if (runtime.recognitionFrameBuffer == nullptr) {
    return false;
  }
  runtime.recognitionFrameBufferSize = bytes;
  return true;
}

bool enqueueRecognitionRequest(
    const RuntimeContext& context,
    RuntimeState& state,
    const face::CameraFrameInfo& frameInfo,
    const uint8_t* frameData,
    const std::list<dl::detect::result_t>& results) {
  if (frameData == nullptr || frameInfo.bytes == 0 || results.empty()) {
    return false;
  }
  auto& runtime = faceDetectionRuntime();

  reloadRecognitionTemplates(context, state);
  if (runtime.templates.empty() || !ensureRecognitionTask()) {
    return false;
  }

  if (xSemaphoreTake(runtime.recognitionMutex, pdMS_TO_TICKS(2)) != pdTRUE) {
    return false;
  }
  if (runtime.recognitionBusy || runtime.recognitionRequest.has_value()) {
    xSemaphoreGive(runtime.recognitionMutex);
    return false;
  }
  if (!ensureRecognitionFrameBuffer(runtime, frameInfo.bytes)) {
    xSemaphoreGive(runtime.recognitionMutex);
    return false;
  }
  std::memcpy(runtime.recognitionFrameBuffer, frameData, frameInfo.bytes);

  PendingRecognitionRequest request = {
      .frameInfo = frameInfo,
      .snapshotKey = recognitionSnapshotKey(state),
      .capturedWallClockMs = resolveWallClockTimeMs(state, static_cast<uint32_t>(frameInfo.capturedAtMs)),
      .generation = runtime.recognitionGeneration,
      .results = results,
      .templates = runtime.templates,
  };
  if (!request.capturedWallClockMs.has_value()) {
    xSemaphoreGive(runtime.recognitionMutex);
    return false;
  }
  runtime.recognitionRequest = std::move(request);
  runtime.recognitionBusy = true;
  xSemaphoreGive(runtime.recognitionMutex);
  return true;
}

void consumeRecognitionResult(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  auto& runtime = faceDetectionRuntime();
  if (runtime.recognitionMutex == nullptr) {
    return;
  }
  std::optional<PendingRecognitionResult> result;
  if (xSemaphoreTake(runtime.recognitionMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
    result = std::move(runtime.recognitionResult);
    runtime.recognitionResult.reset();
    xSemaphoreGive(runtime.recognitionMutex);
  }
  if (!result.has_value()) {
    return;
  }
  if (context.enrollmentService.active() || result->recognizedAt == 0 ||
      result->generation != runtime.recognitionGeneration ||
      !sameSnapshotKey(result->snapshotKey, recognitionSnapshotKey(state)) ||
      !employeeExists(state, result->matched.employeeId)) {
    return;
  }

  handleAttendanceRecognition(
      context,
      state,
      result->matched,
      result->similarity,
      result->recognizedAt,
      nowMs);
}

std::string attendanceTypeLabel(core::AttendanceRecordType type) {
  return type == core::AttendanceRecordType::ClockIn ? "上班" : "下班";
}

std::string employeeDisplayLabel(const StoredFaceTemplate& employee) {
  if (!employee.employeeCode.empty()) {
    return "员工 " + employee.employeeCode;
  }
  if (!employee.employeeId.empty()) {
    return "员工 " + employee.employeeId;
  }
  return "员工";
}

bool recognitionComputationDue(const RuntimeState& state, uint32_t nowMs) {
  return state.lastRecognitionComputeMs == 0 ||
      nowMs - state.lastRecognitionComputeMs >= board::kFaceRecognitionCooldownMs;
}

void notify(
    const RuntimeContext& context,
    infra::DisplayNotificationLevel level,
    const std::string& text,
    uint32_t durationMs) {
  context.display.showNotification(infra::DisplayNotification{
      .level = level,
      .text = text,
      .durationMs = durationMs,
  });
}

void publishAttendanceFeedback(
    const RuntimeContext& context,
    RuntimeState& state,
    const std::string& eventKey,
    infra::DisplayNotificationLevel level,
    const std::string& feedback,
    uint32_t nowMs) {
  state.lastAttendanceFeedback = feedback;
  state.lastAttendanceFeedbackAtMs = nowMs;
  state.renderDirty = true;

  const bool shouldNotify =
      !state.lastRecognitionEventKey.has_value() || state.lastRecognitionEventKey.value() != eventKey ||
      nowMs - state.lastRecognitionHandledMs >= board::kFaceRecognitionCooldownMs;
  state.lastRecognitionEventKey = eventKey;
  state.lastRecognitionHandledMs = nowMs;
  if (shouldNotify) {
    notify(context, level, feedback, level == infra::DisplayNotificationLevel::Success ? 2200 : 2600);
  }
}

void handleAttendanceRecognition(
    const RuntimeContext& context,
    RuntimeState& state,
    const StoredFaceTemplate& matched,
    float similarity,
    uint64_t recognizedAt,
    uint32_t nowMs) {
  const std::string employeeName = employeeDisplayLabel(matched);
  if (!state.snapshots.attendanceConfig.has_value()) {
    publishAttendanceFeedback(
        context,
        state,
        matched.employeeId + ":attendance-config-missing",
        infra::DisplayNotificationLevel::Error,
        employeeName + " 缺少考勤配置",
        nowMs);
    state.lastErrorCode = "ATTENDANCE_CONFIG_MISSING";
    return;
  }

  const auto attendanceType = core::classifyAttendanceType(state.snapshots.attendanceConfig.value(), recognizedAt);
  if (!attendanceType.has_value()) {
    publishAttendanceFeedback(
        context,
        state,
        matched.employeeId + ":not-in-window:" + std::to_string(recognizedAt / 60000ULL),
        infra::DisplayNotificationLevel::Warning,
        employeeName + " 当前不在考勤时段",
        nowMs);
    state.lastErrorCode = "ATTENDANCE_NOT_IN_WINDOW";
    return;
  }

  const core::PendingAttendanceRecord record = {
      .clientRecordId = "att-" + matched.employeeId + "-" + std::to_string(recognizedAt),
      .employeeId = matched.employeeId,
      .recognizedAt = recognizedAt,
      .type = attendanceType.value(),
      .localDate = core::buildShanghaiLocalDate(recognizedAt),
      .createdAt = recognizedAt,
      .lastAttemptAt = std::nullopt,
      .lastResultCode = std::nullopt,
  };
  const std::string attendanceEventKey =
      matched.employeeId + ":" + record.localDate + ":" + core::attendanceTypeToApiValue(record.type);
  const std::size_t marksBeforePrune = state.localAttendanceMarks.size();
  core::pruneLocalAttendanceMarks(state.localAttendanceMarks, record.localDate);
  if (state.localAttendanceMarks.size() != marksBeforePrune) {
    persistLocalAttendanceMarks(context, state);
  }

  if (core::hasLocalAttendanceMark(
          state.localAttendanceMarks, record.employeeId, record.localDate, record.type)) {
    std::ostringstream oss;
    oss << employeeName << " 今日" << attendanceTypeLabel(record.type) << "已打卡 @" << std::fixed
        << std::setprecision(2) << similarity;
    publishAttendanceFeedback(
        context,
        state,
        attendanceEventKey,
        infra::DisplayNotificationLevel::Warning,
        oss.str(),
        nowMs);
    state.lastErrorCode = "ATTENDANCE_DUPLICATE_LATER_OR_EQUAL";
    return;
  }

  core::upsertLocalAttendanceMark(
      state.localAttendanceMarks,
      core::LocalAttendanceMark{
          .employeeId = record.employeeId,
          .localDate = record.localDate,
          .type = record.type,
          .recognizedAt = record.recognizedAt,
          .uploaded = false,
      });
  persistLocalAttendanceMarks(context, state);

  const core::QueueMutationResult mutation = core::enqueueAttendanceRecord(state.pendingAttendanceRecords, record);

  std::string feedback;
  infra::DisplayNotificationLevel level = infra::DisplayNotificationLevel::Success;
  switch (mutation.action) {
    case core::QueueMutationAction::Inserted:
      feedback = employeeName + " " + attendanceTypeLabel(attendanceType.value()) + "已记录";
      persistPendingAttendanceRecords(context, state);
      state.lastErrorCode.reset();
      break;
    case core::QueueMutationAction::ReplacedWithEarlier:
      feedback = employeeName + " 已更新为更早的" + attendanceTypeLabel(attendanceType.value());
      persistPendingAttendanceRecords(context, state);
      state.lastErrorCode.reset();
      break;
    case core::QueueMutationAction::IgnoredLaterOrEqual:
    default:
      feedback = employeeName + " 已存在更早或相同的" + attendanceTypeLabel(attendanceType.value());
      level = infra::DisplayNotificationLevel::Warning;
      state.lastErrorCode = "ATTENDANCE_DUPLICATE_LATER_OR_EQUAL";
      break;
  }

  std::ostringstream oss;
  oss << feedback << " @" << std::fixed << std::setprecision(2) << similarity;

  publishAttendanceFeedback(
      context,
      state,
      attendanceEventKey,
      level,
      oss.str(),
      nowMs);
}

}  // namespace

void initializeFaceEngine(RuntimeState& state) {
#if defined(CONFIG_HUMAN_FACE_DETECT_MODEL_LOCATION) && defined(CONFIG_HUMAN_FACE_FEAT_MODEL_LOCATION)
  state.faceEngineReady = kFaceComponentsLinked;
  state.faceEngineStatusDetail = kFaceComponentsLinked
      ? std::optional<std::string>("Linked (model load deferred)")
      : std::optional<std::string>("Link check failed");
#else
  state.faceEngineReady = false;
  state.faceEngineStatusDetail = "model config missing";
#endif

  state.renderDirty = true;
  state.faceDetectStatusDetail = "Waiting for first frame";

  Serial.printf("[FACE] %s\n", state.faceEngineStatusDetail.value_or("unavailable").c_str());
}

void prepareFaceRecognition(const RuntimeContext& context, RuntimeState& state) {
  if (!state.faceEngineReady) {
    return;
  }

  const uint32_t startedUs = micros();
  const bool featureReady = ensureFeatureModel();
  reloadRecognitionTemplates(context, state);
  const auto& runtime = faceDetectionRuntime();
  Serial.printf(
      "[FACE] recognition prepared feature=%s templates=%u elapsed_us=%lu\n",
      featureReady ? "ready" : "unavailable",
      static_cast<unsigned>(runtime.templates.size()),
      static_cast<unsigned long>(micros() - startedUs));
}

void discardPendingRecognition() {
  auto& runtime = faceDetectionRuntime();
  if (runtime.recognitionMutex == nullptr) {
    runtime.recognitionGeneration += 1;
    return;
  }
  if (xSemaphoreTake(runtime.recognitionMutex, pdMS_TO_TICKS(20)) != pdTRUE) {
    return;
  }
  runtime.recognitionGeneration += 1;
  const bool hadQueuedRequest = runtime.recognitionRequest.has_value();
  runtime.recognitionRequest.reset();
  runtime.recognitionResult.reset();
  if (hadQueuedRequest) {
    runtime.recognitionBusy = false;
  }
  xSemaphoreGive(runtime.recognitionMutex);
}

void runFaceDetection(
    const RuntimeContext& context,
    RuntimeState& state,
    const face::CameraFrameInfo& frameInfo,
    const uint8_t* frameData,
    uint32_t nowMs) {
  maybeLogFacePerf(nowMs);
  if (!state.faceEngineReady || frameData == nullptr) {
    return;
  }
  if (frameInfo.pixelFormat != face::CameraPixelFormat::RGB565) {
    markFaceDetectUnavailable(state, "Unsupported pixel format");
    return;
  }
  if (state.lastFaceDetectionMs != 0 &&
      nowMs - state.lastFaceDetectionMs < board::kFaceDetectIntervalMs) {
    consumeRecognitionResult(context, state, nowMs);
    return;
  }
  state.lastFaceDetectionMs = nowMs;
  if (!ensureFaceDetector(state)) {
    return;
  }
  auto& perf = facePerfWindow();
  auto& runtime = faceDetectionRuntime();
  dl::image::img_t image = {
      .data = const_cast<uint8_t*>(frameData),
      .width = frameInfo.width,
      .height = frameInfo.height,
      .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
  };
  const uint32_t detectStartedUs = micros();
  if (!face::tryLockModel(20)) {
    return;
  }
  const std::list<dl::detect::result_t> results = runtime.detector->run(image);
  face::unlockModel();
  perf.detectRuns += 1;
  perf.detectUs += static_cast<uint64_t>(micros() - detectStartedUs);
  const bool detected = !results.empty();
  const std::size_t count = results.size();
  const std::optional<float> score = detected ? std::optional<float>(topScore(results)) : std::nullopt;
  storeFaceBoxes(state, results);
  applyFaceDetectionResult(state, detected, count, score);
  consumeRecognitionResult(context, state, nowMs);

  if (detected) {
    perf.detectHits += 1;
    if (recognitionComputationDue(state, nowMs)) {
      if (enqueueRecognitionRequest(context, state, frameInfo, frameData, results)) {
        state.lastRecognitionComputeMs = nowMs;
        perf.recognitionRuns += 1;
      }
    }
    Serial.printf(
        "[FACE] detected count=%u top=%.3f\n",
        static_cast<unsigned>(count),
        static_cast<double>(score.value_or(0.0f)));
  } else {
    perf.detectMisses += 1;
    clearFaceBoxes(state);
    Serial.println("[FACE] detected count=0");
  }
}

}  // namespace app
