#pragma once

#include <memory>

#include "face/ports.hpp"

namespace infra {

class Esp32CameraPort final : public face::CameraPort {
 public:
  Esp32CameraPort();
  ~Esp32CameraPort() override;

  Esp32CameraPort(const Esp32CameraPort&) = delete;
  Esp32CameraPort& operator=(const Esp32CameraPort&) = delete;
  Esp32CameraPort(Esp32CameraPort&&) noexcept;
  Esp32CameraPort& operator=(Esp32CameraPort&&) noexcept;

  bool init() override;
  bool available() const override;
  bool ready() const override;
  face::CameraStatus status() const override;
  std::unique_ptr<face::CameraFrameLease> capture() override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace infra
