#pragma once

#include <optional>
#include <sdmmc_cmd.h>
#include <string>

#include "infra/storage/template_manifest_codec.hpp"
#include "infra/template_store_port.hpp"

namespace infra {

class SdMmcTemplateStore final : public TemplateStorePort {
 public:
  TemplateStoreInitStatus begin() override;
  TemplateStoreStatus status() const override;
  TemplateLibrarySummary loadSummary() const override;
  std::vector<std::string> listTemplateEmployeeIds() const override;
  std::optional<TemplateBlob> readTemplate(const std::string& employeeId) override;
  bool upsertTemplate(const std::string& employeeId, const std::vector<uint8_t>& bytes, uint64_t updatedAt) override;
  bool removeTemplate(const std::string& employeeId) override;

 private:
  struct ManifestLoadResult {
    std::optional<TemplateManifest> manifest;
    std::string statusCode;
    std::string detail;
  };

  void endMount();
  void refreshCapacityMetrics();
  void setReadyState(const std::optional<TemplateManifest>& manifest, const std::string& statusCode, const std::string& detail);
  void setUnavailableState(const std::string& statusCode, const std::string& detail, bool clearMount);
  ManifestLoadResult loadManifest() const;
  bool ensureTemplateDirectory();
  bool writeManifest();
  bool validateEmployeeId(const std::string& employeeId) const;

  TemplateStoreStatus status_ = {};
  std::optional<TemplateManifest> manifest_;
  bool mounted_ = false;
  sdmmc_card_t* card_ = nullptr;
};

}  // namespace infra
