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
#include "../json_snapshot_codec.hpp"

namespace infra {
namespace {

constexpr char kLittleFsMountPath[] = "/littlefs";
constexpr char kSnapshotsPath[] = "/snapshots.json";
constexpr char kEnrollmentReportsPath[] = "/enrollment_reports.json";
constexpr char kAttendanceQueuePath[] = "/attendance_queue.json";
constexpr char kLocalAttendanceMarksPath[] = "/attendance_marks.json";
constexpr char kFailureLogsPath[] = "/failure_logs.json";
constexpr char kStorageAuxPath[] = "/storage_aux.json";
constexpr char kDeviceConfigKey[] = "deviceConfig";

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

bool readJsonFile(const char* path, JsonDocument& doc) {
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

bool writeJsonFile(const char* path, const JsonDocument& doc) {
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

constexpr char kFixedOriginMode[] = "fixed_origin";

std::string encodeDeviceConfig(const core::DeviceConfig& config) {
  JsonDocument doc;
  doc["schemaVersion"] = config.schemaVersion;
  doc["diagnosticsEnabled"] = config.diagnosticsEnabled;

  JsonObject backendLocator = doc["backendLocator"].to<JsonObject>();
  backendLocator["mode"] = kFixedOriginMode;
  backendLocator["origin"] = config.backendLocator.origin;

  JsonObject bootstrapIdentity = doc["bootstrapIdentity"].to<JsonObject>();
  bootstrapIdentity["deviceSerial"] = config.bootstrapIdentity.deviceSerial;
  bootstrapIdentity["bootstrapSecret"] = config.bootstrapIdentity.bootstrapSecret;

  JsonObject runtimeCredentials = doc["runtimeCredentials"].to<JsonObject>();
  runtimeCredentials["deviceCode"] = config.runtimeCredentials.deviceCode;
  runtimeCredentials["apiKey"] = config.runtimeCredentials.apiKey;

  JsonArray wifiProfiles = doc["wifiProfiles"].to<JsonArray>();
  for (const auto& profile : config.wifiProfiles) {
    JsonObject item = wifiProfiles.add<JsonObject>();
    item["ssid"] = profile.ssid;
    item["password"] = profile.password;
    item["priority"] = profile.priority;
    item["lastSuccessAt"] = profile.lastSuccessAt;
    item["lastSuccessBssid"] = profile.lastSuccessBssid;
    item["lastSuccessChannel"] = profile.lastSuccessChannel;
    item["disabled"] = profile.disabled;
  }

  std::string output;
  serializeJson(doc, output);
  return output;
}

std::optional<core::DeviceConfig> decodeDeviceConfig(const std::string& payload) {
  JsonDocument doc;
  const auto error = deserializeJson(doc, payload);
  if (error) {
    return std::nullopt;
  }

  core::DeviceConfig config = {};
  config.schemaVersion = doc["schemaVersion"] | 1U;
  config.diagnosticsEnabled = doc["diagnosticsEnabled"] | false;

  JsonVariantConst backendLocator = doc["backendLocator"];
  config.backendLocator.mode = core::BackendLocatorMode::FixedOrigin;
  config.backendLocator.origin = backendLocator["origin"] | "";

  JsonVariantConst bootstrapIdentity = doc["bootstrapIdentity"];
  config.bootstrapIdentity.deviceSerial = bootstrapIdentity["deviceSerial"] | "";
  config.bootstrapIdentity.bootstrapSecret = bootstrapIdentity["bootstrapSecret"] | "";

  JsonVariantConst runtimeCredentials = doc["runtimeCredentials"];
  config.runtimeCredentials.deviceCode = runtimeCredentials["deviceCode"] | "";
  config.runtimeCredentials.apiKey = runtimeCredentials["apiKey"] | "";

  JsonArrayConst wifiProfiles = doc["wifiProfiles"].as<JsonArrayConst>();
  for (JsonVariantConst item : wifiProfiles) {
    config.wifiProfiles.push_back(core::WifiProfile{
        .ssid = item["ssid"] | "",
        .password = item["password"] | "",
        .priority = item["priority"] | 0,
        .lastSuccessAt = item["lastSuccessAt"] | 0ULL,
        .lastSuccessBssid = item["lastSuccessBssid"] | "",
        .lastSuccessChannel = static_cast<uint8_t>(item["lastSuccessChannel"] | 0),
        .disabled = item["disabled"] | false,
    });
  }

  return config;
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

class DeviceConfigStore {
 public:
  bool begin() {
    return preferences_.begin("hitomi-device", false);
  }

  core::DeviceConfig load() {
    if (!preferences_.isKey(kDeviceConfigKey)) {
      return {};
    }

    auto decoded = decodeDeviceConfig(preferences_.getString(kDeviceConfigKey, "").c_str());
    return decoded.value_or(core::DeviceConfig{});
  }

  bool save(const core::DeviceConfig& config) {
    return preferences_.putString(kDeviceConfigKey, encodeDeviceConfig(config).c_str()) > 0;
  }

  bool clear() {
    preferences_.remove(kDeviceConfigKey);
    preferences_.remove("deviceCode");
    preferences_.remove("apiKey");
    return true;
  }

 private:
  Preferences preferences_;
};

class SnapshotStore {
 public:
  core::SnapshotBundle load() {
    core::SnapshotBundle snapshots = {};
    JsonDocument doc;
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
      snapshots.attendanceConfig = infra::detail::parseAttendanceConfig(doc["attendanceConfig"]);
    }
    JsonArrayConst enrollmentTasks = doc["enrollmentTasks"].as<JsonArrayConst>();
    for (JsonVariantConst item : enrollmentTasks) {
      snapshots.enrollmentTasks.push_back(infra::detail::parseEnrollmentTask(item));
    }
    if (snapshots.enrollmentTasks.empty() && !doc["enrollmentTask"].isNull()) {
      snapshots.enrollmentTasks.push_back(infra::detail::parseEnrollmentTask(doc["enrollmentTask"]));
    }

    JsonArrayConst employees = doc["employees"].as<JsonArrayConst>();
    for (JsonVariantConst item : employees) {
      snapshots.employees.push_back(infra::detail::parseEmployee(item));
    }

    return snapshots;
  }

  bool save(const core::SnapshotBundle& snapshots) {
    JsonDocument doc;
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
      JsonObject config = doc["attendanceConfig"].to<JsonObject>();
      config["id"] = snapshots.attendanceConfig->id;
      config["workStartMinute"] = snapshots.attendanceConfig->workStartMinute;
      config["workEndMinute"] = snapshots.attendanceConfig->workEndMinute;
      config["offStartMinute"] = snapshots.attendanceConfig->offStartMinute;
      config["offEndMinute"] = snapshots.attendanceConfig->offEndMinute;
      config["updatedAt"] = snapshots.attendanceConfig->updatedAt;
    } else {
      doc["attendanceConfig"] = nullptr;
    }

    JsonArray employees = doc["employees"].to<JsonArray>();
    for (const auto& employee : snapshots.employees) {
      JsonObject item = employees.add<JsonObject>();
      item["id"] = employee.id;
      item["code"] = employee.code;
      item["name"] = employee.name;
      item["updatedAt"] = employee.updatedAt;
    }

    JsonArray enrollmentTasks = doc["enrollmentTasks"].to<JsonArray>();
    for (const auto& taskItem : snapshots.enrollmentTasks) {
      JsonObject task = enrollmentTasks.add<JsonObject>();
      task["taskId"] = taskItem.taskId;
      task["employeeId"] = taskItem.employeeId;
      task["employeeCode"] = taskItem.employeeCode;
      task["employeeName"] = taskItem.employeeName;
      task["status"] = taskItem.status;
      task["createdAt"] = taskItem.createdAt;
      task["updatedAt"] = taskItem.updatedAt;
    }

    return writeJsonFile(kSnapshotsPath, doc);
  }
};

class AttendanceQueueStore {
 public:
  std::vector<core::PendingAttendanceRecord> load() {
    std::vector<core::PendingAttendanceRecord> records;
    JsonDocument doc;
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
    JsonDocument doc;
    JsonArray items = doc["items"].to<JsonArray>();
    for (const auto& record : records) {
      JsonObject item = items.add<JsonObject>();
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

class LocalAttendanceMarkStore {
 public:
  std::vector<core::LocalAttendanceMark> load() {
    std::vector<core::LocalAttendanceMark> marks;
    JsonDocument doc;
    if (!readJsonFile(kLocalAttendanceMarksPath, doc)) {
      return marks;
    }

    JsonArrayConst items = doc["items"].as<JsonArrayConst>();
    for (JsonVariantConst item : items) {
      auto type = core::attendanceTypeFromApiValue(item["type"] | "");
      if (!type.has_value()) {
        continue;
      }

      marks.push_back(core::LocalAttendanceMark{
          .employeeId = item["employeeId"] | "",
          .localDate = item["localDate"] | "",
          .type = type.value(),
          .recognizedAt = item["recognizedAt"] | 0ULL,
          .uploaded = item["uploaded"] | false,
      });
    }

    return marks;
  }

  bool save(const std::vector<core::LocalAttendanceMark>& marks) {
    JsonDocument doc;
    JsonArray items = doc["items"].to<JsonArray>();
    for (const auto& mark : marks) {
      JsonObject item = items.add<JsonObject>();
      item["employeeId"] = mark.employeeId;
      item["localDate"] = mark.localDate;
      item["type"] = core::attendanceTypeToApiValue(mark.type);
      item["recognizedAt"] = mark.recognizedAt;
      item["uploaded"] = mark.uploaded;
    }

    return writeJsonFile(kLocalAttendanceMarksPath, doc);
  }
};

class EnrollmentReportStore {
 public:
  std::vector<core::PendingEnrollmentReport> load() {
    std::vector<core::PendingEnrollmentReport> reports;
    JsonDocument doc;
    if (!readJsonFile(kEnrollmentReportsPath, doc)) {
      return reports;
    }

    JsonArrayConst items = doc["items"].as<JsonArrayConst>();
    for (JsonVariantConst item : items) {
      reports.push_back(core::PendingEnrollmentReport{
          .taskId = item["taskId"] | "",
          .employeeId = item["employeeId"] | "",
          .result = item["result"] | "",
          .finishedAt = item["finishedAt"] | 0ULL,
          .failureReason = item["failureReason"].isNull()
              ? std::nullopt
              : std::optional<std::string>(item["failureReason"].as<const char*>()),
          .lastAttemptAt = item["lastAttemptAt"].isNull()
              ? std::nullopt
              : std::optional<uint64_t>(item["lastAttemptAt"] | 0ULL),
          .lastResultCode = item["lastResultCode"].isNull()
              ? std::nullopt
              : std::optional<std::string>(item["lastResultCode"].as<const char*>()),
      });
    }

    return reports;
  }

  bool save(const std::vector<core::PendingEnrollmentReport>& reports) {
    JsonDocument doc;
    JsonArray items = doc["items"].to<JsonArray>();
    for (const auto& report : reports) {
      JsonObject item = items.add<JsonObject>();
      item["taskId"] = report.taskId;
      item["employeeId"] = report.employeeId;
      item["result"] = report.result;
      item["finishedAt"] = report.finishedAt;
      if (report.failureReason.has_value()) {
        item["failureReason"] = report.failureReason.value();
      } else {
        item["failureReason"] = nullptr;
      }
      if (report.lastAttemptAt.has_value()) {
        item["lastAttemptAt"] = report.lastAttemptAt.value();
      } else {
        item["lastAttemptAt"] = nullptr;
      }
      if (report.lastResultCode.has_value()) {
        item["lastResultCode"] = report.lastResultCode.value();
      } else {
        item["lastResultCode"] = nullptr;
      }
    }
    return writeJsonFile(kEnrollmentReportsPath, doc);
  }
};

class FailureLogStore {
 public:
  std::vector<core::FailureLogEntry> load() {
    std::vector<core::FailureLogEntry> logs;
    JsonDocument doc;
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
    JsonDocument doc;
    JsonArray items = doc["items"].to<JsonArray>();
    for (const auto& log : logs) {
      JsonObject item = items.add<JsonObject>();
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
  DeviceConfigStore deviceConfigStore;
  CredentialsStore credentialsStore;
  SnapshotStore snapshotStore;
  EnrollmentReportStore enrollmentReportStore;
  AttendanceQueueStore attendanceQueueStore;
  LocalAttendanceMarkStore localAttendanceMarkStore;
  FailureLogStore failureLogStore;
  StorageAuxStore storageAuxStore;
  LocalStoreInitStatus initStatus = {};
};

JsonLocalStore::JsonLocalStore() : impl_(std::make_unique<Impl>()) {}

JsonLocalStore::~JsonLocalStore() = default;

JsonLocalStore::JsonLocalStore(JsonLocalStore&&) noexcept = default;

JsonLocalStore& JsonLocalStore::operator=(JsonLocalStore&&) noexcept = default;

LocalStoreInitStatus JsonLocalStore::begin() {
  impl_->initStatus.credentialsReady = impl_->credentialsStore.begin() && impl_->deviceConfigStore.begin();
  impl_->initStatus.filesystemReady = LittleFS.begin(false);
  return impl_->initStatus;
}

StoredRuntimeState JsonLocalStore::load() {
  StoredRuntimeState state = {};
  if (impl_->initStatus.credentialsReady) {
    state.deviceConfig = impl_->deviceConfigStore.load();
    state.credentials = state.deviceConfig.runtimeCredentials;
    if (!state.credentials.configured()) {
      state.credentials = impl_->credentialsStore.load();
      if (state.credentials.configured()) {
        state.deviceConfig.runtimeCredentials = state.credentials;
      }
    }
  }
  if (impl_->initStatus.filesystemReady) {
    state.snapshots = impl_->snapshotStore.load();
    state.pendingEnrollmentReports = impl_->enrollmentReportStore.load();
    state.pendingAttendanceRecords = impl_->attendanceQueueStore.load();
    state.localAttendanceMarks = impl_->localAttendanceMarkStore.load();
    state.failureLogs = impl_->failureLogStore.load();
    state.storageAux = impl_->storageAuxStore.load();
  }
  return state;
}

bool JsonLocalStore::saveDeviceConfig(const core::DeviceConfig& config) {
  if (!impl_->initStatus.credentialsReady) {
    return false;
  }
  return impl_->deviceConfigStore.save(config);
}

bool JsonLocalStore::clearDeviceConfig() {
  if (!impl_->initStatus.credentialsReady) {
    return false;
  }
  return impl_->deviceConfigStore.clear();
}

bool JsonLocalStore::saveCredentials(const core::DeviceCredentials& credentials) {
  if (!impl_->initStatus.credentialsReady) {
    return false;
  }
  core::DeviceConfig config = impl_->deviceConfigStore.load();
  config.runtimeCredentials = credentials;
  return impl_->credentialsStore.save(credentials) && impl_->deviceConfigStore.save(config);
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

bool JsonLocalStore::savePendingEnrollmentReports(
    const std::vector<core::PendingEnrollmentReport>& reports) {
  if (!impl_->initStatus.filesystemReady) {
    return false;
  }
  return impl_->enrollmentReportStore.save(reports);
}

bool JsonLocalStore::saveLocalAttendanceMarks(
    const std::vector<core::LocalAttendanceMark>& marks) {
  if (!impl_->initStatus.filesystemReady) {
    return false;
  }
  return impl_->localAttendanceMarkStore.save(marks);
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
