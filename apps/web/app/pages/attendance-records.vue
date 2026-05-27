<script setup lang="ts">
import { useQuery, useQueryClient } from "@tanstack/vue-query";
import { computed, ref } from "vue";

import { usePagedListState } from "~/composables/usePagedListState";
import { formatDateTime } from "~/utils/format";
import { fetchAllPages } from "~/utils/pagination";

definePageMeta({
  layout: "dashboard",
  middleware: ["auth"],
});

const { $orpc } = useNuxtApp();
const queryClient = useQueryClient();
const pageSize = 20;
const optionPageSize = 100;

const page = ref(1);
const localDate = ref("");
const employeeId = ref<string | undefined>();
const deviceId = ref<string | undefined>();
const type = ref<"clock_in" | "clock_out" | undefined>();
const slotStatus = ref<"recorded" | "missing" | undefined>();

const employeesQuery = useQuery({
  queryKey: ["attendance-record-employee-options"],
  queryFn: () =>
    fetchAllPages((currentPage) =>
      queryClient.fetchQuery(
        $orpc.employee.list.queryOptions({
          input: {
            page: currentPage,
            pageSize: optionPageSize,
          },
        }),
      ),
    ),
});

const devicesQuery = useQuery({
  queryKey: ["attendance-record-device-options"],
  queryFn: () =>
    fetchAllPages((currentPage) =>
      queryClient.fetchQuery(
        $orpc.device.list.queryOptions({
          input: {
            page: currentPage,
            pageSize: optionPageSize,
          },
        }),
      ),
    ),
});

const recordsQuery = useQuery(
  computed(() =>
    $orpc.attendanceRecord.list.queryOptions({
      input: {
        page: page.value,
        pageSize,
        localDate: localDate.value || undefined,
        employeeId: employeeId.value,
        deviceId: deviceId.value,
        type: type.value,
        slotStatus: slotStatus.value,
      },
    }),
  ),
);

const rows = computed(() => recordsQuery.data.value?.items ?? []);
const hasActiveFilters = computed(() =>
  Boolean(localDate.value || employeeId.value || deviceId.value || type.value || slotStatus.value),
);
const { total, resetPage } = usePagedListState({
  page,
  pageSize,
  getPageInfo: () => recordsQuery.data.value?.pageInfo,
  resetOn: [localDate, employeeId, deviceId, type, slotStatus],
});

function countStatus(item: (typeof rows.value)[number], status: "recorded" | "missing") {
  if (type.value === "clock_in") return item.clockInStatus === status ? 1 : 0;
  if (type.value === "clock_out") return item.clockOutStatus === status ? 1 : 0;

  return Number(item.clockInStatus === status) + Number(item.clockOutStatus === status);
}

const metrics = computed(() => {
  const list = rows.value;

  return [
    {
      label: "日记录",
      value: total.value,
      caption: "当前筛选",
      icon: "i-lucide-clipboard-check",
      color: "primary" as const,
    },
    {
      label: "正常打卡",
      value: list.reduce((sum, item) => sum + countStatus(item, "recorded"), 0),
      caption: "当前页",
      icon: "i-lucide-circle-check",
      color: "success" as const,
    },
    {
      label: "缺卡",
      value: list.reduce((sum, item) => sum + countStatus(item, "missing"), 0),
      caption: "当前页",
      icon: "i-lucide-circle-alert",
      color: "error" as const,
    },
  ];
});

const recordColumns = [
  { accessorKey: "localDate", header: "日期" },
  { accessorKey: "employeeCode", header: "员工编号" },
  { accessorKey: "employeeName", header: "员工姓名" },
  { accessorKey: "clockIn", header: "上班" },
  { accessorKey: "clockOut", header: "下班" },
];

const employeeOptions = computed(() =>
  (employeesQuery.data.value ?? []).map((item) => ({
    label: item.name,
    value: item.id,
    description: item.code,
  })),
);

const deviceOptions = computed(() =>
  (devicesQuery.data.value ?? []).map((item) => ({
    label: item.name,
    value: item.id,
    description: item.deviceCode,
  })),
);

const attendanceTypeOptions = [
  { label: "上班", value: "clock_in", description: "按照上班时间窗口归类" },
  { label: "下班", value: "clock_out", description: "按照下班时间窗口归类" },
];

const slotStatusOptions = [
  { label: "正常打卡", value: "recorded", description: "已产生有效打卡记录" },
  { label: "缺卡", value: "missing", description: "打卡窗口已结束但没有记录" },
];

function labelAttendanceSlotStatus(value: string | undefined) {
  if (value === "recorded") return "已打卡";
  if (value === "missing") return "缺卡";
  return "-";
}

