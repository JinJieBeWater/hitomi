#include "infra/face/esp_who_enrollment_service.hpp"

#include <Arduino.h>
#include <cstdio>
#include <memory>
#include <optional>
#include <utility>

#include "board/app_config.hpp"
#include "dl_image_define.hpp"
#include "esp_err.h"
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"

namespace infra {
namespace {

std::optional<std::vector<uint8_t>> readBinaryFile(const std::string& path) {
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    return std::nullopt;
  }

  if (std::fseek(file, 0, SEEK_END) != 0) {
    std::fclose(file);
    return std::nullopt;
  }

  const long size = std::ftell(file);
  if (size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
    std::fclose(file);
    return std::nullopt;
  }

  std::vector<uint8_t> bytes(static_cast<std::size_t>(size));
  if (!bytes.empty()) {
    const std::size_t readCount = std::fread(bytes.data(), 1, bytes.size(), file);
    if (readCount != bytes.size()) {
      std::fclose(file);
      return std::nullopt;
    }
  }

  std::fclose(file);
  return bytes;
}

void removeTempFile(const std::string& path) {
  if (!path.empty()) {
    std::remove(path.c_str());
  }
}

}  // namespace

struct EspWhoEnrollmentService::Impl {
  explicit Impl(std::string dbPath) : tempDbPath(std::move(dbPath)) {}

  std::string tempDbPath;
  std::unique_ptr<HumanFaceDetect> detector;
  std::unique_ptr<HumanFaceRecognizer> recognizer;
  face::EnrollmentRequest request = {};
  face::EnrollmentProgress progress = {};
  std::optional<face::EnrollmentResult> completedResult;
  uint32_t startedAtMs = 0;
  uint32_t lastSampleAtMs = 0;
  bool sessionActive = false;
  bool loggedFrameShape = false;
  std::string lastLoggedDetail;
  std::size_t lastLoggedSamples = 0;
  std::size_t lastLoggedFaceCount = 0;

  uint32_t elapsedMs(uint32_t nowMs) const {
    return nowMs >= startedAtMs ? nowMs - startedAtMs : 0;
  }

  bool ensureDetector() {
    if (detector == nullptr) {
      detector = std::make_unique<HumanFaceDetect>(
          static_cast<HumanFaceDetect::model_type_t>(CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL),
          false);
    }
    return detector != nullptr;
  }

  bool recreateRecognizer() {
    recognizer.reset();
    removeTempFile(tempDbPath);
    recognizer = std::make_unique<HumanFaceRecognizer>(
        tempDbPath,
        static_cast<HumanFaceFeat::model_type_t>(CONFIG_DEFAULT_HUMAN_FACE_FEAT_MODEL),
        false);
    return recognizer != nullptr;
  }

  void resetDebugState() {
    loggedFrameShape = false;
    lastLoggedDetail.clear();
    lastLoggedSamples = 0;
    lastLoggedFaceCount = 0;
  }

  void logProgressIfChanged(uint32_t nowMs) {
    if (
        progress.detail == lastLoggedDetail &&
        progress.capturedSamples == lastLoggedSamples &&
        progress.detectedFaceCount == lastLoggedFaceCount) {
      return;
    }

    const unsigned long elapsedMsValue =
        startedAtMs == 0 ? 0UL : static_cast<unsigned long>(elapsedMs(nowMs));
    Serial.printf(
        "[ENROLL] task=%s detail=%s samples=%u/%u faces=%u elapsed=%lums\n",
        request.taskId.c_str(),
        progress.detail.c_str(),
        static_cast<unsigned>(progress.capturedSamples),
        static_cast<unsigned>(progress.requiredSamples),
        static_cast<unsigned>(progress.detectedFaceCount),
        elapsedMsValue);
    lastLoggedDetail = progress.detail;
    lastLoggedSamples = progress.capturedSamples;
    lastLoggedFaceCount = progress.detectedFaceCount;
  }

