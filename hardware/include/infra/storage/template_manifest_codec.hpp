#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "infra/template_store_port.hpp"

namespace infra {

struct TemplateManifestItem {
  std::string employeeId;
  uint64_t updatedAt = 0;
  uint64_t sizeBytes = 0;
};

struct TemplateManifest {
  int schemaVersion = 1;
  uint64_t updatedAt = 0;
  std::vector<TemplateManifestItem> items;
};

std::string encodeTemplateManifest(const TemplateManifest& manifest);
std::optional<TemplateManifest> decodeTemplateManifest(const std::string& json);
TemplateLibrarySummary summarizeTemplateManifest(const std::optional<TemplateManifest>& manifest, uint64_t loadedAt);
std::string templatePathForEmployee(const std::string& employeeId);

}  // namespace infra
