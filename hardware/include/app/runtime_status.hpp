#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/models.hpp"

namespace app {

enum class ConnectivityState {
  Unknown,
  Disconnected,
  Connected,
};

struct RuntimeStatus {
  std::string firmwareTag;
  core::DeviceCredentials credentials;
  core::SnapshotBundle snapshots;
  std::size_t pendingQueueSize = 0;
  std::size_t failureLogCount = 0;
  ConnectivityState connectivity = ConnectivityState::Unknown;
  bool credentialsReady = false;
  bool filesystemReady = false;
  bool templateStoreReady = false;
  bool wifiConfigured = false;
  bool syncInFlight = false;
  bool uploadInFlight = false;
  bool faceModuleEnabled = false;
  std::optional<std::string> lastErrorCode;
};

}  // namespace app
