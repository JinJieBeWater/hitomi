#pragma once

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

struct RuntimeContext {
  infra::DisplayPort& display;
  infra::LocalStore& localStore;
  infra::DeviceApiClient& deviceApiClient;
  infra::TemplateStorePort& templateStore;
  face::CameraPort& camera;
  face::EnrollmentServicePort& enrollmentService;
  face::RecognitionServicePort& recognitionService;
};

}  // namespace app
