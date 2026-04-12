#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/models.hpp"

namespace core {

SnapshotBundle applySyncSnapshot(
    const SnapshotBundle& current, const SyncPayload& payload, uint64_t syncedAt);

std::optional<AttendanceRecordType> classifyAttendanceType(
    const AttendanceConfigSnapshot& config, uint64_t recognizedAt);

QueueMutationResult enqueueAttendanceRecord(
    std::vector<PendingAttendanceRecord>& queue, const PendingAttendanceRecord& candidate);

void upsertPendingEnrollmentReport(
    std::vector<PendingEnrollmentReport>& queue, const PendingEnrollmentReport& candidate);

bool removeEnrollmentTask(
    std::vector<EnrollmentTaskSnapshot>& tasks, const std::string& taskId);

UploadApplicationResult applyUploadResults(
    const std::vector<PendingAttendanceRecord>& queue,
    const std::vector<FailureLogEntry>& logs,
    const std::vector<AttendanceUploadItemResult>& results,
    uint64_t occurredAt);

void pruneFailureLogs(std::vector<FailureLogEntry>& logs, std::size_t limit = 200);

void appendFailureLog(
    std::vector<FailureLogEntry>& logs, const FailureLogEntry& entry, std::size_t limit = 200);

std::string attendanceTypeToApiValue(AttendanceRecordType type);
std::optional<AttendanceRecordType> attendanceTypeFromApiValue(const std::string& value);

std::string attendanceUploadStatusToApiValue(AttendanceUploadStatus status);
std::optional<AttendanceUploadStatus> attendanceUploadStatusFromApiValue(const std::string& value);

std::optional<std::size_t> chooseWifiProfile(
    const std::vector<WifiProfile>& profiles, const std::vector<std::string>& availableSsids);
std::vector<std::size_t> rankWifiProfiles(const std::vector<WifiProfile>& profiles);
void markWifiProfileSuccess(std::vector<WifiProfile>& profiles, const std::string& ssid, uint64_t connectedAt);
void upsertWifiProfile(std::vector<WifiProfile>& profiles, const WifiProfile& profile, std::size_t limit = 5);

std::string buildShanghaiLocalDate(uint64_t epochMs);
std::string buildFailureLogId(uint64_t occurredAt, std::size_t ordinal);

}  // namespace core