function colorAttendanceSlotStatus(value: string | undefined) {
  if (value === "recorded") return "success" as const;
  if (value === "missing") return "error" as const;
  return "neutral" as const;
}

const headerBadges = computed(() => {
  const list: Array<{ label: string; color: "primary" | "success" | "warning" | "neutral" }> = [
    { label: `${total.value} 条记录`, color: "neutral" },
  ];

  if (localDate.value) {
    list.push({ label: localDate.value, color: "primary" });
  }

  if (type.value) {
    const label =
      attendanceTypeOptions.find((item) => item.value === type.value)?.label || type.value;
    list.push({ label: `时段: ${label}`, color: "warning" });
  }

  if (slotStatus.value) {
    const label =
      slotStatusOptions.find((item) => item.value === slotStatus.value)?.label || slotStatus.value;
    list.push({
      label: `状态: ${label}`,
      color: slotStatus.value === "missing" ? "warning" : "success",
    });
  }

  if (employeeId.value) {
    const label =
      employeeOptions.value.find((item) => item.value === employeeId.value)?.label || "员工";
    list.push({ label: `员工: ${label}`, color: "primary" });
  }

  if (deviceId.value) {
    const label =
      deviceOptions.value.find((item) => item.value === deviceId.value)?.label || "设备";
    list.push({ label: `设备: ${label}`, color: "success" });
  }

  return list;
});

function resetFilters() {
  resetPage();
  localDate.value = "";
  employeeId.value = undefined;
  deviceId.value = undefined;
  type.value = undefined;
  slotStatus.value = undefined;
}
</script>

