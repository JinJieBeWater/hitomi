#pragma once

#include <cstdint>

#include "ui/app_view_model.hpp"

namespace infra {

class DisplayPort {
 public:
  virtual ~DisplayPort() = default;

  virtual bool init() = 0;
  virtual bool ready() const = 0;
  virtual void render(const ui::AppViewModel& viewModel) = 0;
  virtual void tick(uint32_t nowMs) = 0;
};

}  // namespace infra
