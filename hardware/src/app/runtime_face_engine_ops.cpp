#include "app/runtime_face_engine_ops.hpp"

#include <Arduino.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <list>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "board/app_config.hpp"
#include "core/use_cases.hpp"
#include "dl_detect_define.hpp"
#include "dl_image_define.hpp"
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

struct FaceDetectionRuntime {
  std::unique_ptr<HumanFaceDetect> detector;
  std::unique_ptr<HumanFaceFeat> featureModel;
  std::vector<StoredFaceTemplate> templates;
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

std::optional<std::pair<const StoredFaceTemplate*, float>> recognizeEmployee(
    const RuntimeContext& context,
    RuntimeState& state,
    const dl::image::img_t& image,
    const std::list<dl::detect::result_t>& results) {
  reloadRecognitionTemplates(context, state);
  auto& runtime = faceDetectionRuntime();
  if (runtime.templates.empty() || !ensureFeatureModel()) {
    return std::nullopt;
  }

  const auto* primaryFace = primaryFaceResult(results);
  if (primaryFace == nullptr) {
    return std::nullopt;
  }

  dl::TensorBase* feature = runtime.featureModel->run(image, primaryFace->keypoint);
  if (feature == nullptr) {
    return std::nullopt;
  }

  const StoredFaceTemplate* bestTemplate = nullptr;
  float bestSimilarity = board::kFaceRecognitionThreshold;
  for (const auto& templateItem : runtime.templates) {
    const float similarity = compareFeatureSimilarity(feature, templateItem.feature);
    if (similarity > bestSimilarity) {
      bestSimilarity = similarity;
      bestTemplate = &templateItem;
    }
  }

  if (bestTemplate == nullptr) {
    return std::nullopt;
  }
  return std::make_pair(bestTemplate, bestSimilarity);
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
  const core::QueueMutationResult mutation = core::enqueueAttendanceRecord(state.pendingAttendanceRecords, record);
  const std::string attendanceEventKey =
      matched.employeeId + ":" + record.localDate + ":" + core::attendanceTypeToApiValue(record.type);

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
  const auto& results = runtime.detector->run(image);
  perf.detectRuns += 1;
  perf.detectUs += static_cast<uint64_t>(micros() - detectStartedUs);
  const bool detected = !results.empty();
  const std::size_t count = results.size();
  const std::optional<float> score = detected ? std::optional<float>(topScore(results)) : std::nullopt;
  storeFaceBoxes(state, results);
  applyFaceDetectionResult(state, detected, count, score);

  if (detected) {
    perf.detectHits += 1;
    if (recognitionComputationDue(state, nowMs)) {
      const uint32_t recognitionStartedUs = micros();
      state.lastRecognitionComputeMs = nowMs;
      if (const auto matched = recognizeEmployee(context, state, image, results); matched.has_value()) {
        perf.recognitionRuns += 1;
        perf.recognitionHits += 1;
        perf.recognitionUs += static_cast<uint64_t>(micros() - recognitionStartedUs);
        handleAttendanceRecognition(context, state, *matched->first, matched->second, frameInfo.capturedAtMs, nowMs);
      } else if (!faceDetectionRuntime().templates.empty()) {
        perf.recognitionRuns += 1;
        perf.recognitionUs += static_cast<uint64_t>(micros() - recognitionStartedUs);
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
