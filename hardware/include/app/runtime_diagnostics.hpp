#pragma once

#include <string>

#include "app/runtime_status.hpp"

namespace app {

struct RuntimeDiagnostics {
  std::string credentialsLine;
  std::string snapshotLine;
  std::string queueLine;
  std::string faceLine;
};

RuntimeDiagnostics buildRuntimeDiagnostics(const RuntimeStatus& status);

}  // namespace app
