#include "app/runtime_storage_ops.hpp"

#include <Arduino.h>

#include <algorithm>
#include <utility>

#include "board/app_config.hpp"
#include "core/use_cases.hpp"
#include "infra/local_store.hpp"
#include "infra/template_store_port.hpp"

namespace app {

void initializeLocalStore(const RuntimeContext& context, RuntimeState& state) {
  const infra::LocalStoreInitStatus initStatus = context.localStore.begin();
  state.credentialsReady = initStatus.credentialsReady;
  state.filesystemReady = initStatus.filesystemReady;

  if (!state.credentialsReady) {
    Serial.println("[APP] credentials store unavailable");
  }
  if (!state.filesystemReady) {
    Serial.println("[APP] LittleFS unavailable");
  }
}

void initializeTemplateStore(const RuntimeContext& context, RuntimeState& state) {
  const infra::TemplateStoreInitStatus initStatus = context.templateStore.begin();
  applyTemplateStoreStatus(context, state, initStatus, initStatus.ready);
  persistStorageAux(context, state);

  if (!state.templateStoreReady) {
    Serial.println("[APP] template store unavailable");
  }
}

void loadPersistedState(const RuntimeContext& context, RuntimeState& state) {
  infra::StoredRuntimeState stored = context.localStore.load();
  state.credentials = std::move(stored.credentials);
  state.snapshots = std::move(stored.snapshots);
  state.pendingAttendanceRecords = std::move(stored.pendingAttendanceRecords);
  state.failureLogs = std::move(stored.failureLogs);
  state.storageAux = std::move(stored.storageAux);
}

void persistSnapshots(const RuntimeContext& context, const RuntimeState& state) {
  if (!state.filesystemReady) {
    return;
  }
  context.localStore.saveSnapshots(state.snapshots);
}

void persistPendingAttendanceRecords(const RuntimeContext& context, const RuntimeState& state) {
  if (!state.filesystemReady) {
    return;
  }
  context.localStore.savePendingAttendanceRecords(state.pendingAttendanceRecords);
}

void persistFailureLogs(const RuntimeContext& context, const RuntimeState& state) {
  if (!state.filesystemReady) {
    return;
  }
  context.localStore.saveFailureLogs(state.failureLogs);
}

void persistStorageAux(const RuntimeContext& context, const RuntimeState& state) {
  if (!state.filesystemReady) {
    return;
  }
  context.localStore.saveStorageAux(state.storageAux);
}

void appendApiFailureLog(
    const RuntimeContext& context,
    RuntimeState& state,
    const char* api,
    uint64_t occurredAt,
    const core::ApiError& error,
    std::optional<std::string> relatedId) {
  core::appendFailureLog(
      state.failureLogs,
      core::FailureLogEntry{
          .id = core::buildFailureLogId(occurredAt, state.failureLogs.size()),
          .api = api,
          .occurredAt = occurredAt,
          .code = error.code,
          .message = error.message,
          .relatedId = std::move(relatedId),
      });
  persistFailureLogs(context, state);
}

void markUploadAttemptFailure(
    const RuntimeContext& context,
    RuntimeState& state,
    const std::vector<core::PendingAttendanceRecord>& batch,
    uint64_t occurredAt,
    const std::string& errorCode) {
  for (const auto& submitted : batch) {
    auto it = std::find_if(
        state.pendingAttendanceRecords.begin(),
        state.pendingAttendanceRecords.end(),
        [&](const core::PendingAttendanceRecord& item) { return item.clientRecordId == submitted.clientRecordId; });
    if (it == state.pendingAttendanceRecords.end()) {
      continue;
    }

    it->lastAttemptAt = occurredAt;
    it->lastResultCode = errorCode;
  }

  persistPendingAttendanceRecords(context, state);
}

void applyTemplateStoreStatus(
    const RuntimeContext& context,
    RuntimeState& state,
    const infra::TemplateStoreStatus& status,
    bool replaceSummary) {
  state.templateStoreReady = status.ready;
  state.storageAux.templateStoreHealth = status.health;
  if (replaceSummary) {
    state.storageAux.templateLibrarySummary = status.summary;
  }
  state.faceModuleEnabled = state.templateStoreReady && facePortsReady(context);
}

void runTemplateStoreSelfTest(const RuntimeContext& context, RuntimeState& state) {
#ifdef HITOMI_SD_SELF_TEST
  if (!state.templateStoreReady) {
    return;
  }

  const std::vector<uint8_t> smokeBytes = {0x48, 0x49, 0x54, 0x4F, 0x4D, 0x49};
  if (!context.templateStore.upsertTemplate("__smoke__", smokeBytes, millis())) {
    Serial.println("[APP] SD self-test write failed");
    applyTemplateStoreStatus(context, state, context.templateStore.status(), false);
    persistStorageAux(context, state);
    return;
  }

  auto smokeBlob = context.templateStore.readTemplate("__smoke__");
  if (!smokeBlob.has_value() || smokeBlob->bytes != smokeBytes) {
    Serial.println("[APP] SD self-test read failed");
    applyTemplateStoreStatus(context, state, context.templateStore.status(), false);
    persistStorageAux(context, state);
    return;
  }

  if (!context.templateStore.removeTemplate("__smoke__")) {
    Serial.println("[APP] SD self-test delete failed");
    applyTemplateStoreStatus(context, state, context.templateStore.status(), false);
    persistStorageAux(context, state);
    return;
  }

  applyTemplateStoreStatus(context, state, context.templateStore.status(), true);
  persistStorageAux(context, state);
#endif
}

void probeTemplateStore(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs) {
  if (state.templateStoreReady) {
    return;
  }
  if (nowMs - state.lastTemplateStoreProbeMs < board::kTemplateStoreProbeIntervalMs) {
    return;
  }

  state.lastTemplateStoreProbeMs = nowMs;
  const infra::TemplateStoreInitStatus initStatus = context.templateStore.begin();
  applyTemplateStoreStatus(context, state, initStatus, initStatus.ready);
  persistStorageAux(context, state);
  state.renderDirty = true;
}

}  // namespace app
