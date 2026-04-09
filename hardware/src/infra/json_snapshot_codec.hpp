#pragma once

#include <ArduinoJson.h>

#include <optional>

#include "core/models.hpp"

namespace infra::detail {

inline std::optional<core::AttendanceConfigSnapshot> parseAttendanceConfig(JsonVariantConst value) {
  if (value.isNull()) {
    return std::nullopt;
  }

  return core::AttendanceConfigSnapshot{
      .id = value["id"] | "",
      .workStartMinute = value["workStartMinute"] | 0,
      .workEndMinute = value["workEndMinute"] | 0,
      .offStartMinute = value["offStartMinute"] | 0,
      .offEndMinute = value["offEndMinute"] | 0,
      .updatedAt = value["updatedAt"] | 0ULL,
  };
}

inline core::EnrollmentTaskSnapshot parseEnrollmentTask(JsonVariantConst value) {
  return core::EnrollmentTaskSnapshot{
      .taskId = value["taskId"] | "",
      .employeeId = value["employeeId"] | "",
      .employeeCode = value["employeeCode"] | "",
      .employeeName = value["employeeName"] | "",
      .status = value["status"] | "",
      .createdAt = value["createdAt"] | 0ULL,
      .updatedAt = value["updatedAt"] | 0ULL,
  };
}

inline core::EmployeeSnapshot parseEmployee(JsonVariantConst value) {
  return core::EmployeeSnapshot{
      .id = value["id"] | "",
      .code = value["code"] | "",
      .name = value["name"] | "",
      .updatedAt = value["updatedAt"] | 0ULL,
  };
}

}  // namespace infra::detail
