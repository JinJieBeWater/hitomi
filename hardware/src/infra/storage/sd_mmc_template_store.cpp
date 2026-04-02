#include "infra/storage/sd_mmc_template_store.hpp"

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

#include "board/app_config.hpp"
#include "board/pins.hpp"

namespace infra {
namespace {

constexpr char kTemplatesDirPath[] = "/templates";
constexpr char kManifestPath[] = "/templates/manifest.json";
constexpr char kManifestTempPath[] = "/templates/manifest.tmp.json";

uint64_t nowMs() {
  return static_cast<uint64_t>(millis());
}

std::optional<std::string> readTextFile(const char* path) {
  if (!SD_MMC.exists(path)) {
    return std::nullopt;
  }

  File file = SD_MMC.open(path, "r");
  if (!file) {
    return std::nullopt;
  }

  std::string content;
  content.reserve(file.size());
  while (file.available()) {
    content.push_back(static_cast<char>(file.read()));
  }

  file.close();
  return content;
}

bool replaceFile(const char* tempPath, const char* finalPath) {
  if (SD_MMC.exists(finalPath) && !SD_MMC.remove(finalPath)) {
    return false;
  }
  return SD_MMC.rename(tempPath, finalPath);
}

bool writeTextFileAtomically(const char* tempPath, const char* finalPath, const std::string& content) {
  File file = SD_MMC.open(tempPath, "w");
  if (!file) {
    return false;
  }

  const std::size_t written =
      file.write(reinterpret_cast<const uint8_t*>(content.data()), content.size());
  file.close();
  if (written != content.size()) {
    SD_MMC.remove(tempPath);
    return false;
  }

  if (!replaceFile(tempPath, finalPath)) {
    SD_MMC.remove(tempPath);
    return false;
  }

  return true;
}

std::optional<std::vector<uint8_t>> readBinaryFile(const std::string& path) {
  if (!SD_MMC.exists(path.c_str())) {
    return std::nullopt;
  }

  File file = SD_MMC.open(path.c_str(), "r");
  if (!file) {
    return std::nullopt;
  }

  std::vector<uint8_t> bytes(file.size());
  if (!bytes.empty()) {
    const std::size_t readCount = file.read(bytes.data(), bytes.size());
    if (readCount != bytes.size()) {
      file.close();
      return std::nullopt;
    }
  }

  file.close();
  return bytes;
}

bool writeBinaryFileAtomically(const std::string& tempPath, const std::string& finalPath, const std::vector<uint8_t>& bytes) {
  File file = SD_MMC.open(tempPath.c_str(), "w");
  if (!file) {
    return false;
  }

  const std::size_t written = bytes.empty() ? 0 : file.write(bytes.data(), bytes.size());
  file.close();
  if (written != bytes.size()) {
    SD_MMC.remove(tempPath.c_str());
    return false;
  }

  if (!replaceFile(tempPath.c_str(), finalPath.c_str())) {
    SD_MMC.remove(tempPath.c_str());
    return false;
  }

  return true;
}

const TemplateManifestItem* findManifestItem(const std::optional<TemplateManifest>& manifest, const std::string& employeeId) {
  if (!manifest.has_value()) {
    return nullptr;
  }

  for (const auto& item : manifest->items) {
    if (item.employeeId == employeeId) {
      return &item;
    }
  }

  return nullptr;
}

std::string templateStatusCodeForCount(std::size_t templateCount) {
  return templateCount == 0 ? kTemplateStoreMountedEmpty : kTemplateStoreMountedReady;
}

}  // namespace

TemplateStoreInitStatus SdMmcTemplateStore::begin() {
  endMount();
  manifest_.reset();
  status_.ready = false;

  if (!SD_MMC.setPins(board::kSdMmcClkPin, board::kSdMmcCmdPin, board::kSdMmcD0Pin)) {
    setUnavailableState(kTemplateStoreMountFailed, "SD_MMC.setPins failed", false);
    return status_;
  }

  if (!SD_MMC.begin(
          board::kSdMountPoint,
          board::kSdMode1Bit,
          board::kSdFormatIfMountFailed,
          BOARD_MAX_SDMMC_FREQ,
          board::kSdMaxOpenFiles)) {
    const std::string statusCode = SD_MMC.cardType() == CARD_NONE ? kTemplateStoreCardMissing : kTemplateStoreMountFailed;
    setUnavailableState(statusCode, "SD_MMC.begin failed", true);
    return status_;
  }

  mounted_ = true;
  refreshCapacityMetrics();

  if (SD_MMC.cardType() == CARD_NONE) {
    setUnavailableState(kTemplateStoreCardMissing, "no SD card attached", true);
    return status_;
  }

  const ManifestLoadResult manifestLoad = loadManifest();
  if (templateStoreManifestBroken(manifestLoad.statusCode)) {
    setUnavailableState(manifestLoad.statusCode, manifestLoad.detail, false);
    return status_;
  }

  setReadyState(manifestLoad.manifest, manifestLoad.statusCode, manifestLoad.detail);
  return status_;
}

TemplateStoreStatus SdMmcTemplateStore::status() const {
  return status_;
}

TemplateLibrarySummary SdMmcTemplateStore::loadSummary() const {
  return status_.summary;
}

std::optional<TemplateBlob> SdMmcTemplateStore::readTemplate(const std::string& employeeId) {
  if (!status_.ready || !validateEmployeeId(employeeId)) {
    return std::nullopt;
  }

  const TemplateManifestItem* manifestItem = findManifestItem(manifest_, employeeId);
  if (manifestItem == nullptr) {
    return std::nullopt;
  }

  auto bytes = readBinaryFile(templatePathForEmployee(employeeId));
  if (!bytes.has_value()) {
    setUnavailableState(kTemplateStoreIoError, "failed to read template blob", true);
    return std::nullopt;
  }

  return TemplateBlob{
      .employeeId = employeeId,
      .bytes = std::move(bytes.value()),
      .updatedAt = manifestItem->updatedAt,
  };
}

bool SdMmcTemplateStore::upsertTemplate(
    const std::string& employeeId,
    const std::vector<uint8_t>& bytes,
    uint64_t updatedAt) {
  if (!status_.ready || !validateEmployeeId(employeeId) || !manifest_.has_value()) {
    return false;
  }
  if (!ensureTemplateDirectory()) {
    setUnavailableState(kTemplateStoreIoError, "failed to create template directory", true);
    return false;
  }

  const std::string templatePath = templatePathForEmployee(employeeId);
  const std::string tempPath = "/templates/.tmp_" + employeeId + ".bin";
  if (!writeBinaryFileAtomically(tempPath, templatePath, bytes)) {
    setUnavailableState(kTemplateStoreIoError, "failed to write template blob", true);
    return false;
  }

  bool found = false;
  for (auto& item : manifest_->items) {
    if (item.employeeId != employeeId) {
      continue;
    }

    item.updatedAt = updatedAt;
    item.sizeBytes = bytes.size();
    found = true;
    break;
  }

  if (!found) {
    manifest_->items.push_back(TemplateManifestItem{
        .employeeId = employeeId,
        .updatedAt = updatedAt,
        .sizeBytes = bytes.size(),
    });
  }

  manifest_->updatedAt = updatedAt;
  if (!writeManifest()) {
    setUnavailableState(kTemplateStoreIoError, "failed to write manifest", true);
    return false;
  }

  setReadyState(manifest_, templateStatusCodeForCount(manifest_->items.size()), "ok");
  return true;
}

bool SdMmcTemplateStore::removeTemplate(const std::string& employeeId) {
  if (!status_.ready || !validateEmployeeId(employeeId) || !manifest_.has_value()) {
    return false;
  }

  const std::string templatePath = templatePathForEmployee(employeeId);
  if (SD_MMC.exists(templatePath.c_str()) && !SD_MMC.remove(templatePath.c_str())) {
    setUnavailableState(kTemplateStoreIoError, "failed to remove template blob", true);
    return false;
  }

  auto& items = manifest_->items;
  for (auto it = items.begin(); it != items.end(); ++it) {
    if (it->employeeId == employeeId) {
      items.erase(it);
      break;
    }
  }

  manifest_->updatedAt = nowMs();
  if (!writeManifest()) {
    setUnavailableState(kTemplateStoreIoError, "failed to write manifest", true);
    return false;
  }

  setReadyState(manifest_, templateStatusCodeForCount(manifest_->items.size()), "ok");
  return true;
}

void SdMmcTemplateStore::endMount() {
  if (mounted_) {
    SD_MMC.end();
    mounted_ = false;
  }
}

void SdMmcTemplateStore::refreshCapacityMetrics() {
  if (!mounted_) {
    status_.health.cardSizeBytes = 0;
    status_.health.totalBytes = 0;
    status_.health.usedBytes = 0;
    return;
  }

  status_.health.cardSizeBytes = SD_MMC.cardSize();
  status_.health.totalBytes = SD_MMC.totalBytes();
  status_.health.usedBytes = SD_MMC.usedBytes();
}

void SdMmcTemplateStore::setReadyState(
    const std::optional<TemplateManifest>& manifest,
    const std::string& statusCode,
    const std::string& detail) {
  if (manifest.has_value()) {
    manifest_ = manifest;
  } else {
    manifest_ = TemplateManifest{};
  }

  status_.ready = true;
  status_.summary = summarizeTemplateManifest(manifest_, nowMs());
  status_.health.statusCode = statusCode;
  status_.health.checkedAt = nowMs();
  status_.health.detail = detail;
  refreshCapacityMetrics();
}

void SdMmcTemplateStore::setUnavailableState(
    const std::string& statusCode,
    const std::string& detail,
    bool clearMount) {
  if (clearMount) {
    endMount();
  }

  manifest_.reset();
  status_.ready = false;
  status_.health.statusCode = statusCode;
  status_.health.checkedAt = nowMs();
  status_.health.detail = detail;
  refreshCapacityMetrics();
}

SdMmcTemplateStore::ManifestLoadResult SdMmcTemplateStore::loadManifest() const {
  auto content = readTextFile(kManifestPath);
  if (!content.has_value()) {
    return {
        .manifest = std::nullopt,
        .statusCode = kTemplateStoreMountedEmpty,
        .detail = "manifest missing",
    };
  }

  auto manifest = decodeTemplateManifest(content.value());
  if (!manifest.has_value()) {
    return {
        .manifest = std::nullopt,
        .statusCode = kTemplateStoreManifestInvalid,
        .detail = "manifest parse failed",
    };
  }

  return {
      .manifest = manifest,
      .statusCode = templateStatusCodeForCount(manifest->items.size()),
      .detail = "ok",
  };
}

bool SdMmcTemplateStore::ensureTemplateDirectory() {
  return SD_MMC.exists(kTemplatesDirPath) || SD_MMC.mkdir(kTemplatesDirPath);
}

bool SdMmcTemplateStore::writeManifest() {
  if (!ensureTemplateDirectory() || !manifest_.has_value()) {
    return false;
  }
  return writeTextFileAtomically(kManifestTempPath, kManifestPath, encodeTemplateManifest(manifest_.value()));
}

bool SdMmcTemplateStore::validateEmployeeId(const std::string& employeeId) const {
  return !employeeId.empty() && employeeId.find('/') == std::string::npos;
}

}  // namespace infra
