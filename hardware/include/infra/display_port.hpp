#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ui/app_view_model.hpp"

namespace infra {

enum class DisplayCommandType : uint8_t {
  RefreshData = 0,
  StartEnrollmentTask,
};

struct DisplayCommand {
  DisplayCommandType type = DisplayCommandType::RefreshData;
  std::string targetId;
};

class DisplayPort {
 public:
  virtual ~DisplayPort() = default;

  virtual bool init() = 0;
  virtual bool ready() const = 0;
  virtual void render(const ui::AppViewModel& viewModel) = 0;
  virtual void tick(uint32_t nowMs) = 0;
  virtual std::optional<DisplayCommand> consumeCommand() = 0;
};

}  // namespace infra
