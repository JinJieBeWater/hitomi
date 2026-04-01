#pragma once

namespace face {

class CameraPort {
 public:
  virtual ~CameraPort() = default;
  virtual bool available() const = 0;
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
  bool available() const override {
    return false;
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
