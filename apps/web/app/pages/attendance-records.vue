<script setup lang="ts">
import { useQuery, useQueryClient } from "@tanstack/vue-query";
import { computed, ref } from "vue";

import { usePagedListState } from "~/composables/usePagedListState";
import { colorAttendanceType, formatDateTime, labelAttendanceType } from "~/utils/format";
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
      },
    }),
  ),
);

const rows = computed(() => recordsQuery.data.value?.items ?? []);
const hasActiveFilters = computed(() =>
  Boolean(localDate.value || employeeId.value || deviceId.value || type.value),
);
const { total, resetPage } = usePagedListState({
  page,
  pageSize,
  getPageInfo: () => recordsQuery.data.value?.pageInfo,
  resetOn: [localDate, employeeId, deviceId, type],
});

const metrics = computed(() => {
  const list = rows.value;
  const employeeCount = new Set(list.map((item) => item.employeeId)).size;

  return [
    {
      label: "记录总数",
      value: total.value,
      caption: "当前筛选",
      icon: "i-lucide-clipboard-check",
      color: "primary" as const,
    },
    {
      label: "上班记录",
      value: list.filter((item) => item.type === "clock_in").length,
      caption: "当前页",
      icon: "i-lucide-sunrise",
      color: "warning" as const,
    },
    {
      label: "下班记录",
      value: list.filter((item) => item.type === "clock_out").length,
      caption: "当前页",
      icon: "i-lucide-sunset",
      color: "success" as const,
    },
    {
      label: "涉及员工",
      value: employeeCount,
      caption: "当前页",
      icon: "i-lucide-users",
      color: "neutral" as const,
    },
  ];
});

const recordColumns = [
  { accessorKey: "localDate", header: "日期" },
  { accessorKey: "recognizedAt", header: "打卡时间" },
  { accessorKey: "type", header: "类型" },
  { accessorKey: "employeeCode", header: "员工编号" },
  { accessorKey: "employeeName", header: "员工姓名" },
  { accessorKey: "deviceName", header: "设备名称" },
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
    list.push({ label: `类型: ${label}`, color: "warning" });
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
        <MetricStrip :metrics="metrics" />

        <FilterBar>
          <div class="grid gap-3 md:grid-cols-2 xl:grid-cols-4">
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

            <UFormField label="类型">
              <USelect
                v-model="type"
                data-testid="attendance-record-type-select"
                :items="attendanceTypeOptions"
                placeholder="全部类型"
                class="w-full"
              />
            </UFormField>
          </div>

          <template #actions>
            <UButton
              variant="outline"
              color="neutral"
              icon="i-lucide-rotate-ccw"
              :disabled="!hasActiveFilters"
              @click="resetFilters()"
              >清空筛选</UButton
            >
          </template>
        </FilterBar>

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
                <template #recognizedAt-cell="{ row }">
                  <div class="text-sm text-toned">
                    {{ formatDateTime(row.original.recognizedAt) }}
                  </div>
                </template>

                <template #type-cell="{ row }">
                  <UBadge
                    :label="labelAttendanceType(row.original.type)"
                    :color="colorAttendanceType(row.original.type)"
                    variant="soft"
                  />
                </template>

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

                <template #deviceName-cell="{ row }">
                  <div class="font-medium text-highlighted">
                    {{ row.original.device?.name || "-" }}
                  </div>
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

                  <UBadge
                    :label="labelAttendanceType(item.type)"
                    :color="colorAttendanceType(item.type)"
                    variant="soft"
                  />
                </div>

                <div class="mt-4 grid gap-3 text-sm sm:grid-cols-2">
                  <div>
                    <div class="workspace-section-label">打卡时间</div>
                    <div class="workspace-data-value">{{ formatDateTime(item.recognizedAt) }}</div>
                  </div>

                  <div>
                    <div class="workspace-section-label">设备</div>
                    <div class="workspace-data-value">{{ item.device?.name || "-" }}</div>
                    <div class="text-xs text-toned">{{ item.device?.deviceCode || "-" }}</div>
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
