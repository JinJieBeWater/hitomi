#pragma once

#include <cstdint>
#include <memory>

#include "face/ports.hpp"
#include "infra/display/touch_transform.hpp"

struct _lv_display_t;
using lv_display_t = _lv_display_t;

class SzpiLvglDisplay {
 public:
  static constexpr uint32_t kHorizontalResolution = 320;
  static constexpr uint32_t kVerticalResolution = 240;
  static constexpr uint32_t kBufferRows = 24;

  SzpiLvglDisplay();
  ~SzpiLvglDisplay();

  SzpiLvglDisplay(const SzpiLvglDisplay&) = delete;
  SzpiLvglDisplay& operator=(const SzpiLvglDisplay&) = delete;
  SzpiLvglDisplay(SzpiLvglDisplay&&) noexcept;
  SzpiLvglDisplay& operator=(SzpiLvglDisplay&&) noexcept;

  bool init();
  lv_display_t* display() const;
  bool drawPreviewBitmap(
      int x1,
      int y1,
      int x2,
      int y2,
      const uint16_t* pixels);
  bool touchReady() const;
  uint32_t touchTapCount() const;
  infra::TouchPoint lastTouchPoint() const;
  bool touchPressed() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
