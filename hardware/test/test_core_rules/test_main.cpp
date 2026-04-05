#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/runtime_diagnostics.hpp"
#include "app/runtime_status.hpp"
#include "core/models.hpp"
#include "core/use_cases.hpp"
#include "infra/storage/storage_aux_codec.hpp"
#include "infra/storage/template_manifest_codec.hpp"
#include "infra/template_store_port.hpp"
#include "ui/status_screen_presenter.hpp"

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
using infra::StorageAuxState;
using infra::TemplateLibrarySummary;
using infra::TemplateManifest;
using infra::TemplateManifestItem;
using infra::TemplateStoreHealthSummary;
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
              .message = "not in window",
              .retryable = false,
          },
      },
  };

  auto applied = core::applyUploadResults(queue, logs, results, 99);

  expect(applied.queue.empty(), "processed records should be removed from queue");
  expect(applied.logs.size() == 1, "rejected records should append one failure log");
  expect(applied.logs.front().code == "ATTENDANCE_NOT_IN_WINDOW", "failure log should preserve server code");
}

void testBuildShanghaiLocalDateUsesUtcPlusEight() {
  expect(core::buildShanghaiLocalDate(1'743'158'400'000ULL) == "2025-03-28", "local date should use Asia/Shanghai");
}

void testFailureLogHelpersCapRetention() {
  std::vector<FailureLogEntry> logs;
  for (std::size_t i = 0; i < 5; ++i) {
    core::appendFailureLog(
        logs,
        FailureLogEntry{
            .id = core::buildFailureLogId(100 + i, i),
            .api = "/api/device/sync",
            .occurredAt = 100 + i,
            .code = "ERR",
            .message = "boom",
            .relatedId = std::nullopt,
        },
        3);
  }

  expect(logs.size() == 3, "failure logs should keep the most recent entries");
  expect(logs.front().occurredAt == 102, "oldest retained log should be the third inserted one");
}

void testDecodeStorageAuxStateFallsBackToDefaultsOnInvalidPayload() {
  auto decoded = infra::decodeStorageAuxState("{not-json}");
  expect(!decoded.has_value(), "invalid JSON should fail to decode");
}

void testEncodeDecodeStorageAuxStateRoundTrips() {
  StorageAuxState state = {
      .templateLibrarySummary =
          TemplateLibrarySummary{
              .templateCount = 2,
              .manifestUpdatedAt = 123,
              .lastLoadedAt = 456,
          },
      .templateStoreHealth =
          TemplateStoreHealthSummary{
              .statusCode = infra::kTemplateStoreMountedReady,
              .checkedAt = 456,
              .detail = "ok",
              .cardSizeBytes = 1000,
              .totalBytes = 1000,
              .usedBytes = 400,
          },
  };

  const std::string encoded = infra::encodeStorageAuxState(state);
  auto decoded = infra::decodeStorageAuxState(encoded);

  expect(decoded.has_value(), "encoded storage aux state should decode");
  expect(decoded->templateLibrarySummary.templateCount == 2, "template count should round-trip");
  expect(decoded->templateStoreHealth.statusCode == infra::kTemplateStoreMountedReady, "status code should round-trip");
}

void testDecodeTemplateManifestRejectsMissingSchemaVersion() {
  auto manifest = infra::decodeTemplateManifest("{\"updatedAt\": 1, \"items\": []}");
  expect(!manifest.has_value(), "manifest without version should be rejected");
}

void testEncodeDecodeTemplateManifestRoundTrips() {
  TemplateManifest manifest = {
      .schemaVersion = 1,
      .updatedAt = 77,
      .items = {
          TemplateManifestItem{
              .employeeId = "emp_001",
              .updatedAt = 88,
              .sizeBytes = 512,
          },
      },
  };

  const std::string encoded = infra::encodeTemplateManifest(manifest);
  auto decoded = infra::decodeTemplateManifest(encoded);

  expect(decoded.has_value(), "encoded manifest should decode");
  expect(decoded->schemaVersion == 1, "manifest schema version should round-trip");
  expect(decoded->items.size() == 1, "template entries should round-trip");
  expect(decoded->items.front().employeeId == "emp_001", "employee id should round-trip");
  expect(decoded->items.front().sizeBytes == 512, "size bytes should round-trip");
}

void testRuntimeDiagnosticsAndPresenterExposeStatusLines() {
  app::RuntimeStatus status = {};
  status.credentialsReady = true;
  status.filesystemReady = true;
  status.templateStoreReady = true;
  status.faceModuleEnabled = true;
  status.connectivity = app::ConnectivityState::Connected;
  status.pendingQueueSize = 3;
  status.failureLogCount = 1;
  status.credentials = DeviceCredentials{
      .deviceCode = "DEV-001",
      .apiKey = "secret",
  };
  status.snapshots.deviceName = "Hitomi";
  status.snapshots.lastSyncAt = 1'743'158'400'000ULL;
  status.snapshots.employees = {
      EmployeeSnapshot{
          .id = "emp_001",
          .code = "20230001",
          .name = "张三",
          .updatedAt = 1,
      },
  };
  status.apiConfigured = true;
  status.apiProbeSucceeded = true;
  status.firmwareTag = "fw-tag";
  status.templateStoreStatusCode = infra::kTemplateStoreMountedReady;
  status.templateCount = 2;

  const app::RuntimeDiagnostics diagnostics = app::buildRuntimeDiagnostics(status);
  const AppViewModel view = ui::StatusScreenPresenter::build(status);

  expect(diagnostics.credentialsLine.find("DEV-001") != std::string::npos, "diagnostics should surface device code");
  expect(diagnostics.snapshotLine.find("employees=1") != std::string::npos, "diagnostics should describe snapshots");
  expect(view.storageLine.find("templates=2") != std::string::npos, "view should surface template count");
  expect(view.apiLine.find("reachable") != std::string::npos, "view should surface API success");
  expect(view.footer == "fw-tag", "view footer should use firmware tag");
}

}  // namespace

int main() {
  try {
    testApplySyncSnapshotReplacesRemoteSnapshots();
    testClassifyAttendanceTypeUsesShanghaiWindows();
    testEnqueueAttendanceRecordKeepsEarlierDuplicate();
    testApplyUploadResultsRemovesProcessedRecordsAndLogsRejected();
    testBuildShanghaiLocalDateUsesUtcPlusEight();
    testFailureLogHelpersCapRetention();
    testDecodeStorageAuxStateFallsBackToDefaultsOnInvalidPayload();
    testEncodeDecodeStorageAuxStateRoundTrips();
    testDecodeTemplateManifestRejectsMissingSchemaVersion();
    testEncodeDecodeTemplateManifestRoundTrips();
    testRuntimeDiagnosticsAndPresenterExposeStatusLines();
    std::cout << "[PASS] core rules" << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "[FAIL] core rules: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
