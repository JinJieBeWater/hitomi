#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "face/ports.hpp"
#include "ui/app_view_model.hpp"

namespace infra {

enum class DisplayCommandType : uint8_t {
  RefreshData = 0,
  StartEnrollmentTask,
  CancelEnrollment,
  DismissCapture,
};

struct DisplayCommand {
  DisplayCommandType type = DisplayCommandType::RefreshData;
  std::string targetId;
};

struct DisplayRgb565Frame {
  const uint8_t* data = nullptr;
  uint16_t width = 0;
  uint16_t height = 0;
  static constexpr std::size_t kMaxFaceBoxes = 4;
  std::array<face::FaceBox, kMaxFaceBoxes> faceBoxes = {};
  std::array<face::FaceBoxTone, kMaxFaceBoxes> faceBoxTones = {};
  std::size_t faceBoxCount = 0;
  std::optional<std::size_t> primaryFaceBoxIndex;
};

enum class DisplayNotificationLevel : uint8_t {
  Info = 0,
  Success,
  Warning,
  Error,
};

struct DisplayNotification {
  DisplayNotificationLevel level = DisplayNotificationLevel::Info;
  std::string text;
  uint32_t durationMs = 2200;
};

class DisplayPort {
 public:
  virtual ~DisplayPort() = default;

  virtual bool init() = 0;
  virtual bool ready() const = 0;
  virtual void render(const ui::AppViewModel& viewModel) = 0;
  virtual void updateCameraPreview(const DisplayRgb565Frame& frame) = 0;
  virtual void clearCameraPreview() = 0;
  virtual void showNotification(const DisplayNotification& notification) = 0;
  virtual void tick(uint32_t nowMs) = 0;
  virtual std::optional<DisplayCommand> consumeCommand() = 0;
};

}  // namespace infra