<template>
  <UDashboardPanel id="attendance-records">
    <template #header>
      <PageHeader title="考勤记录" :badges="headerBadges">
        <template #actions>
          <UButton variant="outline" icon="i-lucide-refresh-cw" @click="recordsQuery.refetch()"
            >刷新</UButton
          >
        </template>
      </PageHeader>
    </template>

    <template #body>
      <div class="workspace-page-stack">
        <UCard>
          <div class="space-y-5">
            <div class="flex flex-col gap-4 xl:flex-row xl:items-end xl:justify-between">
              <div class="grid min-w-0 flex-1 gap-3 md:grid-cols-2 xl:grid-cols-5">
                <UFormField label="日期">
                  <UInput
                    v-model="localDate"
                    data-testid="attendance-record-date-input"
                    type="date"
                    class="w-full"
                  />
                </UFormField>

                <UFormField label="员工">
                  <USelect
                    v-model="employeeId"
                    data-testid="attendance-record-employee-select"
                    :items="employeeOptions"
                    placeholder="全部员工"
                    class="w-full"
                  />
                </UFormField>

                <UFormField label="设备">
                  <USelect
                    v-model="deviceId"
                    data-testid="attendance-record-device-select"
                    :items="deviceOptions"
                    placeholder="全部设备"
                    class="w-full"
                  />
                </UFormField>

                <UFormField label="时段">
                  <USelect
                    v-model="type"
                    data-testid="attendance-record-type-select"
                    :items="attendanceTypeOptions"
                    placeholder="全部时段"
                    class="w-full"
                  />
                </UFormField>

                <UFormField label="状态">
                  <USelect
                    v-model="slotStatus"
                    data-testid="attendance-record-status-select"
                    :items="slotStatusOptions"
                    placeholder="全部状态"
                    class="w-full"
                  />
                </UFormField>
              </div>

              <UButton
                class="shrink-0"
                variant="outline"
                color="neutral"
                icon="i-lucide-rotate-ccw"
                :disabled="!hasActiveFilters"
                @click="resetFilters()"
                >清空筛选</UButton
              >
            </div>

            <div class="border-t border-default pt-5">
              <div class="grid gap-3 md:grid-cols-3">
                <div
                  v-for="item in metrics"
                  :key="item.label"
                  class="rounded-lg border border-default bg-muted/30 p-4"
                >
                  <div class="flex items-start justify-between gap-4">
                    <div class="min-w-0 space-y-1">
                      <div class="workspace-metric-label">{{ item.label }}</div>
                      <div class="workspace-metric-value">{{ item.value }}</div>
                      <div class="workspace-metric-meta">{{ item.caption }}</div>
                    </div>

                    <UBadge v-if="item.icon" :color="item.color" variant="soft" size="lg" square>
                      <UIcon :name="item.icon" class="size-5" />
                    </UBadge>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </UCard>

        <DataSurface title="考勤记录列表">
          <QueryGuard
            :status="recordsQuery.status.value"
            :error="recordsQuery.error.value?.message"
            :empty="rows.length === 0"
            empty-title="暂无考勤记录"
            empty-description="当前筛选条件下没有可显示的考勤记录。"
          >
            <div class="workspace-surface-table workspace-table-shell hidden md:block">
              <UTable
                :data="rows"
                :columns="recordColumns"
                empty="暂无考勤记录"
                :ui="{ root: 'w-full overflow-x-auto', base: 'w-full min-w-[820px]' }"
              >
                <template #employeeCode-cell="{ row }">
                  <div class="workspace-code-value mt-0">
                    {{ row.original.employee?.code || "-" }}
                  </div>
                </template>

                <template #employeeName-cell="{ row }">
                  <div class="font-medium text-highlighted">
                    {{ row.original.employee?.name || "-" }}
                  </div>
                </template>

                <template #clockIn-cell="{ row }">
                  <div v-if="row.original.clockIn" class="space-y-1">
                    <div class="text-sm font-medium text-highlighted">
                      {{ formatDateTime(row.original.clockIn.recognizedAt) }}
                    </div>
                    <div class="text-xs text-toned">
                      {{ row.original.clockIn.device?.name || "-" }}
                    </div>
                  </div>
                  <UBadge
                    v-else-if="row.original.clockInStatus"
                    :label="labelAttendanceSlotStatus(row.original.clockInStatus)"
                    :color="colorAttendanceSlotStatus(row.original.clockInStatus)"
                    variant="soft"
                  />
                  <div v-else class="text-sm text-muted">-</div>
                </template>

                <template #clockOut-cell="{ row }">
                  <div v-if="row.original.clockOut" class="space-y-1">
                    <div class="text-sm font-medium text-highlighted">
                      {{ formatDateTime(row.original.clockOut.recognizedAt) }}
                    </div>
                    <div class="text-xs text-toned">
                      {{ row.original.clockOut.device?.name || "-" }}
                    </div>
                  </div>
                  <UBadge
                    v-else-if="row.original.clockOutStatus"
                    :label="labelAttendanceSlotStatus(row.original.clockOutStatus)"
                    :color="colorAttendanceSlotStatus(row.original.clockOutStatus)"
                    variant="soft"
                  />
                  <div v-else class="text-sm text-muted">-</div>
                </template>
              </UTable>
            </div>

            <div class="grid gap-3 md:hidden">
              <div v-for="item in rows" :key="item.id" class="workspace-mobile-card">
                <div class="flex items-start justify-between gap-3">
                  <div class="min-w-0 space-y-1">
                    <div class="workspace-section-label">{{ item.localDate }}</div>
                    <div class="truncate text-base font-semibold text-highlighted">
                      {{ item.employee?.name || "-" }}
                    </div>
                    <div class="workspace-code-value mt-0">{{ item.employee?.code || "-" }}</div>
                  </div>

                  <UBadge label="日汇总" color="neutral" variant="soft" />
                </div>

                <div class="mt-4 grid gap-3 text-sm sm:grid-cols-2">
                  <div>
                    <div class="workspace-section-label">上班</div>
                    <template v-if="item.clockIn">
                      <div class="workspace-data-value">
                        {{ formatDateTime(item.clockIn.recognizedAt) }}
                      </div>
                      <div class="text-xs text-toned">{{ item.clockIn.device?.name || "-" }}</div>
                    </template>
                    <UBadge
                      v-else-if="item.clockInStatus"
                      :label="labelAttendanceSlotStatus(item.clockInStatus)"
                      :color="colorAttendanceSlotStatus(item.clockInStatus)"
                      variant="soft"
                    />
                    <div v-else class="workspace-data-value">-</div>
                  </div>

                  <div>
                    <div class="workspace-section-label">下班</div>
                    <template v-if="item.clockOut">
                      <div class="workspace-data-value">
                        {{ formatDateTime(item.clockOut.recognizedAt) }}
                      </div>
                      <div class="text-xs text-toned">{{ item.clockOut.device?.name || "-" }}</div>
                    </template>
                    <UBadge
                      v-else-if="item.clockOutStatus"
                      :label="labelAttendanceSlotStatus(item.clockOutStatus)"
                      :color="colorAttendanceSlotStatus(item.clockOutStatus)"
                      variant="soft"
                    />
                    <div v-else class="workspace-data-value">-</div>
                  </div>
                </div>
              </div>
            </div>
          </QueryGuard>

          <template #footer>
            <ListPagination
              :page="page"
              :page-size="pageSize"
              :total="total"
              @update:page="page = $event"
            />
          </template>
        </DataSurface>
      </div>
    </template>
  </UDashboardPanel>
</template>
