#include "app/runtime_network_executor.hpp"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <deque>
#include <memory>
#include <new>
#include <utility>

#include "infra/device_api_client.hpp"

namespace app {
namespace {

constexpr uint32_t kNetworkRequestTaskStackSize = 8 * 1024;
constexpr UBaseType_t kNetworkRequestTaskPriority = 1;
constexpr BaseType_t kNetworkRequestTaskCore = 0;
constexpr std::size_t kPendingRequestQueueCapacity = 4;

class FreeRtosRuntimeNetworkExecutor final : public RuntimeNetworkExecutor {
 public:
  explicit FreeRtosRuntimeNetworkExecutor(infra::DeviceApiClient& deviceApiClient)
      : impl_(std::make_unique<Impl>(deviceApiClient)) {}

  ~FreeRtosRuntimeNetworkExecutor() override {
    if (impl_ == nullptr || impl_->mutex == nullptr) {
      return;
    }
    vSemaphoreDelete(impl_->mutex);
  }

  bool start() override {
    if (impl_->started) {
      return true;
    }

    impl_->mutex = xSemaphoreCreateMutex();
    if (impl_->mutex == nullptr) {
      Serial.println("[APP] failed to create network executor mutex");
      return false;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        &FreeRtosRuntimeNetworkExecutor::taskEntry,
        "hitomi_request",
        kNetworkRequestTaskStackSize,
        this,
        kNetworkRequestTaskPriority,
        &impl_->workerHandle,
        kNetworkRequestTaskCore);
    if (created != pdPASS) {
      Serial.printf(
          "[APP] failed to create network executor worker stack=%lu freeHeap=%lu minFreeHeap=%lu\n",
          static_cast<unsigned long>(kNetworkRequestTaskStackSize),
          static_cast<unsigned long>(ESP.getFreeHeap()),
          static_cast<unsigned long>(ESP.getMinFreeHeap()));
      vSemaphoreDelete(impl_->mutex);
      impl_->mutex = nullptr;
      return false;
    }

    impl_->started = true;
    return true;
  }

  bool submit(PendingNetworkRequest request) override {
    if (!impl_->started || impl_->mutex == nullptr) {
      return false;
    }

    if (xSemaphoreTake(impl_->mutex, portMAX_DELAY) != pdTRUE) {
      return false;
    }

    const bool hasCapacity = impl_->pendingRequests.size() < kPendingRequestQueueCapacity;
    if (hasCapacity) {
      impl_->pendingRequests.push_back(std::move(request));
    }
    xSemaphoreGive(impl_->mutex);

    if (!hasCapacity) {
      Serial.println("[APP] network request queue full");
      return false;
    }

    xTaskNotifyGive(impl_->workerHandle);

    return true;
  }

  std::optional<CompletedNetworkRequest> consumeCompleted() override {
    if (!impl_->started || impl_->mutex == nullptr) {
      return std::nullopt;
    }

    if (xSemaphoreTake(impl_->mutex, portMAX_DELAY) != pdTRUE) {
      return std::nullopt;
    }

    std::optional<CompletedNetworkRequest> completed;
    if (!impl_->completedRequests.empty()) {
      completed = std::move(impl_->completedRequests.front());
      impl_->completedRequests.pop_front();
    }
    xSemaphoreGive(impl_->mutex);
    return completed;
  }

 private:
  struct Impl {
    explicit Impl(infra::DeviceApiClient& client) : deviceApiClient(client) {}

    infra::DeviceApiClient& deviceApiClient;
    SemaphoreHandle_t mutex = nullptr;
    TaskHandle_t workerHandle = nullptr;
    std::deque<PendingNetworkRequest> pendingRequests;
    std::deque<CompletedNetworkRequest> completedRequests;
    bool started = false;
  };

  static void taskEntry(void* userData) {
    auto* executor = static_cast<FreeRtosRuntimeNetworkExecutor*>(userData);
    if (executor == nullptr) {
      vTaskDelete(nullptr);
      return;
    }

    while (true) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

      while (true) {
        std::optional<PendingNetworkRequest> request;
        if (xSemaphoreTake(executor->impl_->mutex, portMAX_DELAY) == pdTRUE) {
          if (!executor->impl_->pendingRequests.empty()) {
            request = std::move(executor->impl_->pendingRequests.front());
            executor->impl_->pendingRequests.pop_front();
          }
          xSemaphoreGive(executor->impl_->mutex);
        }

        if (!request.has_value()) {
          break;
        }

        std::unique_ptr<infra::DeviceApiClient> requestClient =
            executor->impl_->deviceApiClient.cloneWithBaseUrl(request->baseUrl);
        if (requestClient == nullptr) {
          Serial.println("[APP] failed to clone request-scoped device API client");
          continue;
        }

        CompletedNetworkRequest completed = executor->execute(*requestClient, std::move(request.value()));
        if (xSemaphoreTake(executor->impl_->mutex, portMAX_DELAY) == pdTRUE) {
          executor->impl_->completedRequests.push_back(std::move(completed));
          xSemaphoreGive(executor->impl_->mutex);
        }
      }
    }
  }

  CompletedNetworkRequest execute(infra::DeviceApiClient& deviceApiClient, PendingNetworkRequest request) {
    CompletedNetworkRequest completed = {};
    completed.type = request.type;
    completed.generation = request.generation;
    completed.requestedAtMs = request.requestedAtMs;
    completed.enrollmentReportRequest = request.enrollmentReportRequest;
    completed.uploadBatch = std::move(request.uploadBatch);

    switch (request.type) {
      case NetworkRequestType::ApiProbe:
        completed.probeResult = deviceApiClient.probeServer();
        return completed;
      case NetworkRequestType::Activation:
        completed.activationResult = deviceApiClient.bootstrapHello(request.activationRequest);
        return completed;
      case NetworkRequestType::EnrollmentReport:
        completed.enrollmentReportResult =
            deviceApiClient.reportEnrollment(request.credentials, request.enrollmentReportRequest);
        return completed;
      case NetworkRequestType::Sync:
        completed.syncResult = deviceApiClient.sync(request.credentials);
        return completed;
      case NetworkRequestType::Upload:
        completed.uploadResult = deviceApiClient.uploadAttendance(request.credentials, completed.uploadBatch);
        return completed;
      case NetworkRequestType::None:
      default:
        return completed;
    }
  }

  std::unique_ptr<Impl> impl_;
};

}  // namespace

std::unique_ptr<RuntimeNetworkExecutor> createRuntimeNetworkExecutor(infra::DeviceApiClient& deviceApiClient) {
  return std::make_unique<FreeRtosRuntimeNetworkExecutor>(deviceApiClient);
}

}  // namespace app
