#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/runtime_status.hpp"
#include "app/runtime_diagnostics.hpp"
#include "core/models.hpp"
#include "core/use_cases.hpp"
#include "ui/status_screen_presenter.hpp"

#include "../../src/app/runtime_diagnostics.cpp"
#include "../../src/core/use_cases.cpp"
#include "../../src/ui/status_screen_presenter.cpp"

namespace {

using core::ApiError;
using core::AttendanceConfigSnapshot;
using core::AttendanceRecordType;
using core::AttendanceUploadItemResult;
using core::AttendanceUploadStatus;
using core::DeviceCredentials;
using core::EmployeeSnapshot;
using core::EnrollmentTaskSnapshot;
using core::FailureLogEntry;
using core::PendingAttendanceRecord;
using core::SnapshotBundle;
using core::SyncPayload;
using ui::AppViewModel;

void expect(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void testApplySyncSnapshotReplacesRemoteSnapshots() {
  SnapshotBundle existing = {};
  existing.lastSyncAt = 100;
  existing.attendanceConfig = AttendanceConfigSnapshot{
      .id = "stale",
      .workStartMinute = 1,
      .workEndMinute = 2,
      .offStartMinute = 3,
      .offEndMinute = 4,
      .updatedAt = 10,
  };
  existing.employees.push_back(EmployeeSnapshot{
      .id = "old-emp",
      .code = "0001",
      .name = "Old",
      .updatedAt = 20,
  });

  SyncPayload payload = {};
  payload.serverTime = 1'743'158'400'000ULL;
  payload.timezone = "Asia/Shanghai";
  payload.deviceId = "dev_001";
  payload.deviceName = "一号设备";
  payload.deviceStatus = "active";
  payload.attendanceConfig = AttendanceConfigSnapshot{
      .id = "default",
      .workStartMinute = 540,
      .workEndMinute = 600,
      .offStartMinute = 1080,
      .offEndMinute = 1140,
      .updatedAt = 30,
  };
  payload.employees = {
      EmployeeSnapshot{
          .id = "emp_001",
          .code = "20230001",
          .name = "张三",
          .updatedAt = 40,
      },
      EmployeeSnapshot{
          .id = "emp_002",
          .code = "20230002",
          .name = "李四",
          .updatedAt = 50,
      },
  };
  payload.enrollmentTask = EnrollmentTaskSnapshot{
      .taskId = "fp_001",
      .employeeId = "emp_001",
      .employeeCode = "20230001",
      .employeeName = "张三",
      .status = "pending",
      .createdAt = 60,
      .updatedAt = 70,
  };

  SnapshotBundle applied = core::applySyncSnapshot(existing, payload, 999);

  expect(applied.lastSyncAt == 999, "lastSyncAt should update");
  expect(applied.lastServerTime == payload.serverTime, "serverTime should update");
  expect(applied.deviceId == "dev_001", "deviceId should update");
  expect(applied.deviceName == "一号设备", "deviceName should update");
  expect(applied.deviceStatus == "active", "device status should update");
  expect(applied.attendanceConfig.has_value(), "attendance config should exist");
  expect(applied.attendanceConfig->workStartMinute == 540, "attendance config should be replaced");
  expect(applied.employees.size() == 2, "employees should be replaced");
  expect(applied.employees.front().id == "emp_001", "employee order should match payload");
  expect(applied.enrollmentTask.has_value(), "enrollment task should exist");
  expect(applied.enrollmentTask->taskId == "fp_001", "task should update");
}

void testClassifyAttendanceTypeUsesShanghaiWindows() {
  AttendanceConfigSnapshot config = {
      .id = "default",
      .workStartMinute = 540,
      .workEndMinute = 600,
      .offStartMinute = 1080,
      .offEndMinute = 1140,
      .updatedAt = 1,
  };

  auto beforeWindow = core::classifyAttendanceType(config, 1'774'659'540'000ULL);
  auto workWindow = core::classifyAttendanceType(config, 1'774'660'200'000ULL);
  auto offWindow = core::classifyAttendanceType(config, 1'774'692'600'000ULL);

  expect(!beforeWindow.has_value(), "time before work window should be rejected");
  expect(workWindow == AttendanceRecordType::ClockIn, "09:10 CST should be clock_in");
  expect(offWindow == AttendanceRecordType::ClockOut, "18:10 CST should be clock_out");
}

void testEnqueueAttendanceRecordKeepsEarlierDuplicate() {
  std::vector<PendingAttendanceRecord> queue = {
      PendingAttendanceRecord{
          .clientRecordId = "local-001",
          .employeeId = "emp_001",
          .recognizedAt = 1'743'158'400'000ULL,
          .type = AttendanceRecordType::ClockIn,
          .localDate = "2025-03-28",
          .createdAt = 1'743'158'401'000ULL,
          .lastAttemptAt = std::nullopt,
          .lastResultCode = std::nullopt,
      },
  };

  PendingAttendanceRecord earlier = {
      .clientRecordId = "local-002",
      .employeeId = "emp_001",
      .recognizedAt = 1'743'158'300'000ULL,
      .type = AttendanceRecordType::ClockIn,
      .localDate = "2025-03-28",
      .createdAt = 1'743'158'402'000ULL,
      .lastAttemptAt = std::nullopt,
      .lastResultCode = std::nullopt,
  };

  PendingAttendanceRecord later = {
      .clientRecordId = "local-003",
      .employeeId = "emp_001",
      .recognizedAt = 1'743'158'500'000ULL,
      .type = AttendanceRecordType::ClockIn,
      .localDate = "2025-03-28",
      .createdAt = 1'743'158'403'000ULL,
      .lastAttemptAt = std::nullopt,
      .lastResultCode = std::nullopt,
  };

  auto replaced = core::enqueueAttendanceRecord(queue, earlier);
  auto ignored = core::enqueueAttendanceRecord(queue, later);

  expect(replaced.action == core::QueueMutationAction::ReplacedWithEarlier,
         "earlier duplicate should replace queued item");
  expect(ignored.action == core::QueueMutationAction::IgnoredLaterOrEqual,
         "later duplicate should be ignored");
  expect(queue.size() == 1, "queue should still contain one record");
  expect(queue.front().clientRecordId == "local-002", "queue should keep earlier record");
}

void testApplyUploadResultsRemovesProcessedRecordsAndLogsRejected() {
  std::vector<PendingAttendanceRecord> queue = {
      PendingAttendanceRecord{
          .clientRecordId = "local-001",
          .employeeId = "emp_001",
          .recognizedAt = 1,
          .type = AttendanceRecordType::ClockIn,
          .localDate = "2026-03-28",
          .createdAt = 2,
          .lastAttemptAt = std::nullopt,
          .lastResultCode = std::nullopt,
      },
      PendingAttendanceRecord{
          .clientRecordId = "local-002",
          .employeeId = "emp_002",
          .recognizedAt = 3,
          .type = AttendanceRecordType::ClockOut,
          .localDate = "2026-03-28",
          .createdAt = 4,
          .lastAttemptAt = std::nullopt,
          .lastResultCode = std::nullopt,
      },
  };

  std::vector<FailureLogEntry> logs;
  std::vector<AttendanceUploadItemResult> results = {
      AttendanceUploadItemResult{
          .clientRecordId = "local-001",
          .status = AttendanceUploadStatus::Saved,
          .attendanceRecordId = std::optional<std::string>("ar_001"),
          .error = std::nullopt,
      },
      AttendanceUploadItemResult{
          .clientRecordId = "local-002",
          .status = AttendanceUploadStatus::Rejected,
          .attendanceRecordId = std::nullopt,
          .error = ApiError{
              .code = "ATTENDANCE_NOT_IN_WINDOW",
              .message = "recognizedAt is not in valid attendance window",
              .retryable = false,
          },
      },
  };

  auto output = core::applyUploadResults(queue, logs, results, 12345);

  expect(output.queue.empty(), "processed records should be removed from queue");
  expect(output.logs.size() == 1, "rejected record should append one failure log");
  expect(output.logs.front().api == "/api/device/attendance/upload", "failure log api should match");
  expect(output.logs.front().code == "ATTENDANCE_NOT_IN_WINDOW", "failure log code should match");
  expect(output.logs.front().relatedId.has_value(), "failure log should track related record");
  expect(output.logs.front().relatedId.value() == "local-002", "failure log should use clientRecordId");
}

void testAppendFailureLogPrunesToMostRecent200() {
  std::vector<FailureLogEntry> logs;
  for (int i = 0; i < 205; ++i) {
    core::appendFailureLog(
        logs,
        FailureLogEntry{
            .id = "log-" + std::to_string(i),
            .api = "/api/device/sync",
            .occurredAt = static_cast<uint64_t>(i),
            .code = "INTERNAL_ERROR",
            .message = "boom",
            .relatedId = std::nullopt,
        });
  }

  expect(logs.size() == 200, "failure logs should be capped at 200 entries");
  expect(logs.front().id == "log-5", "oldest entries should be pruned first");
  expect(logs.back().id == "log-204", "newest entry should be retained");
}

void testStatusScreenPresenterBuildsStateLines() {
  app::RuntimeStatus status = {};
  status.firmwareTag = "runtime-tag";
  status.credentials = DeviceCredentials{
      .deviceCode = "DEV-001",
      .apiKey = "secret",
  };
  status.snapshots.deviceName = "一号设备";
  status.snapshots.lastSyncAt = 1'743'158'400'000ULL;
  status.snapshots.enrollmentTask = EnrollmentTaskSnapshot{
      .taskId = "fp_001",
      .employeeId = "emp_001",
      .employeeCode = "20230001",
      .employeeName = "张三",
      .status = "pending",
      .createdAt = 1,
      .updatedAt = 2,
  };
  status.pendingQueueSize = 3;
  status.connectivity = app::ConnectivityState::Connected;
  status.credentialsReady = true;
  status.filesystemReady = true;
  status.templateStoreReady = false;
  status.syncInFlight = true;
  status.lastErrorCode = std::optional<std::string>("DEVICE_DISABLED");
  status.faceModuleEnabled = false;

  AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(view.title == "Hitomi Device", "view title should be stable");
  expect(view.credentialsLine.find("DEV-001") != std::string::npos, "credentials line should show device code");
  expect(view.storageLine.find("SD unavailable") != std::string::npos, "storage line should show template store state");
  expect(view.wifiLine.find("Connected") != std::string::npos, "wifi line should show connectivity");
  expect(view.syncLine.find("Syncing") != std::string::npos, "sync line should show active sync");
  expect(view.taskLine.find("张三") != std::string::npos, "task line should show employee name");
  expect(view.queueLine.find("3") != std::string::npos, "queue line should show pending count");
  expect(view.errorLine.find("DEVICE_DISABLED") != std::string::npos, "error line should show latest error");
  expect(view.faceLine.find("Disabled") != std::string::npos, "face line should show module state");
  expect(view.footer == "runtime-tag", "footer should use firmware tag");
}

void testRuntimeDiagnosticsExplainUnconfiguredState() {
  app::RuntimeStatus status = {};
  status.credentialsReady = true;
  status.filesystemReady = true;
  status.templateStoreReady = false;
  status.pendingQueueSize = 0;
  status.failureLogCount = 0;
  status.faceModuleEnabled = false;

  app::RuntimeDiagnostics diagnostics = app::buildRuntimeDiagnostics(status);

  expect(diagnostics.credentialsLine == "Credentials: missing", "diagnostics should report missing credentials");
  expect(diagnostics.snapshotLine == "Local cache: empty", "diagnostics should report empty cache");
  expect(diagnostics.queueLine == "Pending uploads: 0, failure logs: 0", "diagnostics should report queue and log counts");
  expect(
      diagnostics.faceLine == "Recognition: disabled (template store unavailable)",
      "diagnostics should explain why recognition is disabled");
}

}  // namespace

int main() {
  std::vector<std::pair<std::string, std::function<void()>>> tests = {
      {"apply sync snapshot", testApplySyncSnapshotReplacesRemoteSnapshots},
      {"classify attendance type", testClassifyAttendanceTypeUsesShanghaiWindows},
      {"enqueue attendance record", testEnqueueAttendanceRecordKeepsEarlierDuplicate},
      {"apply upload results", testApplyUploadResultsRemovesProcessedRecordsAndLogsRejected},
      {"append failure log", testAppendFailureLogPrunesToMostRecent200},
      {"status screen presenter", testStatusScreenPresenterBuildsStateLines},
      {"runtime diagnostics", testRuntimeDiagnosticsExplainUnconfiguredState},
  };

  for (const auto& test : tests) {
    try {
      test.second();
      std::cout << "[PASS] " << test.first << '\n';
    } catch (const std::exception& error) {
      std::cerr << "[FAIL] " << test.first << ": " << error.what() << '\n';
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
