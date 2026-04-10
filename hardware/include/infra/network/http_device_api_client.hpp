#pragma once

#include <string>

#include "infra/device_api_client.hpp"

namespace infra {

class HttpDeviceApiClient final : public DeviceApiClient {
 public:
  explicit HttpDeviceApiClient(std::string baseUrl);

  std::unique_ptr<DeviceApiClient> cloneWithBaseUrl(const std::string& baseUrl) const override;
  ApiResult<ServerProbeResponse> probeServer() override;
  ApiResult<BootstrapActivationResponse> bootstrapHello(const BootstrapHelloRequest& request) override;
  ApiResult<core::SyncPayload> sync(const core::DeviceCredentials& credentials) override;
  ApiResult<EnrollmentReportResponse> reportEnrollment(
      const core::DeviceCredentials& credentials, const EnrollmentReportRequest& request) override;
  ApiResult<AttendanceUploadResponse> uploadAttendance(
      const core::DeviceCredentials& credentials,
      const std::vector<core::PendingAttendanceRecord>& records) override;

 private:
  std::string endpointUrl(const char* path) const;

  std::string baseUrl_;
};

}  // namespace infra
