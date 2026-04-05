#pragma once

#include <cstdint>
#include <memory>

namespace infra {
class DisplayPort;
class LocalStore;
class DeviceApiClient;
class TemplateStorePort;
}  // namespace infra

namespace face {
class CameraPort;
class EnrollmentServicePort;
class RecognitionServicePort;
}  // namespace face

namespace app {

struct RuntimeContext;
struct RuntimeState;

class AppRuntime {
 public:
  AppRuntime(
      infra::DisplayPort& display,
      infra::LocalStore& localStore,
      infra::DeviceApiClient& deviceApiClient,
      infra::TemplateStorePort& templateStore,
      face::CameraPort& camera,
      face::EnrollmentServicePort& enrollmentService,
      face::RecognitionServicePort& recognitionService);
  ~AppRuntime();

  AppRuntime(const AppRuntime&) = delete;
  AppRuntime& operator=(const AppRuntime&) = delete;

  void setup();
  void tick(uint32_t nowMs);

 private:
  std::unique_ptr<RuntimeContext> context_;
  std::unique_ptr<RuntimeState> state_;
};

}  // namespace app
