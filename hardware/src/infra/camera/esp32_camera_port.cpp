#include "infra/camera/esp32_camera_port.hpp"

#include <Arduino.h>

#include <esp_camera.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <sensor.h>

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "board/app_config.hpp"
#include "board/pins.hpp"
#include "infra/board/board_control_bus.hpp"

namespace infra {
namespace {

constexpr int kI2cTimeoutMs = 1000;
constexpr UBaseType_t kCameraTaskPriority = 2;
constexpr uint32_t kCameraTaskStackSize = 6 * 1024;
constexpr BaseType_t kCameraTaskCore = 0;

bool setIoExpanderOutputBit(uint8_t gpioBit, bool level) {
  return infra::updateBoardControlRegisterBit(
      board::kIoExpanderAddress,
      board::kIoExpanderOutputReg,
      gpioBit,
      level,
      kI2cTimeoutMs);
}

face::CameraPixelFormat toCameraPixelFormat(pixformat_t format) {
  switch (format) {
    case PIXFORMAT_RGB565:
      return face::CameraPixelFormat::RGB565;
    case PIXFORMAT_JPEG:
      return face::CameraPixelFormat::JPEG;
    case PIXFORMAT_GRAYSCALE:
      return face::CameraPixelFormat::Grayscale;
    case PIXFORMAT_YUV422:
      return face::CameraPixelFormat::YUV422;
    default:
      return face::CameraPixelFormat::Unknown;
  }
}

std::string sensorModelName(int pid) {
#ifdef GC0308_PID
  if (pid == GC0308_PID) {
    return "GC0308";
  }
#endif
#ifdef GC032A_PID
  if (pid == GC032A_PID) {
    return "GC032A";
  }
#endif
#ifdef OV2640_PID
  if (pid == OV2640_PID) {
    return "OV2640";
  }
#endif

  return pid == 0 ? "Unknown" : "PID " + std::to_string(pid);
}

camera_config_t buildCameraConfig() {
  camera_config_t config = {};
  config.ledc_channel = board::kCameraXclkChannel;
  config.ledc_timer = board::kCameraXclkTimer;
  config.pin_d0 = board::kCameraData0Pin;
  config.pin_d1 = board::kCameraData1Pin;
  config.pin_d2 = board::kCameraData2Pin;
  config.pin_d3 = board::kCameraData3Pin;
  config.pin_d4 = board::kCameraData4Pin;
  config.pin_d5 = board::kCameraData5Pin;
  config.pin_d6 = board::kCameraData6Pin;
  config.pin_d7 = board::kCameraData7Pin;
  config.pin_xclk = board::kCameraXclkPin;
  config.pin_pclk = board::kCameraPclkPin;
  config.pin_vsync = board::kCameraVsyncPin;
  config.pin_href = board::kCameraHrefPin;
  config.pin_sccb_sda = -1;
  config.pin_sccb_scl = board::kCameraSccbSclPin;
  config.pin_pwdn = -1;
  config.pin_reset = -1;
  config.xclk_freq_hz = board::kCameraXclkHz;
  config.sccb_i2c_port = board::kI2cPort;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  // One extra buffer absorbs short stalls from RGB565 face detection and Wi-Fi activity.
  config.fb_count = board::kCameraFrameBufferCount;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;
  return config;
}

class Esp32CameraFrameLease final : public face::CameraFrameLease {
 public:
  Esp32CameraFrameLease(camera_fb_t* frameBuffer, face::CameraFrameInfo frameInfo)
      : frameBuffer_(frameBuffer), frameInfo_(std::move(frameInfo)) {}

  ~Esp32CameraFrameLease() override {
    if (frameBuffer_ != nullptr) {
      esp_camera_fb_return(frameBuffer_);
      frameBuffer_ = nullptr;
    }
  }

  const face::CameraFrameInfo& info() const override {
    return frameInfo_;
  }

  const uint8_t* data() const override {
    return frameBuffer_ == nullptr ? nullptr : frameBuffer_->buf;
  }

