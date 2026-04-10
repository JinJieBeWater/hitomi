#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace face {

enum class CameraPixelFormat : uint8_t {
  Unknown = 0,
  RGB565,
  JPEG,
  Grayscale,
  YUV422,
};

inline const char* cameraPixelFormatName(CameraPixelFormat format) {
  switch (format) {
    case CameraPixelFormat::RGB565:
      return "RGB565";
    case CameraPixelFormat::JPEG:
      return "JPEG";
    case CameraPixelFormat::Grayscale:
      return "Grayscale";
    case CameraPixelFormat::YUV422:
      return "YUV422";
    case CameraPixelFormat::Unknown:
    default:
      return "Unknown";
  }
}

struct CameraFrameInfo {
  uint16_t width = 0;
  uint16_t height = 0;
  std::size_t bytes = 0;
  CameraPixelFormat pixelFormat = CameraPixelFormat::Unknown;
  uint64_t capturedAtMs = 0;
};

struct CameraStatus {
  bool supported = false;
  bool initialized = false;
  uint32_t captureCount = 0;
  uint32_t failedCaptureCount = 0;
  CameraFrameInfo lastFrame = {};
  std::string sensorModel;
  std::string lastError;
};

class CameraFrameLease {
 public:
  virtual ~CameraFrameLease() = default;
  virtual const CameraFrameInfo& info() const = 0;
  virtual const uint8_t* data() const = 0;
};

class CameraPort {
 public:
  virtual ~CameraPort() = default;
  virtual bool init() = 0;
  virtual bool available() const = 0;
  virtual bool ready() const = 0;
  virtual CameraStatus status() const = 0;
  virtual std::unique_ptr<CameraFrameLease> capture() = 0;
};

class EnrollmentServicePort {
 public:
  virtual ~EnrollmentServicePort() = default;
  virtual bool available() const = 0;
};

class RecognitionServicePort {
 public:
  virtual ~RecognitionServicePort() = default;
  virtual bool available() const = 0;
};

class NoopCameraPort final : public CameraPort {
 public:
  bool init() override {
    return false;
  }

  bool available() const override {
    return false;
  }

  bool ready() const override {
    return false;
  }

  CameraStatus status() const override {
    return CameraStatus{};
  }

  std::unique_ptr<CameraFrameLease> capture() override {
    return nullptr;
  }
};

class NoopEnrollmentServicePort final : public EnrollmentServicePort {
 public:
  bool available() const override {
    return false;
  }
};

class NoopRecognitionServicePort final : public RecognitionServicePort {
 public:
  bool available() const override {
    return false;
  }
};

}  // namespace face
