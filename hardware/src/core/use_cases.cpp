#include "core/use_cases.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <sstream>

namespace core {
namespace {

constexpr int kShanghaiUtcOffsetSeconds = 8 * 60 * 60;

std::tm toShanghaiTm(uint64_t epochMs) {
  std::time_t localSeconds = static_cast<std::time_t>(epochMs / 1000ULL) + kShanghaiUtcOffsetSeconds;
  std::tm tmValue = {};
#if defined(_WIN32)
  gmtime_s(&tmValue, &localSeconds);
#else
  gmtime_r(&localSeconds, &tmValue);
#endif
  return tmValue;
}

int minuteOfDay(uint64_t epochMs) {
  std::tm tmValue = toShanghaiTm(epochMs);
  return (tmValue.tm_hour * 60) + tmValue.tm_min;
}

bool sameQueueKey(const PendingAttendanceRecord& left, const PendingAttendanceRecord& right) {
  return left.localDate == right.localDate && left.employeeId == right.employeeId &&
         left.type == right.type;
}

bool compareWifiProfiles(const WifiProfile& left, const WifiProfile& right) {
  if (left.priority != right.priority) {
    return left.priority > right.priority;
  }
  if (left.lastSuccessAt != right.lastSuccessAt) {
    return left.lastSuccessAt > right.lastSuccessAt;
  }
  return left.ssid < right.ssid;
}

FailureLogEntry buildUploadFailureLog(
    uint64_t occurredAt, std::size_t ordinal, const AttendanceUploadItemResult& result) {
  FailureLogEntry entry = {};
  entry.id = buildFailureLogId(occurredAt, ordinal);
  entry.api = "/api/device/attendance/upload";
  entry.occurredAt = occurredAt;
  entry.code = result.error.has_value() ? result.error->code : "UNKNOWN_UPLOAD_ERROR";
  entry.message = result.error.has_value() ? result.error->message : "upload result rejected";
  entry.relatedId = result.clientRecordId;
  return entry;
}

}  // namespace

SnapshotBundle applySyncSnapshot(
    const SnapshotBundle& current, const SyncPayload& payload, uint64_t syncedAt) {
  SnapshotBundle next = current;
  next.timezone = payload.timezone.empty() ? current.timezone : payload.timezone;
  next.deviceId = payload.deviceId;
  next.deviceName = payload.deviceName;
  next.deviceStatus = payload.deviceStatus;
  next.attendanceConfig = payload.attendanceConfig;
  next.attendanceConfigSyncedAt = syncedAt;
  next.employees = payload.employees;
  next.employeesSyncedAt = syncedAt;
  next.enrollmentTasks = payload.enrollmentTasks;
  next.enrollmentTaskSyncedAt = syncedAt;
  next.lastSyncAt = syncedAt;
  next.lastServerTime = payload.serverTime;
  return next;
}

std::optional<AttendanceRecordType> classifyAttendanceType(
    const AttendanceConfigSnapshot& config, uint64_t recognizedAt) {
  int minute = minuteOfDay(recognizedAt);
  if (minute >= config.workStartMinute && minute < config.workEndMinute) {
    return AttendanceRecordType::ClockIn;
  }
  if (minute >= config.offStartMinute && minute < config.offEndMinute) {
    return AttendanceRecordType::ClockOut;
  }
  return std::nullopt;
}

QueueMutationResult enqueueAttendanceRecord(
    std::vector<PendingAttendanceRecord>& queue, const PendingAttendanceRecord& candidate) {
  auto it = std::find_if(queue.begin(), queue.end(), [&](const PendingAttendanceRecord& current) {
    return sameQueueKey(current, candidate);
  });

  if (it == queue.end()) {
    queue.push_back(candidate);
    return {
        .action = QueueMutationAction::Inserted,
        .index = queue.empty() ? 0U : queue.size() - 1U,
    };
  }

  std::size_t index = static_cast<std::size_t>(std::distance(queue.begin(), it));
  if (candidate.recognizedAt < it->recognizedAt) {
    *it = candidate;
    return {
        .action = QueueMutationAction::ReplacedWithEarlier,
        .index = index,
    };
  }

  return {
      .action = QueueMutationAction::IgnoredLaterOrEqual,
      .index = index,
  };
}

UploadApplicationResult applyUploadResults(
    const std::vector<PendingAttendanceRecord>& queue,
    const std::vector<FailureLogEntry>& logs,
    const std::vector<AttendanceUploadItemResult>& results,
    uint64_t occurredAt) {
  UploadApplicationResult output = {
      .queue = queue,
      .logs = logs,
  };

  std::size_t ordinal = 0;
  for (const auto& result : results) {
    auto queueIt =
        std::find_if(output.queue.begin(), output.queue.end(), [&](const PendingAttendanceRecord& item) {
          return item.clientRecordId == result.clientRecordId;
        });
    if (queueIt == output.queue.end()) {
      continue;
    }

    if (result.status == AttendanceUploadStatus::Rejected) {
      appendFailureLog(output.logs, buildUploadFailureLog(occurredAt, ordinal, result));
    }

    output.queue.erase(queueIt);
    ++ordinal;
  }

  return output;
}

void pruneFailureLogs(std::vector<FailureLogEntry>& logs, std::size_t limit) {
  if (logs.size() <= limit) {
    return;
  }

  logs.erase(logs.begin(), logs.begin() + static_cast<std::ptrdiff_t>(logs.size() - limit));
}

void appendFailureLog(
    std::vector<FailureLogEntry>& logs, const FailureLogEntry& entry, std::size_t limit) {
  logs.push_back(entry);
  pruneFailureLogs(logs, limit);
}

std::string attendanceTypeToApiValue(AttendanceRecordType type) {
  switch (type) {
    case AttendanceRecordType::ClockIn:
      return "clock_in";
    case AttendanceRecordType::ClockOut:
      return "clock_out";
    default:
      return "clock_in";
  }
}

std::optional<AttendanceRecordType> attendanceTypeFromApiValue(const std::string& value) {
  if (value == "clock_in") {
    return AttendanceRecordType::ClockIn;
  }
  if (value == "clock_out") {
    return AttendanceRecordType::ClockOut;
  }
  return std::nullopt;
}

std::string attendanceUploadStatusToApiValue(AttendanceUploadStatus status) {
  switch (status) {
    case AttendanceUploadStatus::Saved:
      return "saved";
    case AttendanceUploadStatus::UpdatedEarlier:
      return "updated_earlier";
    case AttendanceUploadStatus::IgnoredDuplicate:
      return "ignored_duplicate";
    case AttendanceUploadStatus::Rejected:
      return "rejected";
    default:
      return "saved";
  }
}

std::optional<AttendanceUploadStatus> attendanceUploadStatusFromApiValue(const std::string& value) {
  if (value == "saved") {
    return AttendanceUploadStatus::Saved;
  }
  if (value == "updated_earlier") {
    return AttendanceUploadStatus::UpdatedEarlier;
  }
  if (value == "ignored_duplicate") {
    return AttendanceUploadStatus::IgnoredDuplicate;
  }
  if (value == "rejected") {
    return AttendanceUploadStatus::Rejected;
  }
  return std::nullopt;
}

std::optional<std::size_t> chooseWifiProfile(
    const std::vector<WifiProfile>& profiles, const std::vector<std::string>& availableSsids) {
  std::optional<std::size_t> bestIndex;
  for (std::size_t index = 0; index < profiles.size(); ++index) {
    const auto& profile = profiles[index];
    if (!profile.configured()) {
      continue;
    }
    if (std::find(availableSsids.begin(), availableSsids.end(), profile.ssid) == availableSsids.end()) {
      continue;
    }
    if (!bestIndex.has_value() || compareWifiProfiles(profile, profiles[bestIndex.value()])) {
      bestIndex = index;
    }
  }
  return bestIndex;
}

std::vector<std::size_t> rankWifiProfiles(const std::vector<WifiProfile>& profiles) {
  std::vector<std::size_t> ranked;
  ranked.reserve(profiles.size());

  for (std::size_t index = 0; index < profiles.size(); ++index) {
    if (profiles[index].configured()) {
      ranked.push_back(index);
    }
  }

  std::sort(ranked.begin(), ranked.end(), [&](std::size_t left, std::size_t right) {
    return compareWifiProfiles(profiles[left], profiles[right]);
  });
  return ranked;
}

void markWifiProfileSuccess(std::vector<WifiProfile>& profiles, const std::string& ssid, uint64_t connectedAt) {
  for (auto& profile : profiles) {
    if (profile.ssid == ssid) {
      profile.lastSuccessAt = connectedAt;
      profile.disabled = false;
      return;
    }
  }
}

void upsertWifiProfile(std::vector<WifiProfile>& profiles, const WifiProfile& profile, std::size_t limit) {
  if (profile.ssid.empty()) {
    return;
  }

  auto existing = std::find_if(
      profiles.begin(), profiles.end(), [&](const WifiProfile& item) { return item.ssid == profile.ssid; });
  if (existing != profiles.end()) {
    *existing = profile;
  } else {
    profiles.push_back(profile);
  }

  std::sort(profiles.begin(), profiles.end(), compareWifiProfiles);
  if (profiles.size() > limit) {
    profiles.resize(limit);
  }
}

std::string buildShanghaiLocalDate(uint64_t epochMs) {
  std::tm tmValue = toShanghaiTm(epochMs);
  char buffer[32] = {};
  std::snprintf(
      buffer,
      sizeof(buffer),
      "%04d-%02d-%02d",
      tmValue.tm_year + 1900,
      tmValue.tm_mon + 1,
      tmValue.tm_mday);
  return buffer;
}

std::string buildFailureLogId(uint64_t occurredAt, std::size_t ordinal) {
  std::ostringstream oss;
  oss << "log-" << occurredAt << "-" << ordinal;
  return oss.str();
}

}  // namespace core
