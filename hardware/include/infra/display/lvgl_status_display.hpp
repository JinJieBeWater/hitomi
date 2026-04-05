#pragma once

#include <cstdint>
#include <memory>

#include "infra/display_port.hpp"

namespace infra {

class LvglStatusDisplay final : public DisplayPort {
 public:
  LvglStatusDisplay();
  ~LvglStatusDisplay() override;

  LvglStatusDisplay(const LvglStatusDisplay&) = delete;
  LvglStatusDisplay& operator=(const LvglStatusDisplay&) = delete;
  LvglStatusDisplay(LvglStatusDisplay&&) noexcept;
  LvglStatusDisplay& operator=(LvglStatusDisplay&&) noexcept;

  bool init() override;
  bool ready() const override;
  void render(const ui::AppViewModel& viewModel) override;
  void tick(uint32_t nowMs) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace infra