 private:
  camera_fb_t* frameBuffer_ = nullptr;
  face::CameraFrameInfo frameInfo_ = {};
};

}  // namespace

struct Esp32CameraPort::Impl {
  Impl() {
    status.supported = true;
    statusMutex = xSemaphoreCreateMutex();
  }

  ~Impl() {
    if (statusMutex != nullptr) {
      vSemaphoreDelete(statusMutex);
      statusMutex = nullptr;
    }
  }

  template <typename Fn>
  void withStatusLock(Fn&& updater) {
    if (statusMutex == nullptr) {
      std::forward<Fn>(updater)(status);
      return;
    }

    if (xSemaphoreTake(statusMutex, portMAX_DELAY) == pdTRUE) {
      std::forward<Fn>(updater)(status);
      xSemaphoreGive(statusMutex);
    }
  }

  face::CameraStatus copyStatus() const {
    if (statusMutex == nullptr) {
      return status;
    }

    face::CameraStatus snapshot = {};
    if (xSemaphoreTake(statusMutex, portMAX_DELAY) == pdTRUE) {
      snapshot = status;
      xSemaphoreGive(statusMutex);
    }
    return snapshot;
  }

  static void cameraCaptureTask(void* userData) {
    auto* impl = static_cast<Impl*>(userData);
    if (impl == nullptr) {
      vTaskDelete(nullptr);
      return;
    }

    while (!impl->stopTask.load(std::memory_order_acquire)) {
      camera_fb_t* frameBuffer = esp_camera_fb_get();
      if (frameBuffer == nullptr) {
        impl->withStatusLock([](face::CameraStatus& status) {
          status.failedCaptureCount += 1;
          status.lastError = "esp_camera_fb_get returned null";
        });
        vTaskDelay(pdMS_TO_TICKS(10));
        continue;
      }

      camera_fb_t* staleFrame = nullptr;
      if (xQueueReceive(impl->previewQueue, &staleFrame, 0) == pdTRUE && staleFrame != nullptr) {
        esp_camera_fb_return(staleFrame);
      }
      xQueueOverwrite(impl->previewQueue, &frameBuffer);
    }

    camera_fb_t* pendingFrame = nullptr;
    while (xQueueReceive(impl->previewQueue, &pendingFrame, 0) == pdTRUE) {
      if (pendingFrame != nullptr) {
        esp_camera_fb_return(pendingFrame);
      }
    }

    vTaskDelete(nullptr);
  }

  bool initialized = false;
  std::atomic<bool> stopTask{false};
  TaskHandle_t taskHandle = nullptr;
  QueueHandle_t previewQueue = nullptr;
  SemaphoreHandle_t statusMutex = nullptr;
  face::CameraStatus status = {};
};

Esp32CameraPort::Esp32CameraPort() : impl_(std::make_unique<Impl>()) {}

Esp32CameraPort::~Esp32CameraPort() {
  if (impl_ == nullptr || !impl_->initialized) {
    return;
  }

  impl_->stopTask.store(true, std::memory_order_release);
  if (impl_->taskHandle != nullptr) {
    while (eTaskGetState(impl_->taskHandle) != eDeleted) {
      vTaskDelay(pdMS_TO_TICKS(5));
    }
    impl_->taskHandle = nullptr;
  }
  if (impl_->previewQueue != nullptr) {
    vQueueDelete(impl_->previewQueue);
    impl_->previewQueue = nullptr;
  }

  esp_camera_deinit();
  setIoExpanderOutputBit(board::kIoExpanderCameraPwdnBit, true);
}

Esp32CameraPort::Esp32CameraPort(Esp32CameraPort&&) noexcept = default;

Esp32CameraPort& Esp32CameraPort::operator=(Esp32CameraPort&&) noexcept = default;

bool Esp32CameraPort::init() {
  if (impl_->initialized) {
    return true;
  }

  impl_->withStatusLock([](face::CameraStatus& status) {
    status.lastError.clear();
  });
  if (!infra::ensureBoardIoExpanderReady()) {
    impl_->withStatusLock([](face::CameraStatus& status) {
      status.lastError = "board control bus init failed";
    });
    return false;
  }
  if (!setIoExpanderOutputBit(board::kIoExpanderCameraPwdnBit, false)) {
    impl_->withStatusLock([](face::CameraStatus& status) {
      status.lastError = "camera power enable failed";
    });
    return false;
  }

  delay(board::kCameraPowerStabilizeDelayMs);

  camera_config_t config = buildCameraConfig();
  const esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    const std::string message = std::string("esp_camera_init failed: ") + esp_err_to_name(err);
    impl_->withStatusLock([&](face::CameraStatus& status) {
      status.lastError = message;
    });
    setIoExpanderOutputBit(board::kIoExpanderCameraPwdnBit, true);
    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    const std::string modelName = sensorModelName(sensor->id.PID);
    impl_->withStatusLock([&](face::CameraStatus& status) {
      status.sensorModel = modelName;
    });
#ifdef GC0308_PID
    if (sensor->id.PID == GC0308_PID && sensor->set_hmirror != nullptr) {
      sensor->set_hmirror(sensor, 1);
    }
#endif
  } else {
    impl_->withStatusLock([](face::CameraStatus& status) {
      status.sensorModel = "Unknown";
    });
  }