  void finishFailure(const char* reason, uint32_t nowMs) {
    const unsigned long elapsedMsValue =
        startedAtMs == 0 ? 0UL : static_cast<unsigned long>(elapsedMs(nowMs));
    Serial.printf(
        "[ENROLL] failed task=%s employee=%s reason=%s samples=%u/%u faces=%u elapsed=%lums\n",
        request.taskId.c_str(),
        request.employeeId.c_str(),
        reason,
        static_cast<unsigned>(progress.capturedSamples),
        static_cast<unsigned>(progress.requiredSamples),
        static_cast<unsigned>(progress.detectedFaceCount),
        elapsedMsValue);
    completedResult = face::EnrollmentResult{
        .taskId = request.taskId,
        .employeeId = request.employeeId,
        .success = false,
        .finishedAt = nowMs,
        .templateBytes = {},
        .failureReason = std::optional<std::string>(reason),
    };
    progress.active = false;
    progress.detail = reason;
    sessionActive = false;
    removeTempFile(tempDbPath);
    recognizer.reset();
  }
};

EspWhoEnrollmentService::EspWhoEnrollmentService(std::string tempDbPath)
    : impl_(std::make_unique<Impl>(std::move(tempDbPath))) {}

EspWhoEnrollmentService::~EspWhoEnrollmentService() = default;

bool EspWhoEnrollmentService::available() const {
  return true;
}

bool EspWhoEnrollmentService::active() const {
  return impl_->sessionActive;
}

bool EspWhoEnrollmentService::start(const face::EnrollmentRequest& request) {
  if (request.taskId.empty() || request.employeeId.empty()) {
    return false;
  }

  cancel();
  removeTempFile(impl_->tempDbPath);

  impl_->request = request;
  impl_->progress = face::EnrollmentProgress{
      .active = true,
      .capturedSamples = 0,
      .requiredSamples = board::kEnrollmentRequiredSamples,
      .detectedFaceCount = 0,
      .detail = "请看向摄像头",
  };
  impl_->completedResult.reset();
  impl_->startedAtMs = millis();
  impl_->lastSampleAtMs = 0;
  impl_->sessionActive = true;
  impl_->resetDebugState();
  Serial.printf(
      "[ENROLL] started task=%s employee=%s requiredSamples=%u\n",
      request.taskId.c_str(),
      request.employeeId.c_str(),
      static_cast<unsigned>(impl_->progress.requiredSamples));
  return true;
}

void EspWhoEnrollmentService::cancel() {
  if (!impl_->sessionActive) {
    impl_->completedResult.reset();
    return;
  }

  impl_->completedResult = face::EnrollmentResult{
      .taskId = impl_->request.taskId,
      .employeeId = impl_->request.employeeId,
      .success = false,
      .finishedAt = millis(),
      .templateBytes = {},
      .failureReason = std::optional<std::string>("ENROLLMENT_CANCELLED"),
  };
  impl_->progress.active = false;
  impl_->progress.detail = "已取消";
  impl_->sessionActive = false;
  Serial.printf(
      "[ENROLL] cancelled task=%s employee=%s samples=%u/%u\n",
      impl_->request.taskId.c_str(),
      impl_->request.employeeId.c_str(),
      static_cast<unsigned>(impl_->progress.capturedSamples),
      static_cast<unsigned>(impl_->progress.requiredSamples));
  removeTempFile(impl_->tempDbPath);
  impl_->recognizer.reset();
}

face::EnrollmentProgress EspWhoEnrollmentService::progress() const {
  return impl_->progress;
}

