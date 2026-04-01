#pragma once

#include "app/runtime_status.hpp"
#include "ui/app_view_model.hpp"

namespace ui {

class StatusScreenPresenter {
 public:
  static AppViewModel build(const app::RuntimeStatus& status);
};

}  // namespace ui
