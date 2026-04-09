#include "infra/storage/template_manifest_codec.hpp"

#include <ArduinoJson.h>

namespace infra {
std::string encodeTemplateManifest(const TemplateManifest& manifest) {
  JsonDocument doc;
  doc["schemaVersion"] = manifest.schemaVersion;
  doc["updatedAt"] = manifest.updatedAt;

  JsonArray items = doc["items"].to<JsonArray>();
  for (const auto& item : manifest.items) {
    JsonObject entry = items.add<JsonObject>();
    entry["employeeId"] = item.employeeId;
    entry["updatedAt"] = item.updatedAt;
    entry["sizeBytes"] = item.sizeBytes;
  }

  std::string encoded;
  serializeJson(doc, encoded);
  return encoded;
}

std::optional<TemplateManifest> decodeTemplateManifest(const std::string& json) {
  JsonDocument doc;
  const auto error = deserializeJson(doc, json);
  if (error) {
    return std::nullopt;
  }

  TemplateManifest manifest = {};
  manifest.schemaVersion = doc["schemaVersion"] | 0;
  manifest.updatedAt = doc["updatedAt"] | 0ULL;
  if (manifest.schemaVersion != 1) {
    return std::nullopt;
  }

  JsonArrayConst items = doc["items"].as<JsonArrayConst>();
  for (JsonVariantConst item : items) {
    manifest.items.push_back(TemplateManifestItem{
        .employeeId = item["employeeId"] | "",
        .updatedAt = item["updatedAt"] | 0ULL,
        .sizeBytes = item["sizeBytes"] | 0ULL,
    });
  }

  return manifest;
}

TemplateLibrarySummary summarizeTemplateManifest(const std::optional<TemplateManifest>& manifest, uint64_t loadedAt) {
  TemplateLibrarySummary summary = {};
  summary.lastLoadedAt = loadedAt;
  if (!manifest.has_value()) {
    return summary;
  }

  summary.templateCount = manifest->items.size();
  summary.manifestUpdatedAt = manifest->updatedAt;
  return summary;
}

std::string templatePathForEmployee(const std::string& employeeId) {
  return "/templates/" + employeeId + ".bin";
}

}  // namespace infra
