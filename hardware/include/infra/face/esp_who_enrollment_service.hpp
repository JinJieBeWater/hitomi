#pragma once

#include <memory>
#include <optional>
#include <string>

#include "face/ports.hpp"

namespace infra {

class EspWhoEnrollmentService final : public face::EnrollmentServicePort {
 public:
  explicit EspWhoEnrollmentService(std::string tempDbPath);
  ~EspWhoEnrollmentService() override;

  bool available() const override;
  bool active() const override;
  bool start(const face::EnrollmentRequest& request) override;
  void cancel() override;
  face::EnrollmentProgress progress() const override;
  void processFrame(const face::CameraFrameInfo& frameInfo, const uint8_t* frameData, uint32_t nowMs) override;
  std::optional<face::EnrollmentResult> takeResult() override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace infra
