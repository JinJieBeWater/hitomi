#include "ui/status_screen_presenter.hpp"

#include <ctime>
#include <sstream>

#include "app/runtime_face_status.hpp"
#include "core/use_cases.hpp"
#include "infra/template_store_port.hpp"

namespace ui {
namespace {

std::string connectivityLabel(app::ConnectivityState state) {
  switch (state) {
    case app::ConnectivityState::Connected:
      return "Connected";
    case app::ConnectivityState::Disconnected:
      return "Disconnected";
    case app::ConnectivityState::Unknown:
    default:
      return "Unknown";
  }
}

std::string activationLabel(const app::RuntimeStatus& status) {
  if (status.activationInFlight) {
    return "Activation: pending";
  }

  switch (status.activationState) {
    case core::DeviceActivationState::Activated:
      return "Activation: activated";
    case core::DeviceActivationState::PendingActivation: {
      if (!status.activationRegistrationId.has_value()) {
        return "Activation: waiting for claim";
      }

      std::string code = status.activationRegistrationId.value();
      if (code.size() > 12) {
        code = code.substr(0, 12);
      }
      return "Activation: waiting for claim (" + code + ")";
    }
    case core::DeviceActivationState::Unconfigured:
    default:
      return "Activation: setup required";
  }
}

std::string syncLabel(const app::RuntimeStatus& status) {
  if (!status.credentials.configured()) {
    return "Inactive";
  }
  if (status.syncInFlight) {
    return "Syncing";
  }
  if (status.snapshots.lastSyncAt == 0) {
    return "Waiting";
  }

  std::ostringstream oss;
  oss << "Ready @" << status.snapshots.lastSyncAt;
  return oss.str();
}

std::string activeEnrollmentSubject(const app::RuntimeStatus& status) {
  if (status.activeEnrollmentTaskId.has_value()) {
    for (const auto& task : status.snapshots.enrollmentTasks) {
      if (task.taskId != status.activeEnrollmentTaskId.value()) {
        continue;
      }
      if (!task.employeeCode.empty()) {
        return "Employee " + task.employeeCode;
      }
      if (!task.employeeId.empty()) {
        return "Employee " + task.employeeId;
      }
      break;
    }
  }

  return status.activeEnrollmentEmployeeName.value_or(
      status.activeEnrollmentTaskId.value_or("pending"));
}

std::string enrollmentProgressSuffix(const app::RuntimeStatus& status) {
  if (status.enrollmentRequiredSamples == 0) {
    return "";
  }

  std::ostringstream oss;
  oss << " (" << status.enrollmentCapturedSamples << "/" << status.enrollmentRequiredSamples << ")";
  return oss.str();
}

std::string subtitleLabel(const app::RuntimeStatus& status) {
  if (status.credentials.configured()) {
    return status.credentials.deviceCode;
  }
  return "SZPI ESP32-S3";
}

std::string credentialsLabel(const app::RuntimeStatus& status) {
  if (status.credentials.configured()) {
    return status.credentials.deviceCode;
  }
  if (status.bootstrapConfigured) {
    return "Bootstrap only";
  }
  return "Missing";
}

std::string friendlyEnrollmentDetail(const app::RuntimeStatus& status) {
  const std::string detail = status.enrollmentStatusDetail.value_or("");

  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
      if (detail == "No face detected") {
        return "Move closer. One face.";
      }
      if (detail == "Multiple faces detected") {
        return "Only one face.";
      }
      if (detail == "Hold still for enrollment") {
        if (status.enrollmentRequiredSamples > 0 && status.enrollmentCapturedSamples < status.enrollmentRequiredSamples) {
          return "Sample " + std::to_string(status.enrollmentCapturedSamples) + "/" +
              std::to_string(status.enrollmentRequiredSamples) + ". Hold still.";
        }
        return "Hold still.";
      }
      if (detail == "Waiting for single face" || detail == "Look at the camera") {
        return "Look at camera.";
      }
      if (!detail.empty()) {
        return detail;
      }
      return "Center one face.";
    case app::EnrollmentRunState::SavingTemplate:
      return detail.empty() ? "Saving to SD..." : detail;
    case app::EnrollmentRunState::Reporting:
      return detail.empty() ? "Reporting..." : detail;
    case app::EnrollmentRunState::Done:
      return detail.empty() ? "Done." : detail;
    case app::EnrollmentRunState::Failed:
      if (status.enrollmentFailureReason.has_value()) {
        return "Failed: " + status.enrollmentFailureReason.value();
      }
      return detail.empty() ? "Failed." : detail;
    case app::EnrollmentRunState::Cancelled:
      return detail.empty() ? "Cancelled." : detail;
    case app::EnrollmentRunState::Idle:
    default:
      return "";
  }
}

bool capturePageActive(const app::RuntimeStatus& status) {
  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
    case app::EnrollmentRunState::SavingTemplate:
    case app::EnrollmentRunState::Reporting:
    case app::EnrollmentRunState::Done:
    case app::EnrollmentRunState::Failed:
    case app::EnrollmentRunState::Cancelled:
      return true;
    case app::EnrollmentRunState::Idle:
    default:
      return false;
  }
}

