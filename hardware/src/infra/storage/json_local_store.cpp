#include "infra/storage/json_local_store.hpp"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "core/use_cases.hpp"

namespace infra {
namespace {

constexpr char kSnapshotsPath[] = "/snapshots.json";
constexpr char kAttendanceQueuePath[] = "/attendance_queue.json";
constexpr char kFailureLogsPath[] = "/failure_logs.json";

bool readJsonFile(const char* path, DynamicJsonDocument& doc) {
  if (!LittleFS.exists(path)) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  const auto error = deserializeJson(doc, file);
  file.close();
  return !error;
}

bool writeJsonFile(const char* path, const DynamicJsonDocument& doc) {
  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }

  const std::size_t written = serializeJson(doc, file);
  file.close();
  return written > 0;
}

core::AttendanceConfigSnapshot parseAttendanceConfig(JsonVariantConst value) {
  return {
      .id = value["id"] | "",
      .workStartMinute = value["workStartMinute"] | 0,
      .workEndMinute = value["workEndMinute"] | 0,
      .offStartMinute = value["offStartMinute"] | 0,
      .offEndMinute = value["offEndMinute"] | 0,
      .updatedAt = value["updatedAt"] | 0ULL,
  };
}

core::EnrollmentTaskSnapshot parseEnrollmentTask(JsonVariantConst value) {
  return {
      .taskId = value["taskId"] | "",
      .employeeId = value["employeeId"] | "",
      .employeeCode = value["employeeCode"] | "",
      .employeeName = value["employeeName"] | "",
      .status = value["status"] | "",
      .createdAt = value["createdAt"] | 0ULL,
      .updatedAt = value["updatedAt"] | 0ULL,
  };
}

}  // namespace

bool CredentialsStore::begin() {
  return preferences_.begin("hitomi-device", false);
}

core::DeviceCredentials CredentialsStore::load() {
  return {
      .deviceCode = preferences_.getString("deviceCode", "").c_str(),
      .apiKey = preferences_.getString("apiKey", "").c_str(),
  };
}

bool CredentialsStore::save(const core::DeviceCredentials& credentials) {
  return preferences_.putString("deviceCode", credentials.deviceCode.c_str()) > 0 &&
         preferences_.putString("apiKey", credentials.apiKey.c_str()) > 0;
}

core::SnapshotBundle SnapshotStore::load() {
  core::SnapshotBundle snapshots = {};
  DynamicJsonDocument doc(32 * 1024);
  if (!readJsonFile(kSnapshotsPath, doc)) {
    return snapshots;
  }

  snapshots.timezone = doc["timezone"] | "Asia/Shanghai";
  snapshots.deviceId = doc["deviceId"] | "";
  snapshots.deviceName = doc["deviceName"] | "";
  snapshots.deviceStatus = doc["deviceStatus"] | "";
  snapshots.attendanceConfigSyncedAt = doc["attendanceConfigSyncedAt"] | 0ULL;
  snapshots.employeesSyncedAt = doc["employeesSyncedAt"] | 0ULL;
  snapshots.enrollmentTaskSyncedAt = doc["enrollmentTaskSyncedAt"] | 0ULL;
  snapshots.lastSyncAt = doc["lastSyncAt"] | 0ULL;
  snapshots.lastServerTime = doc["lastServerTime"] | 0ULL;

  if (!doc["attendanceConfig"].isNull()) {
    snapshots.attendanceConfig = parseAttendanceConfig(doc["attendanceConfig"]);
  }
  if (!doc["enrollmentTask"].isNull()) {
    snapshots.enrollmentTask = parseEnrollmentTask(doc["enrollmentTask"]);
  }

  JsonArrayConst employees = doc["employees"].as<JsonArrayConst>();
  for (JsonVariantConst item : employees) {
    snapshots.employees.push_back(core::EmployeeSnapshot{
        .id = item["id"] | "",
        .code = item["code"] | "",
        .name = item["name"] | "",
        .updatedAt = item["updatedAt"] | 0ULL,
    });
  }

  return snapshots;
}

bool SnapshotStore::save(const core::SnapshotBundle& snapshots) {
  DynamicJsonDocument doc(32 * 1024);
  doc["timezone"] = snapshots.timezone;
  doc["deviceId"] = snapshots.deviceId;
  doc["deviceName"] = snapshots.deviceName;
  doc["deviceStatus"] = snapshots.deviceStatus;
  doc["attendanceConfigSyncedAt"] = snapshots.attendanceConfigSyncedAt;
  doc["employeesSyncedAt"] = snapshots.employeesSyncedAt;
  doc["enrollmentTaskSyncedAt"] = snapshots.enrollmentTaskSyncedAt;
  doc["lastSyncAt"] = snapshots.lastSyncAt;
  doc["lastServerTime"] = snapshots.lastServerTime;

  if (snapshots.attendanceConfig.has_value()) {
    JsonObject config = doc.createNestedObject("attendanceConfig");
    config["id"] = snapshots.attendanceConfig->id;
    config["workStartMinute"] = snapshots.attendanceConfig->workStartMinute;
    config["workEndMinute"] = snapshots.attendanceConfig->workEndMinute;
    config["offStartMinute"] = snapshots.attendanceConfig->offStartMinute;
    config["offEndMinute"] = snapshots.attendanceConfig->offEndMinute;
    config["updatedAt"] = snapshots.attendanceConfig->updatedAt;
  } else {
    doc["attendanceConfig"] = nullptr;
  }

  JsonArray employees = doc.createNestedArray("employees");
  for (const auto& employee : snapshots.employees) {
    JsonObject item = employees.createNestedObject();
    item["id"] = employee.id;
    item["code"] = employee.code;
    item["name"] = employee.name;
    item["updatedAt"] = employee.updatedAt;
  }

  if (snapshots.enrollmentTask.has_value()) {
    JsonObject task = doc.createNestedObject("enrollmentTask");
    task["taskId"] = snapshots.enrollmentTask->taskId;
    task["employeeId"] = snapshots.enrollmentTask->employeeId;
    task["employeeCode"] = snapshots.enrollmentTask->employeeCode;
    task["employeeName"] = snapshots.enrollmentTask->employeeName;
    task["status"] = snapshots.enrollmentTask->status;
    task["createdAt"] = snapshots.enrollmentTask->createdAt;
    task["updatedAt"] = snapshots.enrollmentTask->updatedAt;
  } else {
    doc["enrollmentTask"] = nullptr;
  }

  return writeJsonFile(kSnapshotsPath, doc);
}

