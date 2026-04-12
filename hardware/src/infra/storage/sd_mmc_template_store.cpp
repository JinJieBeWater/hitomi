#include "infra/storage/sd_mmc_template_store.hpp"

#include <Arduino.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#include <driver/sdmmc_host.h>
#include <driver/sdmmc_defs.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include "board/app_config.hpp"
#include "board/pins.hpp"

namespace infra {
namespace {

constexpr char kTemplatesDirPath[] = "/templates";
constexpr char kManifestPath[] = "/templates/manifest.json";
constexpr char kManifestTempPath[] = "/templates/manifest.tmp.json";

std::string mountedPath(const std::string& path) {
  return std::string(board::kSdMountPoint) + path;
}

uint64_t nowMs() {
  return static_cast<uint64_t>(millis());
}

std::optional<std::string> readTextFile(const char* path) {
  const std::string mounted = mountedPath(path);
  struct stat fileInfo = {};
  if (::stat(mounted.c_str(), &fileInfo) != 0) {
    return std::nullopt;
  }

  std::FILE* file = std::fopen(mounted.c_str(), "rb");
  if (!file) {
    return std::nullopt;
  }

  std::string content(static_cast<std::size_t>(fileInfo.st_size), '\0');
  if (!content.empty()) {
    const std::size_t readCount = std::fread(content.data(), 1, content.size(), file);
    if (readCount != content.size()) {
      std::fclose(file);
      return std::nullopt;
    }
  }

  std::fclose(file);
  return content;
}

bool replaceFile(const char* tempPath, const char* finalPath) {
  const std::string mountedTemp = mountedPath(tempPath);
  const std::string mountedFinal = mountedPath(finalPath);
  if (::access(mountedFinal.c_str(), F_OK) == 0 && std::remove(mountedFinal.c_str()) != 0) {
    return false;
  }
  return std::rename(mountedTemp.c_str(), mountedFinal.c_str()) == 0;
}

bool writeTextFileAtomically(const char* tempPath, const char* finalPath, const std::string& content) {
  const std::string mountedTemp = mountedPath(tempPath);
  std::FILE* file = std::fopen(mountedTemp.c_str(), "wb");
  if (!file) {
    return false;
  }

  const std::size_t written = std::fwrite(content.data(), 1, content.size(), file);
  std::fclose(file);
  if (written != content.size()) {
    std::remove(mountedTemp.c_str());
    return false;
  }

  if (!replaceFile(tempPath, finalPath)) {
    std::remove(mountedTemp.c_str());
    return false;
  }

  return true;
}

std::optional<std::vector<uint8_t>> readBinaryFile(const std::string& path) {
  const std::string mounted = mountedPath(path);
  struct stat fileInfo = {};
  if (::stat(mounted.c_str(), &fileInfo) != 0) {
    return std::nullopt;
  }

  std::FILE* file = std::fopen(mounted.c_str(), "rb");
  if (!file) {
    return std::nullopt;
  }

  std::vector<uint8_t> bytes(static_cast<std::size_t>(fileInfo.st_size));
  if (!bytes.empty()) {
    const std::size_t readCount = std::fread(bytes.data(), 1, bytes.size(), file);
    if (readCount != bytes.size()) {
      std::fclose(file);
      return std::nullopt;
    }
  }

  std::fclose(file);
  return bytes;
}

bool writeBinaryFileAtomically(const std::string& tempPath, const std::string& finalPath, const std::vector<uint8_t>& bytes) {
  const std::string mountedTemp = mountedPath(tempPath);
  std::FILE* file = std::fopen(mountedTemp.c_str(), "wb");
  if (!file) {
    return false;
  }

  const std::size_t written = bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), file);
  std::fclose(file);
  if (written != bytes.size()) {
    std::remove(mountedTemp.c_str());
    return false;
  }

  if (!replaceFile(tempPath.c_str(), finalPath.c_str())) {
    std::remove(mountedTemp.c_str());
    return false;
  }

  return true;
}

