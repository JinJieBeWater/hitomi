#pragma once

namespace infra {
class DisplayPort;
class LocalStore;
class TemplateStorePort;
}  // namespace infra

namespace face {
class CameraPort;
class EnrollmentServicePort;
class RecognitionServicePort;
}  // namespace face

namespace app {

class RuntimeNetworkExecutor;

struct RuntimeContext {
  infra::DisplayPort& display;
  infra::LocalStore& localStore;
  RuntimeNetworkExecutor& networkExecutor;
  infra::TemplateStorePort& templateStore;
  face::CameraPort& camera;
  face::EnrollmentServicePort& enrollmentService;
  face::RecognitionServicePort& recognitionService;
};

}  // namespace app