void EspWhoEnrollmentService::processFrame(
    const face::CameraFrameInfo& frameInfo,
    const uint8_t* frameData,
    uint32_t nowMs) {
  if (!impl_->sessionActive || frameData == nullptr) {
    return;
  }
  if (frameInfo.pixelFormat != face::CameraPixelFormat::RGB565) {
    impl_->finishFailure("UNSUPPORTED_PIXEL_FORMAT", nowMs);
    return;
  }
  if (impl_->lastSampleAtMs != 0 && nowMs - impl_->lastSampleAtMs < board::kEnrollmentFrameIntervalMs) {
    return;
  }
  if (impl_->startedAtMs != 0 && nowMs < impl_->startedAtMs) {
    Serial.printf(
        "[ENROLL] correcting clock drift start=%lu now=%lu\n",
        static_cast<unsigned long>(impl_->startedAtMs),
        static_cast<unsigned long>(nowMs));
    impl_->startedAtMs = nowMs;
  }
  if (impl_->startedAtMs != 0 && impl_->elapsedMs(nowMs) > board::kEnrollmentSessionTimeoutMs) {
    impl_->finishFailure("ENROLLMENT_TIMEOUT", nowMs);
    return;
  }
  if (!impl_->ensureDetector()) {
    impl_->finishFailure("ENROLLMENT_MODEL_INIT_FAILED", nowMs);
    return;
  }

  impl_->lastSampleAtMs = nowMs;
  if (!impl_->loggedFrameShape) {
    impl_->loggedFrameShape = true;
    Serial.printf(
        "[ENROLL] frame=%ux%u bytes=%u format=%s\n",
        static_cast<unsigned>(frameInfo.width),
        static_cast<unsigned>(frameInfo.height),
        static_cast<unsigned>(frameInfo.bytes),
        face::cameraPixelFormatName(frameInfo.pixelFormat));
  }

  dl::image::img_t image = {
      .data = const_cast<uint8_t*>(frameData),
      .width = frameInfo.width,
      .height = frameInfo.height,
      .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565,
  };

  const auto& detections = impl_->detector->run(image);
  impl_->progress.detectedFaceCount = detections.size();

  if (detections.empty()) {
    impl_->progress.capturedSamples = 0;
    impl_->progress.detail = "未检测到人脸";
    impl_->logProgressIfChanged(nowMs);
    return;
  }
  if (detections.size() > 1) {
    impl_->progress.capturedSamples = 0;
    impl_->progress.detail = "检测到多张人脸";
    impl_->logProgressIfChanged(nowMs);
    return;
  }

  impl_->progress.capturedSamples += 1;
  impl_->progress.detail = "请保持不动";
  impl_->logProgressIfChanged(nowMs);

  if (impl_->progress.capturedSamples < impl_->progress.requiredSamples) {
    return;
  }

  Serial.printf("[ENROLL] extracting template task=%s\n", impl_->request.taskId.c_str());
  if (!impl_->recreateRecognizer() || impl_->recognizer->enroll(image, detections) != ESP_OK) {
    impl_->finishFailure("ENROLLMENT_FEATURE_EXTRACTION_FAILED", nowMs);
    return;
  }

  auto templateBytes = readBinaryFile(impl_->tempDbPath);
  if (!templateBytes.has_value() || templateBytes->empty()) {
    impl_->finishFailure("ENROLLMENT_TEMPLATE_READ_FAILED", nowMs);
    return;
  }

  impl_->completedResult = face::EnrollmentResult{
      .taskId = impl_->request.taskId,
      .employeeId = impl_->request.employeeId,
      .success = true,
      .finishedAt = nowMs,
      .templateBytes = std::move(templateBytes.value()),
      .failureReason = std::nullopt,
  };
  impl_->progress.active = false;
  impl_->progress.detail = "模板采集完成";
  impl_->sessionActive = false;
  Serial.printf(
      "[ENROLL] success task=%s employee=%s templateBytes=%u elapsed=%lums\n",
      impl_->request.taskId.c_str(),
      impl_->request.employeeId.c_str(),
      static_cast<unsigned>(impl_->completedResult->templateBytes.size()),
      static_cast<unsigned long>(impl_->elapsedMs(nowMs)));
  removeTempFile(impl_->tempDbPath);
  impl_->recognizer.reset();
}

std::optional<face::EnrollmentResult> EspWhoEnrollmentService::takeResult() {
  auto result = std::move(impl_->completedResult);
  impl_->completedResult.reset();
  return result;
}

}  // namespace infra