std::vector<core::PendingAttendanceRecord> AttendanceQueueStore::load() {
  std::vector<core::PendingAttendanceRecord> records;
  DynamicJsonDocument doc(32 * 1024);
  if (!readJsonFile(kAttendanceQueuePath, doc)) {
    return records;
  }

  JsonArrayConst items = doc["items"].as<JsonArrayConst>();
  for (JsonVariantConst item : items) {
    auto type = core::attendanceTypeFromApiValue(item["type"] | "");
    if (!type.has_value()) {
      continue;
    }

    core::PendingAttendanceRecord record = {
        .clientRecordId = item["clientRecordId"] | "",
        .employeeId = item["employeeId"] | "",
        .recognizedAt = item["recognizedAt"] | 0ULL,
        .type = type.value(),
        .localDate = item["localDate"] | "",
        .createdAt = item["createdAt"] | 0ULL,
        .lastAttemptAt = item["lastAttemptAt"].isNull()
            ? std::nullopt
            : std::optional<uint64_t>(item["lastAttemptAt"] | 0ULL),
        .lastResultCode = item["lastResultCode"].isNull()
            ? std::nullopt
            : std::optional<std::string>(item["lastResultCode"].as<const char*>()),
    };
    records.push_back(record);
  }

  return records;
}

bool AttendanceQueueStore::save(const std::vector<core::PendingAttendanceRecord>& records) {
  DynamicJsonDocument doc(32 * 1024);
  JsonArray items = doc.createNestedArray("items");
  for (const auto& record : records) {
    JsonObject item = items.createNestedObject();
    item["clientRecordId"] = record.clientRecordId;
    item["employeeId"] = record.employeeId;
    item["recognizedAt"] = record.recognizedAt;
    item["type"] = core::attendanceTypeToApiValue(record.type);
    item["localDate"] = record.localDate;
    item["createdAt"] = record.createdAt;
    if (record.lastAttemptAt.has_value()) {
      item["lastAttemptAt"] = record.lastAttemptAt.value();
    } else {
      item["lastAttemptAt"] = nullptr;
    }
    if (record.lastResultCode.has_value()) {
      item["lastResultCode"] = record.lastResultCode.value();
    } else {
      item["lastResultCode"] = nullptr;
    }
  }

  return writeJsonFile(kAttendanceQueuePath, doc);
}

std::vector<core::FailureLogEntry> FailureLogStore::load() {
  std::vector<core::FailureLogEntry> logs;
  DynamicJsonDocument doc(24 * 1024);
  if (!readJsonFile(kFailureLogsPath, doc)) {
    return logs;
  }

  JsonArrayConst items = doc["items"].as<JsonArrayConst>();
  for (JsonVariantConst item : items) {
    logs.push_back(core::FailureLogEntry{
        .id = item["id"] | "",
        .api = item["api"] | "",
        .occurredAt = item["occurredAt"] | 0ULL,
        .code = item["code"] | "",
        .message = item["message"] | "",
        .relatedId = item["relatedId"].isNull()
            ? std::nullopt
            : std::optional<std::string>(item["relatedId"].as<const char*>()),
    });
  }

  return logs;
}

bool FailureLogStore::save(const std::vector<core::FailureLogEntry>& logs) {
  DynamicJsonDocument doc(24 * 1024);
  JsonArray items = doc.createNestedArray("items");
  for (const auto& log : logs) {
    JsonObject item = items.createNestedObject();
    item["id"] = log.id;
    item["api"] = log.api;
    item["occurredAt"] = log.occurredAt;
    item["code"] = log.code;
    item["message"] = log.message;
    if (log.relatedId.has_value()) {
      item["relatedId"] = log.relatedId.value();
    } else {
      item["relatedId"] = nullptr;
    }
  }

  return writeJsonFile(kFailureLogsPath, doc);
}

bool JsonLocalStore::begin() {
  return LittleFS.begin(true) && credentialsStore_.begin();
}

StoredRuntimeState JsonLocalStore::load() {
  return {
      .credentials = credentialsStore_.load(),
      .snapshots = snapshotStore_.load(),
      .pendingAttendanceRecords = attendanceQueueStore_.load(),
      .failureLogs = failureLogStore_.load(),
  };
}

bool JsonLocalStore::saveCredentials(const core::DeviceCredentials& credentials) {
  return credentialsStore_.save(credentials);
}

bool JsonLocalStore::saveSnapshots(const core::SnapshotBundle& snapshots) {
  return snapshotStore_.save(snapshots);
}

bool JsonLocalStore::savePendingAttendanceRecords(
    const std::vector<core::PendingAttendanceRecord>& records) {
  return attendanceQueueStore_.save(records);
}

bool JsonLocalStore::saveFailureLogs(const std::vector<core::FailureLogEntry>& logs) {
  return failureLogStore_.save(logs);
}

}  // namespace infra
