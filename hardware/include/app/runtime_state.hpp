#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "app/runtime_context.hpp"
#include "app/runtime_status.hpp"
#include "infra/local_store.hpp"

namespace app {

struct RuntimeState {
  core::DeviceConfig deviceConfig;
  core::DeviceCredentials credentials;
  core::SnapshotBundle snapshots;
  std::vector<core::PendingAttendanceRecord> pendingAttendanceRecords;
  std::vector<core::FailureLogEntry> failureLogs;
  infra::StorageAuxState storageAux;
  ConnectivityState connectivity = ConnectivityState::Unknown;
  bool credentialsReady = false;
  bool filesystemReady = false;
  bool templateStoreReady = false;
  bool displayReady = false;
  bool faceModuleEnabled = false;
  bool apiProbeInFlight = false;
  bool apiProbeSucceeded = false;
  bool syncInFlight = false;
  bool uploadInFlight = false;
  bool renderDirty = true;
  bool lastButtonPressed = false;
  bool activationInFlight = false;
  bool wifiScanInProgress = false;
  uint32_t lastButtonPollMs = 0;
  uint32_t lastApiProbeAttemptMs = 0;
  uint32_t lastNetworkProbeMs = 0;
  uint32_t lastTemplateStoreProbeMs = 0;
  uint32_t lastWifiConnectAttemptMs = 0;
  uint32_t lastSyncAttemptMs = 0;
  uint32_t lastUploadAttemptMs = 0;
  uint32_t lastActivationAttemptMs = 0;
  std::optional<std::string> apiProbeStatusCode;
  std::optional<std::string> lastErrorCode;
  std::optional<std::string> activeWifiSsid;
  std::string serialCommandBuffer;
};

bool facePortsReady(const RuntimeContext& context);
RuntimeStatus buildRuntimeStatus(const RuntimeContext& context, const RuntimeState& state);
void setLastError(RuntimeState& state, const std::optional<core::ApiError>& error);

}  // namespace app