bool captureRunning(const app::RuntimeStatus& status) {
  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
    case app::EnrollmentRunState::SavingTemplate:
      return true;
    case app::EnrollmentRunState::Reporting:
      return false;
    case app::EnrollmentRunState::Idle:
    case app::EnrollmentRunState::Done:
    case app::EnrollmentRunState::Failed:
    case app::EnrollmentRunState::Cancelled:
    default:
      return false;
  }
}

std::string captureTitle(const app::RuntimeStatus& status) {
  const std::string subject = activeEnrollmentSubject(status);

  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
      return "Capture: " + subject;
    case app::EnrollmentRunState::SavingTemplate:
      return "Saving";
    case app::EnrollmentRunState::Reporting:
      return "Reporting";
    case app::EnrollmentRunState::Done:
      return "Done";
    case app::EnrollmentRunState::Failed:
      return "Failed";
    case app::EnrollmentRunState::Cancelled:
      return "Cancelled";
    default:
      return subject.empty() ? "Enrollment capture" : subject;
  }
}

std::string captureProgress(const app::RuntimeStatus& status) {
  if (status.enrollmentRequiredSamples > 0) {
    return std::to_string(status.enrollmentCapturedSamples) + "/" +
        std::to_string(status.enrollmentRequiredSamples) + "  face " +
        std::to_string(status.detectedFaceCount);
  }

  if (status.detectedFaceCount > 0) {
    return std::to_string(status.detectedFaceCount) + " face";
  }

  return "";
}

std::string captureActionLabel(const app::RuntimeStatus& status) {
  return captureRunning(status) ? "Cancel" : "Back";
}

std::string enrollmentTaskLabel(const core::EnrollmentTaskSnapshot& task) {
  if (!task.employeeCode.empty()) {
    return "Employee " + task.employeeCode;
  }
  if (!task.employeeId.empty()) {
    return "Employee " + task.employeeId;
  }
  return "Employee";
}

std::string taskLabel(const app::RuntimeStatus& status) {
  if (status.enrollmentState == app::EnrollmentRunState::Capturing ||
      status.enrollmentState == app::EnrollmentRunState::Preparing) {
    std::ostringstream oss;
    oss << "Task: enrolling "
        << status.activeEnrollmentEmployeeName.value_or(
               status.activeEnrollmentTaskId.value_or("pending"));
    if (status.enrollmentRequiredSamples > 0) {
      oss << " (" << status.enrollmentCapturedSamples << "/" << status.enrollmentRequiredSamples << ")";
    }
    return oss.str();
  }
  if (status.enrollmentState == app::EnrollmentRunState::Reporting) {
    return "Task: reporting enrollment";
  }
  if (status.enrollmentState == app::EnrollmentRunState::Failed) {
    return "Task: enrollment failed";
  }
  if (status.enrollmentState == app::EnrollmentRunState::Done) {
    return "Task: enrollment complete";
  }
  if (status.enrollmentState == app::EnrollmentRunState::Cancelled) {
    return "Task: enrollment cancelled";
  }

  if (!status.credentials.configured()) {
    return "Task: unavailable";
  }
  if (status.snapshots.enrollmentTasks.empty()) {
    return "Task: none";
  }

  const auto& task = status.snapshots.enrollmentTasks.front();
  const std::size_t remaining = status.snapshots.enrollmentTasks.size() - 1;
  std::ostringstream oss;
  oss << "Task: " << enrollmentTaskLabel(task);
  if (remaining > 0) {
    oss << " +" << remaining << " more";
  }
  return oss.str();
}

