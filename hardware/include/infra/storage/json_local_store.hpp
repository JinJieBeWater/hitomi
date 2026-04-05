#pragma once

#include <memory>

#include "infra/local_store.hpp"

namespace infra {

class JsonLocalStore final : public LocalStore {
 public:
  JsonLocalStore();
  ~JsonLocalStore() override;

  JsonLocalStore(const JsonLocalStore&) = delete;
  JsonLocalStore& operator=(const JsonLocalStore&) = delete;
  JsonLocalStore(JsonLocalStore&&) noexcept;
  JsonLocalStore& operator=(JsonLocalStore&&) noexcept;

  LocalStoreInitStatus begin() override;
  StoredRuntimeState load() override;
  bool saveCredentials(const core::DeviceCredentials& credentials) override;
  bool saveSnapshots(const core::SnapshotBundle& snapshots) override;
  bool savePendingAttendanceRecords(
      const std::vector<core::PendingAttendanceRecord>& records) override;
  bool saveFailureLogs(const std::vector<core::FailureLogEntry>& logs) override;
  bool saveStorageAux(const StorageAuxState& storageAux) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace infra
