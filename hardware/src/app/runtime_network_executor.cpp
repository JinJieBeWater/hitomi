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

constexpr uint32_t kNetworkRequestTaskStackSize = 16 * 1024;
constexpr UBaseType_t kNetworkRequestTaskPriority = 1;
constexpr BaseType_t kNetworkRequestTaskCore = 0;
constexpr std::size_t kMaxConcurrentNetworkRequests = 2;

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

    impl_->started = true;
    return true;
  }

  bool submit(PendingNetworkRequest request) override {
    if (!impl_->started || impl_->mutex == nullptr) {
      return false;
    }

    std::unique_ptr<infra::DeviceApiClient> requestClient =
        impl_->deviceApiClient.cloneWithBaseUrl(request.baseUrl);
    if (requestClient == nullptr) {
      Serial.println("[APP] failed to clone request-scoped device API client");
      return false;
    }

    if (xSemaphoreTake(impl_->mutex, portMAX_DELAY) != pdTRUE) {
      return false;
    }

    const bool hasCapacity = impl_->activeRequestCount < kMaxConcurrentNetworkRequests;
    if (hasCapacity) {
      impl_->activeRequestCount += 1;
    }
    xSemaphoreGive(impl_->mutex);

    if (!hasCapacity) {
      return false;
    }

    auto* taskContext = new (std::nothrow) RequestTaskContext{
        .executor = this,
        .request = std::move(request),
        .requestClient = std::move(requestClient),
    };
    if (taskContext == nullptr) {
      releaseActiveRequestSlot();
      Serial.println("[APP] failed to allocate network request task context");
      return false;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        &FreeRtosRuntimeNetworkExecutor::taskEntry,
        "hitomi_request",
        kNetworkRequestTaskStackSize,
        taskContext,
        kNetworkRequestTaskPriority,
        nullptr,
        kNetworkRequestTaskCore);
    if (created != pdPASS) {
      delete taskContext;
      releaseActiveRequestSlot();
      Serial.println("[APP] failed to create network request task");
      return false;
    }

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
  struct RequestTaskContext {
    FreeRtosRuntimeNetworkExecutor* executor = nullptr;
    PendingNetworkRequest request = {};
    std::unique_ptr<infra::DeviceApiClient> requestClient;
  };

  struct Impl {
    explicit Impl(infra::DeviceApiClient& client) : deviceApiClient(client) {}

    infra::DeviceApiClient& deviceApiClient;
    SemaphoreHandle_t mutex = nullptr;
    std::deque<CompletedNetworkRequest> completedRequests;
    std::size_t activeRequestCount = 0;
    bool started = false;
  };

  static void taskEntry(void* userData) {
    std::unique_ptr<RequestTaskContext> context(static_cast<RequestTaskContext*>(userData));
    if (context == nullptr || context->executor == nullptr || context->requestClient == nullptr) {
      vTaskDelete(nullptr);
      return;
    }

    context->executor->runRequestTask(*context);
    vTaskDelete(nullptr);
  }

  void runRequestTask(RequestTaskContext& context) {
    CompletedNetworkRequest completed = execute(*context.requestClient, std::move(context.request));

    if (xSemaphoreTake(impl_->mutex, portMAX_DELAY) == pdTRUE) {
      impl_->completedRequests.push_back(std::move(completed));
      if (impl_->activeRequestCount > 0) {
        impl_->activeRequestCount -= 1;
      }
      xSemaphoreGive(impl_->mutex);
      return;
    }

    releaseActiveRequestSlot();
  }

  void releaseActiveRequestSlot() {
    if (impl_->mutex == nullptr) {
      return;
    }

    if (xSemaphoreTake(impl_->mutex, portMAX_DELAY) != pdTRUE) {
      return;
    }
    if (impl_->activeRequestCount > 0) {
      impl_->activeRequestCount -= 1;
    }
    xSemaphoreGive(impl_->mutex);
  }

  CompletedNetworkRequest execute(infra::DeviceApiClient& deviceApiClient, PendingNetworkRequest request) {
    CompletedNetworkRequest completed = {};
    completed.type = request.type;
    completed.generation = request.generation;
    completed.requestedAtMs = request.requestedAtMs;
    completed.uploadBatch = std::move(request.uploadBatch);

    switch (request.type) {
      case NetworkRequestType::ApiProbe:
        completed.probeResult = deviceApiClient.probeServer();
        return completed;
      case NetworkRequestType::Activation:
        completed.activationResult = deviceApiClient.bootstrapHello(request.activationRequest);
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
