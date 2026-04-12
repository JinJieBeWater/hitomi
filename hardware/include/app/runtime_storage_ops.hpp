#pragma once

#include <cstdint>
#include <optional>

#include "app/runtime_context.hpp"
#include "app/runtime_state.hpp"
#include "infra/template_store_port.hpp"

namespace app {

void initializeLocalStore(const RuntimeContext& context, RuntimeState& state);
void initializeTemplateStore(const RuntimeContext& context, RuntimeState& state);
void loadPersistedState(const RuntimeContext& context, RuntimeState& state);
void persistDeviceConfig(const RuntimeContext& context, const RuntimeState& state);
void persistSnapshots(const RuntimeContext& context, const RuntimeState& state);
void persistPendingEnrollmentReports(const RuntimeContext& context, const RuntimeState& state);
void persistPendingAttendanceRecords(const RuntimeContext& context, const RuntimeState& state);
void persistFailureLogs(const RuntimeContext& context, const RuntimeState& state);
void persistStorageAux(const RuntimeContext& context, const RuntimeState& state);
void appendApiFailureLog(
    const RuntimeContext& context,
    RuntimeState& state,
    const char* api,
    uint64_t occurredAt,
    const core::ApiError& error,
    std::optional<std::string> relatedId);
void markUploadAttemptFailure(
    const RuntimeContext& context,
    RuntimeState& state,
    const std::vector<core::PendingAttendanceRecord>& batch,
    uint64_t occurredAt,
    const std::string& errorCode);
void applyTemplateStoreStatus(
    const RuntimeContext& context,
    RuntimeState& state,
    const infra::TemplateStoreStatus& status,
    bool replaceSummary);
void runTemplateStoreSelfTest(const RuntimeContext& context, RuntimeState& state);
void probeTemplateStore(const RuntimeContext& context, RuntimeState& state, uint32_t nowMs);

}  // namespace app