std::string storageLabel(const app::RuntimeStatus& status) {
  if (!status.credentialsReady) {
    return "Credentials unavailable";
  }
  if (!status.filesystemReady) {
    return "LittleFS unavailable";
  }
  if (infra::templateStoreDisabled(status.templateStoreStatusCode)) {
    return "SD disabled";
  }
  if (infra::templateStoreManifestBroken(status.templateStoreStatusCode)) {
    return "SD invalid manifest";
  }
  if (status.templateStoreStatusCode == infra::kTemplateStoreIoError) {
    return "SD io_error";
  }
  if (status.templateStoreStatusCode == infra::kTemplateStoreMountFailed) {
    return "SD mount_failed";
  }
  if (status.templateStoreStatusCode == infra::kTemplateStoreCardMissing) {
    return "SD card_missing";
  }
  if (infra::templateStoreMounted(status.templateStoreStatusCode)) {
    std::ostringstream oss;
    oss << "SD ready (templates=" << status.templateCount << ")";
    return oss.str();
  }
  if (!status.templateStoreReady) {
    return status.templateStoreStatusCode.empty() ? "SD not ready" : "SD " + status.templateStoreStatusCode;
  }
  return "Ready";
}

std::string faceModuleLabel(const app::RuntimeStatus& status) {
  return app::formatFaceEngineLine(status, app::FaceLineStyle::Presenter);
}

std::string apiLabel(const app::RuntimeStatus& status) {
  if (!status.apiConfigured) {
    return "Missing origin";
  }
  if (status.apiProbeInFlight) {
    return "Probing";
  }
  if (status.connectivity != app::ConnectivityState::Connected) {
    return "Waiting for WiFi";
  }
  if (status.apiProbeSucceeded) {
    return "Reachable";
  }
  if (status.apiProbeStatusCode.has_value()) {
    return "Failed (" + status.apiProbeStatusCode.value() + ")";
  }
  return "Waiting";
}

std::string faceDetectLabel(const app::RuntimeStatus& status) {
  return app::formatFaceDetectLine(status, app::FaceLineStyle::Presenter);
}

uint64_t effectiveStatusTime(const app::RuntimeStatus& status) {
  if (status.snapshots.lastServerTime > 0) {
    return status.snapshots.lastServerTime;
  }

  return static_cast<uint64_t>(std::time(nullptr)) * 1000ULL;
}

std::string periodLabel(const app::RuntimeStatus& status) {
  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
      return "Enroll: " + activeEnrollmentSubject(status) + enrollmentProgressSuffix(status);
    case app::EnrollmentRunState::SavingTemplate:
      return "Enroll: saving";
    case app::EnrollmentRunState::Reporting:
      return "Enroll: reporting";
    case app::EnrollmentRunState::Done:
      return "Enroll: complete";
    case app::EnrollmentRunState::Failed:
      return "Enroll: failed";
    case app::EnrollmentRunState::Cancelled:
      return "Enroll: cancelled";
    case app::EnrollmentRunState::Idle:
    default:
      break;
  }

  if (!status.snapshots.attendanceConfig.has_value()) {
    return "Period: not configured";
  }

  const auto attendanceType =
      core::classifyAttendanceType(status.snapshots.attendanceConfig.value(), effectiveStatusTime(status));
  if (!attendanceType.has_value()) {
    return "Period: outside window";
  }

  return *attendanceType == core::AttendanceRecordType::ClockIn ? "Period: Clock In" : "Period: Clock Out";
}

