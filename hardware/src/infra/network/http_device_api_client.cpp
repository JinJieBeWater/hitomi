#include "infra/network/http_device_api_client.hpp"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include <utility>

#include "core/use_cases.hpp"

namespace infra {
namespace {

core::ApiError makeTransportError(const std::string& code, const std::string& message, bool retryable) {
  return {
      .code = code,
      .message = message,
      .retryable = retryable,
  };
}

std::optional<core::ApiError> parseApiError(JsonVariantConst root) {
  JsonVariantConst error = root["error"];
  if (error.isNull()) {
    return std::nullopt;
  }

  return core::ApiError{
      .code = error["code"] | "UNKNOWN_ERROR",
      .message = error["message"] | "Unknown error",
      .retryable = error["retryable"] | false,
  };
}

template <typename T, typename ParseFn>
ApiResult<T> postJson(const std::string& url, const DynamicJsonDocument& requestDoc, ParseFn parseFn) {
  ApiResult<T> result = {};
  if (WiFi.status() != WL_CONNECTED) {
    result.error = makeTransportError("NETWORK_UNAVAILABLE", "WiFi is not connected", true);
    return result;
  }

  String body;
  serializeJson(requestDoc, body);

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, url.c_str())) {
    result.error = makeTransportError("HTTP_BEGIN_FAILED", "Failed to open HTTP connection", true);
    return result;
  }

  http.addHeader("Content-Type", "application/json");
  const int httpCode = http.POST(body);
  const String responseBody = http.getString();
  http.end();

  if (httpCode <= 0) {
    result.error = makeTransportError(
        "HTTP_REQUEST_FAILED", "HTTP request failed: " + std::to_string(httpCode), true);
    return result;
  }

  DynamicJsonDocument responseDoc(32 * 1024);
  const auto deserializeError = deserializeJson(responseDoc, responseBody);
  if (deserializeError) {
    result.error = makeTransportError("INVALID_RESPONSE", deserializeError.c_str(), true);
    return result;
  }

  const bool success = responseDoc["success"] | false;
  if (!success) {
    result.error = parseApiError(responseDoc);
    if (!result.error.has_value()) {
      result.error = makeTransportError("UNKNOWN_ERROR", "Device API returned failure without error body", true);
    }
    return result;
  }

  JsonVariantConst data = responseDoc["data"];
  if (data.isNull()) {
    result.error = makeTransportError("INVALID_RESPONSE", "Missing data payload", true);
    return result;
  }

  auto parsed = parseFn(data);
  if (!parsed.has_value()) {
    result.error = makeTransportError("INVALID_RESPONSE", "Failed to parse success payload", true);
    return result;
  }

  result.success = true;
  result.data = parsed;
  return result;
}

