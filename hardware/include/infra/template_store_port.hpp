#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace infra {

inline constexpr char kTemplateStoreMountedReady[] = "mounted_ready";
inline constexpr char kTemplateStoreMountedEmpty[] = "mounted_empty";
inline constexpr char kTemplateStoreCardMissing[] = "card_missing";
inline constexpr char kTemplateStoreMountFailed[] = "mount_failed";
inline constexpr char kTemplateStoreManifestInvalid[] = "manifest_invalid";
inline constexpr char kTemplateStoreIoError[] = "io_error";
inline constexpr char kTemplateStoreDisabled[] = "disabled";

struct TemplateBlob {
  std::string employeeId;
  std::vector<uint8_t> bytes;
  uint64_t updatedAt = 0;
};

struct TemplateLibrarySummary {
  std::size_t templateCount = 0;
  uint64_t manifestUpdatedAt = 0;
  uint64_t lastLoadedAt = 0;
};

struct TemplateStoreHealthSummary {
  std::string statusCode;
  uint64_t checkedAt = 0;
  std::string detail;
  uint64_t cardSizeBytes = 0;
  uint64_t totalBytes = 0;
  uint64_t usedBytes = 0;
};

struct TemplateStoreStatus {
  bool ready = false;
  TemplateLibrarySummary summary;
  TemplateStoreHealthSummary health;
};

using TemplateStoreInitStatus = TemplateStoreStatus;

inline bool templateStoreMounted(const std::string& code) {
  return code == kTemplateStoreMountedReady || code == kTemplateStoreMountedEmpty;
}

inline bool templateStoreManifestBroken(const std::string& code) {
  return code == kTemplateStoreManifestInvalid;
}

inline bool templateStoreDisabled(const std::string& code) {
  return code == kTemplateStoreDisabled;
}

class TemplateStorePort {
 public:
  virtual ~TemplateStorePort() = default;
  virtual TemplateStoreInitStatus begin() = 0;
  virtual TemplateStoreStatus status() const = 0;
  virtual TemplateLibrarySummary loadSummary() const = 0;
  virtual std::optional<TemplateBlob> readTemplate(const std::string& employeeId) = 0;
  virtual bool upsertTemplate(const std::string& employeeId, const std::vector<uint8_t>& bytes, uint64_t updatedAt) = 0;
  virtual bool removeTemplate(const std::string& employeeId) = 0;
};

class NoopTemplateStorePort final : public TemplateStorePort {
 public:
  TemplateStoreInitStatus begin() override {
    status_.ready = false;
    status_.health.statusCode = kTemplateStoreCardMissing;
    status_.health.detail = "template store not wired";
    return status_;
  }

  TemplateStoreStatus status() const override {
    return status_;
  }

  TemplateLibrarySummary loadSummary() const override {
    return status_.summary;
  }

  std::optional<TemplateBlob> readTemplate(const std::string& employeeId) override {
    (void)employeeId;
    return std::nullopt;
  }

  bool upsertTemplate(const std::string& employeeId, const std::vector<uint8_t>& bytes, uint64_t updatedAt) override {
    (void)employeeId;
    (void)bytes;
    (void)updatedAt;
    return false;
  }

  bool removeTemplate(const std::string& employeeId) override {
    (void)employeeId;
    return false;
  }

 private:
  TemplateStoreStatus status_ = {};
};

}  // namespace infra
