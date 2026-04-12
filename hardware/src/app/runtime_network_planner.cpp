#include "app/runtime_network_planner.hpp"

#include "app/api_probe_policy.hpp"
#include "board/app_config.hpp"

namespace app {
namespace {

bool apiConfigured(const RuntimeState& state) {
  return state.deviceConfig.backendLocator.configured();
}

bool activationEligible(const RuntimeState& state) {
  return state.connectivity == ConnectivityState::Connected && !state.credentials.configured() &&
      apiConfigured(state) && state.deviceConfig.bootstrapIdentity.configured();
}

bool syncEligible(const RuntimeState& state) {
  return state.connectivity == ConnectivityState::Connected && state.credentials.configured() && apiConfigured(state);
}

bool enrollmentReportEligible(const RuntimeState& state) {
  return syncEligible(state) && !state.pendingEnrollmentReports.empty();
}

bool uploadEligible(const RuntimeState& state) {
  return syncEligible(state) && !state.pendingAttendanceRecords.empty() && state.snapshots.attendanceConfig.has_value();
}

}  // namespace

bool hasNetworkRequestInFlight(const RuntimeState& state) {
  return state.apiProbeInFlight || state.activationInFlight || state.enrollmentReportInFlight ||
      state.syncInFlight || state.uploadInFlight;
}

NetworkRequestType nextNetworkRequestType(const RuntimeState& state, uint32_t nowMs) {
  if (hasNetworkRequestInFlight(state)) {
    return NetworkRequestType::None;
  }

  if (state.manualApiProbeRequested && state.connectivity == ConnectivityState::Connected && apiConfigured(state)) {
    return NetworkRequestType::ApiProbe;
  }

  if (state.manualActivationRequested && activationEligible(state)) {
    return NetworkRequestType::Activation;
  }

  if (state.manualSyncRequested && syncEligible(state)) {
    return NetworkRequestType::Sync;
  }

  if (app::shouldProbeApi(
          apiConfigured(state),
          state.apiProbeInFlight,
          state.connectivity,
          state.lastApiProbeAttemptMs,
          nowMs)) {
    return NetworkRequestType::ApiProbe;
  }

  if (!state.activationInFlight && activationEligible(state) &&
      (state.lastActivationAttemptMs == 0 ||
       nowMs - state.lastActivationAttemptMs >= board::kActivationRetryIntervalMs)) {
    return NetworkRequestType::Activation;
  }

  if (!state.enrollmentReportInFlight && enrollmentReportEligible(state) &&
      (state.lastEnrollmentReportAttemptMs == 0 ||
       nowMs - state.lastEnrollmentReportAttemptMs >= board::kUploadRetryIntervalMs)) {
    return NetworkRequestType::EnrollmentReport;
  }

  if (!state.syncInFlight && syncEligible(state) &&
      (state.lastSyncAttemptMs == 0 || nowMs - state.lastSyncAttemptMs >= board::kSyncIntervalMs)) {
    return NetworkRequestType::Sync;
  }

  if (!state.uploadInFlight && uploadEligible(state) &&
      (state.lastUploadAttemptMs == 0 || nowMs - state.lastUploadAttemptMs >= board::kUploadRetryIntervalMs)) {
    return NetworkRequestType::Upload;
  }

  return NetworkRequestType::None;
}

}  // namespace app
