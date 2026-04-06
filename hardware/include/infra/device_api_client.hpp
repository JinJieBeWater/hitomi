#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/models.hpp"

namespace infra {

template <typename T>
struct ApiResult {
  bool success = false;
  std::optional<T> data;
  std::optional<core::ApiError> error;
};

struct EnrollmentReportRequest {
  std::string taskId;
  std::string employeeId;
  std::string result;
  uint64_t finishedAt = 0;
  std::optional<std::string> failureReason;
};

struct EnrollmentReportResponse {
  std::string taskId;
  std::string employeeId;
  std::string status;
  bool applied = false;
};

struct AttendanceUploadResponse {
  std::vector<core::AttendanceUploadItemResult> results;
  int saved = 0;
  int updatedEarlier = 0;
  int ignoredDuplicate = 0;
  int rejected = 0;
};

struct ServerProbeResponse {
  std::string service;
  uint64_t now = 0;
};

struct BootstrapHelloRequest {
  std::string deviceSerial;
  std::string bootstrapSecret;
  std::string firmwareTag;
  std::optional<std::string> wifiSsid;
};

struct BootstrapActivationResponse {
  std::string registrationId;
  std::string state;
  std::optional<std::string> deviceCode;
  std::optional<std::string> apiKey;
};

class DeviceApiClient {
 public:
  virtual ~DeviceApiClient() = default;

  virtual bool configured() const = 0;
  virtual void setBaseUrl(const std::string& baseUrl) = 0;
  virtual ApiResult<ServerProbeResponse> probeServer() = 0;
  virtual ApiResult<BootstrapActivationResponse> bootstrapHello(const BootstrapHelloRequest& request) = 0;
  virtual ApiResult<core::SyncPayload> sync(const core::DeviceCredentials& credentials) = 0;
  virtual ApiResult<EnrollmentReportResponse> reportEnrollment(
      const core::DeviceCredentials& credentials, const EnrollmentReportRequest& request) = 0;
  virtual ApiResult<AttendanceUploadResponse> uploadAttendance(
      const core::DeviceCredentials& credentials,
      const std::vector<core::PendingAttendanceRecord>& records) = 0;
};

}  // namespace infra