std::string attendanceResultLabel(const app::RuntimeStatus& status) {
  switch (status.enrollmentState) {
    case app::EnrollmentRunState::Preparing:
    case app::EnrollmentRunState::Capturing:
      return "Rec " + activeEnrollmentSubject(status) + enrollmentProgressSuffix(status);
    case app::EnrollmentRunState::SavingTemplate:
      return "Saving...";
    case app::EnrollmentRunState::Reporting:
      return "Reporting...";
    case app::EnrollmentRunState::Done:
      return "Done";
    case app::EnrollmentRunState::Failed:
      return "Failed";
    case app::EnrollmentRunState::Cancelled:
      return "Cancelled";
    case app::EnrollmentRunState::Idle:
    default:
      break;
  }

  if (status.pendingQueueSize > 0) {
    return std::to_string(status.pendingQueueSize) + " record(s) queued";
  }

  if (status.lastAttendanceFeedback.has_value()) {
    return status.lastAttendanceFeedback.value();
  }

  if (status.lastErrorCode.has_value()) {
    return "Error: " + status.lastErrorCode.value();
  }

  return "";
}

std::vector<EnrollmentTaskItemViewModel> enrollmentTasks(const app::RuntimeStatus& status) {
  std::vector<EnrollmentTaskItemViewModel> items;
  items.reserve(status.snapshots.enrollmentTasks.size());

  for (const auto& task : status.snapshots.enrollmentTasks) {
    items.push_back(EnrollmentTaskItemViewModel{
        .taskId = task.taskId,
        .title = enrollmentTaskLabel(task),
        .meta = "Status: " + task.status,
    });
  }

  return items;
}

std::string enrollmentTaskSummary(const app::RuntimeStatus& status) {
  if (status.enrollmentPendingCount > 0) {
    return "Report pending";
  }
  if (status.snapshots.enrollmentTasks.empty()) {
    return "No tasks";
  }

  return std::to_string(status.snapshots.enrollmentTasks.size()) + " task(s)";
}

std::string cameraLabel(const app::RuntimeStatus& status) {
  const std::string enrollmentDetail = friendlyEnrollmentDetail(status);
  if (!enrollmentDetail.empty()) {
    return enrollmentDetail;
  }

  if (!status.cameraAvailable) {
    return "Camera unavailable";
  }

  if (!status.cameraReady) {
    if (!status.cameraLastError.empty()) {
      return "Camera init failed\n" + status.cameraLastError;
    }
    return "Camera present\nWaiting for init";
  }

  std::ostringstream oss;
  oss << (status.cameraSensorModel.empty() ? "Camera ready" : status.cameraSensorModel + " ready");
  if (status.cameraLastFrame.bytes > 0) {
    oss << "\n" << status.cameraLastFrame.width << "x" << status.cameraLastFrame.height << " "
        << face::cameraPixelFormatName(status.cameraLastFrame.pixelFormat) << " #" << status.cameraCaptureCount;
  } else {
    oss << "\nWaiting for first frame";
  }
  return oss.str();
}

}  // namespace

AppViewModel StatusScreenPresenter::build(const app::RuntimeStatus& status) {
  AppViewModel view = {};
  view.title = "Hitomi Device";
  view.subtitle = subtitleLabel(status);
  view.periodLine = periodLabel(status);
  view.cameraHintLine = cameraLabel(status);
  view.attendanceResultLine = attendanceResultLabel(status);
  view.captureActive = capturePageActive(status);
  view.captureRunning = captureRunning(status);
  view.captureTitleLine = captureTitle(status);
  view.captureStatusLine = friendlyEnrollmentDetail(status);
  view.captureProgressLine = captureProgress(status);
  view.captureActionLabel = captureActionLabel(status);
  view.enrollmentTasks = enrollmentTasks(status);
  view.enrollmentTaskSummaryLine = enrollmentTaskSummary(status);
  view.credentialsLine = credentialsLabel(status);
  view.storageLine = storageLabel(status);
  view.wifiLine = connectivityLabel(status.connectivity) +
      (status.activeWifiSsid.has_value() ? " (" + status.activeWifiSsid.value() + ")" : "");
  view.activationLine = activationLabel(status);
  view.apiLine = apiLabel(status);
  view.faceDetectLine = faceDetectLabel(status);
  view.syncLine = "Sync: " + syncLabel(status);
  view.taskLine = taskLabel(status);
  view.queueLine = "Queue: " + std::to_string(status.pendingQueueSize);
  view.errorLine = status.lastErrorCode.value_or("None");
  view.faceLine = faceModuleLabel(status);
  view.footer = status.firmwareTag;
  return view;
}

}  // namespace ui
