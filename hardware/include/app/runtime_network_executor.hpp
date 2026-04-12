#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/models.hpp"
#include "infra/device_api_client.hpp"

namespace app {

enum class NetworkRequestType : uint8_t {
  None = 0,
  ApiProbe,
  Activation,
  EnrollmentReport,
  Sync,
  Upload,
};

struct PendingNetworkRequest {
  NetworkRequestType type = NetworkRequestType::None;
  uint32_t generation = 0;
  uint32_t requestedAtMs = 0;
  std::string baseUrl;
  core::DeviceCredentials credentials;
  infra::BootstrapHelloRequest activationRequest;
  infra::EnrollmentReportRequest enrollmentReportRequest;
  std::vector<core::PendingAttendanceRecord> uploadBatch;
};

struct CompletedNetworkRequest {
  NetworkRequestType type = NetworkRequestType::None;
  uint32_t generation = 0;
  uint32_t requestedAtMs = 0;
  infra::EnrollmentReportRequest enrollmentReportRequest;
  std::vector<core::PendingAttendanceRecord> uploadBatch;
  infra::ApiResult<infra::ServerProbeResponse> probeResult;
  infra::ApiResult<infra::BootstrapActivationResponse> activationResult;
  infra::ApiResult<infra::EnrollmentReportResponse> enrollmentReportResult;
  infra::ApiResult<core::SyncPayload> syncResult;
  infra::ApiResult<infra::AttendanceUploadResponse> uploadResult;
};

class RuntimeNetworkExecutor {
 public:
  virtual ~RuntimeNetworkExecutor() = default;

  virtual bool start() = 0;
  virtual bool submit(PendingNetworkRequest request) = 0;
  virtual std::optional<CompletedNetworkRequest> consumeCompleted() = 0;
};

std::unique_ptr<RuntimeNetworkExecutor> createRuntimeNetworkExecutor(infra::DeviceApiClient& deviceApiClient);

}  // namespace app
