#include "infra/storage/storage_aux_codec.hpp"

#include <ArduinoJson.h>

namespace infra {

std::string encodeStorageAuxState(const StorageAuxState& storageAux) {
  DynamicJsonDocument doc(2048);

  JsonObject summary = doc.createNestedObject("templateLibrarySummary");
  summary["templateCount"] = storageAux.templateLibrarySummary.templateCount;
  summary["manifestUpdatedAt"] = storageAux.templateLibrarySummary.manifestUpdatedAt;
  summary["lastLoadedAt"] = storageAux.templateLibrarySummary.lastLoadedAt;

  JsonObject health = doc.createNestedObject("templateStoreHealth");
  health["statusCode"] = storageAux.templateStoreHealth.statusCode;
  health["checkedAt"] = storageAux.templateStoreHealth.checkedAt;
  health["detail"] = storageAux.templateStoreHealth.detail;
  health["cardSizeBytes"] = storageAux.templateStoreHealth.cardSizeBytes;
  health["totalBytes"] = storageAux.templateStoreHealth.totalBytes;
  health["usedBytes"] = storageAux.templateStoreHealth.usedBytes;

  std::string encoded;
  serializeJson(doc, encoded);
  return encoded;
}

std::optional<StorageAuxState> decodeStorageAuxState(const std::string& json) {
  DynamicJsonDocument doc(4096);
  const auto error = deserializeJson(doc, json);
  if (error) {
    return std::nullopt;
  }

  StorageAuxState storageAux = {};
  JsonVariantConst summary = doc["templateLibrarySummary"];
  storageAux.templateLibrarySummary.templateCount = summary["templateCount"] | 0U;
  storageAux.templateLibrarySummary.manifestUpdatedAt = summary["manifestUpdatedAt"] | 0ULL;
  storageAux.templateLibrarySummary.lastLoadedAt = summary["lastLoadedAt"] | 0ULL;

  JsonVariantConst health = doc["templateStoreHealth"];
  storageAux.templateStoreHealth.statusCode = health["statusCode"] | "";
  storageAux.templateStoreHealth.checkedAt = health["checkedAt"] | 0ULL;
  storageAux.templateStoreHealth.detail = health["detail"] | "";
  storageAux.templateStoreHealth.cardSizeBytes = health["cardSizeBytes"] | 0ULL;
  storageAux.templateStoreHealth.totalBytes = health["totalBytes"] | 0ULL;
  storageAux.templateStoreHealth.usedBytes = health["usedBytes"] | 0ULL;

  return storageAux;
}

}  // namespace infra