  impl_->previewQueue = xQueueCreate(1, sizeof(camera_fb_t*));
  if (impl_->previewQueue == nullptr) {
    impl_->withStatusLock([](face::CameraStatus& status) {
      status.lastError = "preview queue create failed";
    });
    esp_camera_deinit();
    setIoExpanderOutputBit(board::kIoExpanderCameraPwdnBit, true);
    return false;
  }

  impl_->stopTask.store(false, std::memory_order_release);
  if (xTaskCreatePinnedToCore(
          Impl::cameraCaptureTask,
          "hitomi_camera",
          kCameraTaskStackSize,
          impl_.get(),
          kCameraTaskPriority,
          &impl_->taskHandle,
          kCameraTaskCore) != pdPASS) {
    impl_->withStatusLock([](face::CameraStatus& status) {
      status.lastError = "camera capture task create failed";
    });
    vQueueDelete(impl_->previewQueue);
    impl_->previewQueue = nullptr;
    esp_camera_deinit();
    setIoExpanderOutputBit(board::kIoExpanderCameraPwdnBit, true);
    return false;
  }

  impl_->initialized = true;
  impl_->withStatusLock([](face::CameraStatus& status) {
    status.initialized = true;
    status.lastError.clear();
  });
  return true;
}

bool Esp32CameraPort::available() const {
  return impl_->status.supported;
}

bool Esp32CameraPort::ready() const {
  return impl_->initialized;
}

face::CameraStatus Esp32CameraPort::status() const {
  return impl_->copyStatus();
}

std::unique_ptr<face::CameraFrameLease> Esp32CameraPort::capture() {
  if (!impl_->initialized || impl_->previewQueue == nullptr) {
    return nullptr;
  }

  camera_fb_t* frameBuffer = nullptr;
  if (xQueueReceive(impl_->previewQueue, &frameBuffer, 0) != pdTRUE || frameBuffer == nullptr) {
    return nullptr;
  }

  face::CameraFrameInfo frameInfo = {
      .width = static_cast<uint16_t>(frameBuffer->width),
      .height = static_cast<uint16_t>(frameBuffer->height),
      .bytes = frameBuffer->len,
      .pixelFormat = toCameraPixelFormat(frameBuffer->format),
      .capturedAtMs = static_cast<uint64_t>(esp_timer_get_time() / 1000ULL),
  };

  impl_->withStatusLock([&](face::CameraStatus& status) {
    status.captureCount += 1;
    status.lastFrame = frameInfo;
    status.lastError.clear();
  });

  return std::make_unique<Esp32CameraFrameLease>(frameBuffer, frameInfo);
}

}  // namespace infra
