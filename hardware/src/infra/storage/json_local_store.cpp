#include "infra/storage/json_local_store.hpp"

#include <cstdio>
#include <optional>
#include <string>
#include <sys/stat.h>
#include <utility>

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Preferences.h>

#include "core/use_cases.hpp"
#include "infra/storage/storage_aux_codec.hpp"

namespace infra {
namespace {

constexpr char kLittleFsMountPath[] = "/littlefs";
constexpr char kSnapshotsPath[] = "/snapshots.json";
constexpr char kAttendanceQueuePath[] = "/attendance_queue.json";
constexpr char kFailureLogsPath[] = "/failure_logs.json";
constexpr char kStorageAuxPath[] = "/storage_aux.json";

std::string loadPreferenceString(Preferences& preferences, const char* key) {
  if (!preferences.isKey(key)) {
    return "";
  }

  return preferences.getString(key, "").c_str();
}

bool littleFsFileExists(const char* path) {
  char mountedPath[64] = {};
  const int written = std::snprintf(mountedPath, sizeof(mountedPath), "%s%s", kLittleFsMountPath, path);
  if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(mountedPath)) {
    return false;
  }

  struct stat fileInfo = {};
  return ::stat(mountedPath, &fileInfo) == 0;
}

bool readJsonFile(const char* path, DynamicJsonDocument& doc) {
  if (!littleFsFileExists(path)) {
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

std::optional<std::string> readTextFile(const char* path) {
  if (!littleFsFileExists(path)) {
    return std::nullopt;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return std::nullopt;
  }

  std::string content;
  content.reserve(file.size());
  while (file.available()) {
    content.push_back(static_cast<char>(file.read()));
  }

  file.close();
  return content;
}

bool writeTextFile(const char* path, const std::string& content) {
  File file = LittleFS.open(path, "w");
  if (!file) {
    return false;
  }

  const std::size_t written =
      file.write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
  file.close();
  return written == content.size();
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

class CredentialsStore {
 public:
  bool begin() {
    return preferences_.begin("hitomi-device", false);
  }

  core::DeviceCredentials load() {
    return {
        .deviceCode = loadPreferenceString(preferences_, "deviceCode"),
        .apiKey = loadPreferenceString(preferences_, "apiKey"),
    };
  }

  bool save(const core::DeviceCredentials& credentials) {
    return preferences_.putString("deviceCode", credentials.deviceCode.c_str()) > 0 &&
           preferences_.putString("apiKey", credentials.apiKey.c_str()) > 0;
  }

 private:
  Preferences preferences_;
};

class SnapshotStore {
 public:
  core::SnapshotBundle load() {
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

  bool save(const core::SnapshotBundle& snapshots) {
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
};

class AttendanceQueueStore {
 public:
  std::vector<core::PendingAttendanceRecord> load() {
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

  bool save(const std::vector<core::PendingAttendanceRecord>& records) {
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
};

class FailureLogStore {
 public:
  std::vector<core::FailureLogEntry> load() {
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

  bool save(const std::vector<core::FailureLogEntry>& logs) {
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
};

class StorageAuxStore {
 public:
  StorageAuxState load() {
    auto content = readTextFile(kStorageAuxPath);
    if (!content.has_value()) {
      return {};
    }

    auto storageAux = decodeStorageAuxState(content.value());
    return storageAux.value_or(StorageAuxState{});
  }

  bool save(const StorageAuxState& storageAux) {
    return writeTextFile(kStorageAuxPath, encodeStorageAuxState(storageAux));
  }
};

}  // namespace

struct JsonLocalStore::Impl {
  CredentialsStore credentialsStore;
  SnapshotStore snapshotStore;
  AttendanceQueueStore attendanceQueueStore;
  FailureLogStore failureLogStore;
  StorageAuxStore storageAuxStore;
  LocalStoreInitStatus initStatus = {};
};

JsonLocalStore::JsonLocalStore() : impl_(std::make_unique<Impl>()) {}

JsonLocalStore::~JsonLocalStore() = default;

JsonLocalStore::JsonLocalStore(JsonLocalStore&&) noexcept = default;

JsonLocalStore& JsonLocalStore::operator=(JsonLocalStore&&) noexcept = default;

LocalStoreInitStatus JsonLocalStore::begin() {
  impl_->initStatus.credentialsReady = impl_->credentialsStore.begin();
  impl_->initStatus.filesystemReady = LittleFS.begin(false);
  return impl_->initStatus;
}

StoredRuntimeState JsonLocalStore::load() {
  StoredRuntimeState state = {};
  if (impl_->initStatus.credentialsReady) {
    state.credentials = impl_->credentialsStore.load();
  }
  if (impl_->initStatus.filesystemReady) {
    state.snapshots = impl_->snapshotStore.load();
    state.pendingAttendanceRecords = impl_->attendanceQueueStore.load();
    state.failureLogs = impl_->failureLogStore.load();
    state.storageAux = impl_->storageAuxStore.load();
  }
  return state;
}

bool JsonLocalStore::saveCredentials(const core::DeviceCredentials& credentials) {
  if (!impl_->initStatus.credentialsReady) {
    return false;
  }
  return impl_->credentialsStore.save(credentials);
}

bool JsonLocalStore::saveSnapshots(const core::SnapshotBundle& snapshots) {
  if (!impl_->initStatus.filesystemReady) {
    return false;
  }
  return impl_->snapshotStore.save(snapshots);
}

bool JsonLocalStore::savePendingAttendanceRecords(
    const std::vector<core::PendingAttendanceRecord>& records) {
  if (!impl_->initStatus.filesystemReady) {
    return false;
  }
  return impl_->attendanceQueueStore.save(records);
}

bool JsonLocalStore::saveFailureLogs(const std::vector<core::FailureLogEntry>& logs) {
  if (!impl_->initStatus.filesystemReady) {
    return false;
  }
  return impl_->failureLogStore.save(logs);
}

bool JsonLocalStore::saveStorageAux(const StorageAuxState& storageAux) {
  if (!impl_->initStatus.filesystemReady) {
    return false;
  }
  return impl_->storageAuxStore.save(storageAux);
}

}  // namespace infra
