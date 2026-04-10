#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "app/runtime_network_planner.hpp"

namespace {

void expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

app::RuntimeState makeConnectedState() {
  app::RuntimeState state = {};
  state.connectivity = app::ConnectivityState::Connected;
  state.deviceConfig.backendLocator.origin = "http://device-api.local";
  return state;
}

void testDoesNotDispatchWhileAnotherRequestIsInFlight() {
  app::RuntimeState state = makeConnectedState();
  state.apiProbeInFlight = true;
  state.manualSyncRequested = true;

  expect(
      app::nextNetworkRequestType(state, 1'000) == app::NetworkRequestType::None,
      "planner should not dispatch a second request while one is still in flight");
}

void testManualRefreshPrioritizesApiProbeFirst() {
  app::RuntimeState state = makeConnectedState();
  state.deviceConfig.bootstrapIdentity.deviceSerial = "BOOT-001";
  state.deviceConfig.bootstrapIdentity.bootstrapSecret = "secret";
  state.manualApiProbeRequested = true;
  state.manualActivationRequested = true;
  state.manualSyncRequested = true;

  expect(
      app::nextNetworkRequestType(state, 1'000) == app::NetworkRequestType::ApiProbe,
      "manual refresh should probe API before activation or sync");
}

void testManualRefreshFallsThroughToActivationAfterProbe() {
  app::RuntimeState state = makeConnectedState();
  state.deviceConfig.bootstrapIdentity.deviceSerial = "BOOT-001";
  state.deviceConfig.bootstrapIdentity.bootstrapSecret = "secret";
  state.manualActivationRequested = true;
  state.manualSyncRequested = true;

  expect(
      app::nextNetworkRequestType(state, 1'000) == app::NetworkRequestType::Activation,
      "manual refresh should activate after probe when credentials are missing");
}

void testManualRefreshCanForceImmediateSync() {
  app::RuntimeState state = makeConnectedState();
  state.credentials.deviceCode = "DEV-001";
  state.credentials.apiKey = "api-key";
  state.lastSyncAttemptMs = 59'500;
  state.manualSyncRequested = true;

  expect(
      app::nextNetworkRequestType(state, 60'000) == app::NetworkRequestType::Sync,
      "manual sync should bypass the regular retry interval");
}

void testUploadRunsWhenQueueIsReady() {
  app::RuntimeState state = makeConnectedState();
  state.credentials.deviceCode = "DEV-001";
  state.credentials.apiKey = "api-key";
  state.snapshots.attendanceConfig = core::AttendanceConfigSnapshot{
      .id = "default",
      .workStartMinute = 540,
      .workEndMinute = 600,
      .offStartMinute = 1080,
      .offEndMinute = 1140,
      .updatedAt = 1,
  };
  state.pendingAttendanceRecords.push_back(core::PendingAttendanceRecord{
      .clientRecordId = "local-001",
      .employeeId = "emp-001",
      .recognizedAt = 1,
      .type = core::AttendanceRecordType::ClockIn,
      .localDate = "2026-04-10",
      .createdAt = 1,
      .lastAttemptAt = std::nullopt,
      .lastResultCode = std::nullopt,
  });

  expect(
      app::nextNetworkRequestType(state, 10'000) == app::NetworkRequestType::ApiProbe,
      "the regular scheduler should probe API health before sync or upload");

  state.lastApiProbeAttemptMs = 10'000;
  state.lastSyncAttemptMs = 10'000;
  expect(
      app::nextNetworkRequestType(state, 10'100) == app::NetworkRequestType::Upload,
      "upload should run once probe and sync are not immediately due");
}

}  // namespace

int main() {
  try {
    testDoesNotDispatchWhileAnotherRequestIsInFlight();
    testManualRefreshPrioritizesApiProbeFirst();
    testManualRefreshFallsThroughToActivationAfterProbe();
    testManualRefreshCanForceImmediateSync();
    testUploadRunsWhenQueueIsReady();
    std::cout << "[PASS] runtime network planner" << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] runtime network planner: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
