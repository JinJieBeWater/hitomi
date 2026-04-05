#include "app/runtime_view_ops.hpp"

#include "infra/display_port.hpp"
#include "ui/status_screen_presenter.hpp"

namespace app {

void updateView(const RuntimeContext& context, RuntimeState& state) {
  context.display.render(ui::StatusScreenPresenter::build(buildRuntimeStatus(context, state)));
  state.renderDirty = false;
}

}  // namespace app