std::optional<core::AttendanceConfigSnapshot> parseAttendanceConfig(JsonVariantConst value) {
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

std::optional<core::EnrollmentTaskSnapshot> parseEnrollmentTask(JsonVariantConst value) {
  if (value.isNull()) {
    return std::nullopt;
  }

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

std::optional<core::SyncPayload> parseSyncPayload(JsonVariantConst data) {
  core::SyncPayload payload = {};
  payload.serverTime = data["serverTime"] | 0ULL;
  payload.timezone = data["timezone"] | "Asia/Shanghai";
  JsonVariantConst device = data["device"];
  payload.deviceId = device["id"] | "";
  payload.deviceName = device["name"] | "";
  payload.deviceStatus = device["status"] | "";
  payload.attendanceConfig = parseAttendanceConfig(data["attendanceConfig"]);
  payload.enrollmentTask = parseEnrollmentTask(data["enrollmentTask"]);

  JsonArrayConst employees = data["employees"].as<JsonArrayConst>();
  for (JsonVariantConst item : employees) {
    payload.employees.push_back(core::EmployeeSnapshot{
        .id = item["id"] | "",
        .code = item["code"] | "",
        .name = item["name"] | "",
        .updatedAt = item["updatedAt"] | 0ULL,
    });
  }

  return payload;
}

std::optional<EnrollmentReportResponse> parseEnrollmentReportResponse(JsonVariantConst data) {
  return EnrollmentReportResponse{
      .taskId = data["taskId"] | "",
      .employeeId = data["employeeId"] | "",
      .status = data["status"] | "",
      .applied = data["applied"] | false,
  };
}

std::optional<AttendanceUploadResponse> parseAttendanceUploadResponse(JsonVariantConst data) {
  AttendanceUploadResponse response = {};
  JsonArrayConst results = data["results"].as<JsonArrayConst>();
  for (JsonVariantConst item : results) {
    auto status = core::attendanceUploadStatusFromApiValue(item["status"] | "");
    if (!status.has_value()) {
      return std::nullopt;
    }

    core::AttendanceUploadItemResult parsed = {
        .clientRecordId = item["clientRecordId"] | "",
        .status = status.value(),
        .attendanceRecordId = item["attendanceRecordId"].isNull()
            ? std::nullopt
            : std::optional<std::string>(item["attendanceRecordId"].as<const char*>()),
        .error = std::nullopt,
    };

    if (!item["error"].isNull()) {
      parsed.error = core::ApiError{
          .code = item["error"]["code"] | "",
          .message = item["error"]["message"] | "",
          .retryable = item["error"]["retryable"] | false,
      };
    }

    response.results.push_back(parsed);
  }

  JsonVariantConst summary = data["summary"];
  response.saved = summary["saved"] | 0;
  response.updatedEarlier = summary["updatedEarlier"] | 0;
  response.ignoredDuplicate = summary["ignoredDuplicate"] | 0;
  response.rejected = summary["rejected"] | 0;
  return response;
}

}  // namespace

HttpDeviceApiClient::HttpDeviceApiClient(std::string baseUrl)
    : baseUrl_(std::move(baseUrl)) {}

bool HttpDeviceApiClient::configured() const {
  return !baseUrl_.empty();
}

ApiResult<ServerProbeResponse> HttpDeviceApiClient::probeServer() {
  ApiResult<ServerProbeResponse> result = {};
  if (WiFi.status() != WL_CONNECTED) {
    result.error = makeTransportError("NETWORK_UNAVAILABLE", "WiFi is not connected", true);
    return result;
  }

  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, endpointUrl("/rpc/healthCheck").c_str())) {
    result.error = makeTransportError("HTTP_BEGIN_FAILED", "Failed to open HTTP connection", true);
    return result;
  }

  const int httpCode = http.POST("");
  const String responseBody = http.getString();
  http.end();

  if (httpCode <= 0) {
    result.error = makeTransportError(
        "HTTP_REQUEST_FAILED", "HTTP request failed: " + std::to_string(httpCode), true);
    return result;
  }

  DynamicJsonDocument responseDoc(1024);
  const auto deserializeError = deserializeJson(responseDoc, responseBody);
  if (deserializeError) {
    result.error = makeTransportError("INVALID_RESPONSE", deserializeError.c_str(), true);
    return result;
  }

  if (httpCode < 200 || httpCode >= 300) {
    result.error = makeTransportError(
        "HTTP_" + std::to_string(httpCode),
        "RPC healthCheck returned HTTP " + std::to_string(httpCode),
        httpCode >= 500);
    return result;
  }

  const char* rpcValue = responseDoc["json"];
  if (rpcValue == nullptr || std::string(rpcValue) != "OK") {
    result.error = makeTransportError("INVALID_RESPONSE", "Unexpected RPC healthCheck payload", true);
    return result;
  }

  result.success = true;
  result.data = ServerProbeResponse{
      .service = "rpc.healthCheck",
      .now = 0,
  };
  return result;
}

ApiResult<core::SyncPayload> HttpDeviceApiClient::sync(const core::DeviceCredentials& credentials) {
  DynamicJsonDocument requestDoc(1024);
  requestDoc["deviceCode"] = credentials.deviceCode;
  requestDoc["apiKey"] = credentials.apiKey;
  return postJson<core::SyncPayload>(endpointUrl("/api/device/sync"), requestDoc, parseSyncPayload);
}

ApiResult<EnrollmentReportResponse> HttpDeviceApiClient::reportEnrollment(
    const core::DeviceCredentials& credentials, const EnrollmentReportRequest& request) {
  DynamicJsonDocument requestDoc(1024);
  requestDoc["deviceCode"] = credentials.deviceCode;
  requestDoc["apiKey"] = credentials.apiKey;
  requestDoc["taskId"] = request.taskId;
  requestDoc["employeeId"] = request.employeeId;
  requestDoc["result"] = request.result;
  requestDoc["finishedAt"] = request.finishedAt;
  if (request.failureReason.has_value()) {
    requestDoc["failureReason"] = request.failureReason.value();
  } else {
    requestDoc["failureReason"] = nullptr;
  }

  return postJson<EnrollmentReportResponse>(
      endpointUrl("/api/device/enrollment/report"), requestDoc, parseEnrollmentReportResponse);
}

ApiResult<AttendanceUploadResponse> HttpDeviceApiClient::uploadAttendance(
    const core::DeviceCredentials& credentials,
    const std::vector<core::PendingAttendanceRecord>& records) {
  DynamicJsonDocument requestDoc(4096 + (records.size() * 256));
  requestDoc["deviceCode"] = credentials.deviceCode;
  requestDoc["apiKey"] = credentials.apiKey;
  JsonArray jsonRecords = requestDoc.createNestedArray("records");
  for (const auto& record : records) {
    JsonObject entry = jsonRecords.createNestedObject();
    entry["clientRecordId"] = record.clientRecordId;
    entry["employeeId"] = record.employeeId;
    entry["recognizedAt"] = record.recognizedAt;
    entry["type"] = core::attendanceTypeToApiValue(record.type);
  }

  return postJson<AttendanceUploadResponse>(
      endpointUrl("/api/device/attendance/upload"), requestDoc, parseAttendanceUploadResponse);
}

std::string HttpDeviceApiClient::endpointUrl(const char* path) const {
  if (baseUrl_.empty()) {
    return path;
  }
  if (baseUrl_.back() == '/') {
    return baseUrl_.substr(0, baseUrl_.size() - 1) + path;
  }
  return baseUrl_ + path;
}

}  // namespace infra