std::string errnoDetail(const char* action, const std::string& path) {
  return std::string(action) + " " + path + " failed: " + std::strerror(errno);
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
  manifest_.reset();
  status_.ready = false;

  if (!mounted_ || card_ == nullptr) {
    endMount();

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1;
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slotConfig = SDMMC_SLOT_CONFIG_DEFAULT();
    slotConfig.width = 1;
    slotConfig.clk = board::kSdMmcClkPin;
    slotConfig.cmd = board::kSdMmcCmdPin;
    slotConfig.d0 = board::kSdMmcD0Pin;
    slotConfig.d1 = GPIO_NUM_NC;
    slotConfig.d2 = GPIO_NUM_NC;
    slotConfig.d3 = GPIO_NUM_NC;
    slotConfig.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    esp_vfs_fat_sdmmc_mount_config_t mountConfig = {
        .format_if_mount_failed = board::kSdFormatIfMountFailed,
        .max_files = board::kSdMaxOpenFiles,
        .allocation_unit_size = 0,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    sdmmc_card_t* mountedCard = nullptr;
    const esp_err_t err = esp_vfs_fat_sdmmc_mount(
        board::kSdMountPoint,
        &host,
        &slotConfig,
        &mountConfig,
        &mountedCard);
    if (err != ESP_OK) {
      setUnavailableState(
          kTemplateStoreMountFailed,
          std::string("esp_vfs_fat_sdmmc_mount failed: ") + esp_err_to_name(err),
          true);
      return status_;
    }

    mounted_ = true;
    card_ = mountedCard;
  }
  refreshCapacityMetrics();

  if (card_ == nullptr) {
    setUnavailableState(kTemplateStoreCardMissing, "no SD card attached", true);
    return status_;
  }

  const ManifestLoadResult manifestLoad = loadManifest();
  if (templateStoreManifestBroken(manifestLoad.statusCode)) {
    setUnavailableState(manifestLoad.statusCode, manifestLoad.detail, false);
    return status_;
  }

  if (!ensureTemplateDirectory()) {
    setUnavailableState(kTemplateStoreIoError, errnoDetail("mkdir", mountedPath(kTemplatesDirPath)), false);
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
    setUnavailableState(kTemplateStoreIoError, errnoDetail("mkdir", mountedPath(kTemplatesDirPath)), true);
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

  const std::string templatePath = mountedPath(templatePathForEmployee(employeeId));
  if (::access(templatePath.c_str(), F_OK) == 0 && std::remove(templatePath.c_str()) != 0) {
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
    esp_vfs_fat_sdcard_unmount(board::kSdMountPoint, card_);
    card_ = nullptr;
    mounted_ = false;
  }
}

void SdMmcTemplateStore::refreshCapacityMetrics() {
  if (!mounted_ || card_ == nullptr) {
    status_.health.cardSizeBytes = 0;
    status_.health.totalBytes = 0;
    status_.health.usedBytes = 0;
    return;
  }

  status_.health.cardSizeBytes =
      static_cast<uint64_t>(card_->csd.capacity) * card_->csd.sector_size;

  uint64_t totalBytes = 0;
  uint64_t freeBytes = 0;
  if (esp_vfs_fat_info(board::kSdMountPoint, &totalBytes, &freeBytes) == ESP_OK) {
    status_.health.totalBytes = totalBytes;
    status_.health.usedBytes = totalBytes - freeBytes;
  } else {
    status_.health.totalBytes = 0;
    status_.health.usedBytes = 0;
  }
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
  const std::string mountedDir = mountedPath(kTemplatesDirPath);
  struct stat dirInfo = {};
  if (::stat(mountedDir.c_str(), &dirInfo) == 0) {
    if (S_ISDIR(dirInfo.st_mode)) {
      return true;
    }

    if (std::remove(mountedDir.c_str()) != 0) {
      return false;
    }
  }
  return ::mkdir(mountedDir.c_str(), 0755) == 0 || errno == EEXIST;
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
