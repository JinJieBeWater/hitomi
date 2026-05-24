<script setup lang="ts">
import { useMutation, useQuery, useQueryClient } from "@tanstack/vue-query";
import { computed, ref, watch } from "vue";

import { usePagedListState } from "~/composables/usePagedListState";
import { colorFaceStatus, formatDateTime, labelFaceStatus } from "~/utils/format";
import { fetchAllPages } from "~/utils/pagination";

definePageMeta({
  layout: "dashboard",
  middleware: ["auth"],
});

const { $orpc } = useNuxtApp();
const queryClient = useQueryClient();
const toast = useToast();
const pageSize = 20;

type FaceProfileStatusFilter = "pending" | "success" | "failed" | "cancelled" | undefined;

const page = ref(1);
const status = ref<FaceProfileStatusFilter>();
const employeeId = ref<string | undefined>();
const deviceId = ref<string | undefined>();
const taskModalOpen = ref(false);
const taskEmployeeId = ref<string | undefined>();
const taskDeviceId = ref<string | undefined>();
const taskError = ref("");
const optionPageSize = 100;

const faceProfilesQuery = useQuery(
  computed(() =>
    $orpc.faceProfile.list.queryOptions({
      input: {
        page: page.value,
        pageSize,
        status: status.value,
        employeeId: employeeId.value,
        deviceId: deviceId.value,
      },
    }),
  ),
);

const employeesQuery = useQuery({
  queryKey: ["face-profile-employee-options"],
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

const allDevicesQuery = useQuery({
  queryKey: ["face-profile-all-device-options"],
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

const activeDevicesQuery = useQuery({
  queryKey: ["face-profile-active-device-options"],
  queryFn: () =>
    fetchAllPages((currentPage) =>
      queryClient.fetchQuery(
        $orpc.device.list.queryOptions({
          input: {
            page: currentPage,
            pageSize: optionPageSize,
            status: "active",
          },
        }),
      ),
    ),
});

const cancelTask = useMutation($orpc.faceProfile.cancel.mutationOptions());
const enqueueTask = useMutation($orpc.faceProfile.enqueue.mutationOptions());

const rows = computed(() => faceProfilesQuery.data.value?.items ?? []);
const hasActiveFilters = computed(() =>
  Boolean(status.value || employeeId.value || deviceId.value),
);
const { total, resetPage } = usePagedListState({
  page,
  pageSize,
  getPageInfo: () => faceProfilesQuery.data.value?.pageInfo,
  resetOn: [status, employeeId, deviceId],
});

const employeeOptions = computed(() =>
  (employeesQuery.data.value ?? []).map((item) => ({
    label: `${item.name} · ${item.code}`,
    value: item.id,
    description: item.code,
  })),
);

const deviceOptions = computed(() =>
  (allDevicesQuery.data.value ?? []).map((item) => ({
    label: item.name,
    value: item.id,
    description: item.deviceCode,
  })),
);

const activeDeviceOptions = computed(() =>
  (activeDevicesQuery.data.value ?? []).map((item) => ({
    label: item.name,
    value: item.id,
    description: item.deviceCode,
  })),
);

watch(taskModalOpen, (open) => {
  if (!open) {
    taskEmployeeId.value = undefined;
    taskDeviceId.value = undefined;
    taskError.value = "";
  }
});

const faceProfileColumns = [
  { accessorKey: "employeeCode", header: "员工编号" },
  { accessorKey: "employeeName", header: "员工姓名" },
  { accessorKey: "deviceName", header: "设备名称" },
  { accessorKey: "deviceCode", header: "设备码" },
  { accessorKey: "status", header: "状态" },
  { accessorKey: "createdAt", header: "创建时间" },
  { accessorKey: "updatedAt", header: "更新时间" },
  { accessorKey: "actions", header: "" },
];

const statusOptions = [
  { label: "待录入", value: "pending", description: "等待设备端完成采集" },
  { label: "录入成功", value: "success", description: "录脸已完成，可参与识别" },
  { label: "录入失败", value: "failed", description: "需要人工介入或重试" },
  { label: "已取消", value: "cancelled", description: "任务已停止，不再继续" },
];

const headerBadges = computed(() => {
  const list: Array<{ label: string; color: "primary" | "success" | "warning" | "neutral" }> = [];

  if (status.value) {
    const label = statusOptions.find((item) => item.value === status.value)?.label || status.value;
    list.push({ label: `状态: ${label}`, color: "warning" });
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

async function handleCancel(id: string) {
  try {
    await cancelTask.mutateAsync({ id });
    await queryClient.invalidateQueries();
    toast.add({ title: "录脸任务已取消" });
  } catch (error: any) {
    toast.add({
      title: "取消任务失败",
      description: error?.message || "请稍后重试",
      color: "error",
    });
  }
}

async function handleEnqueue(input: { employeeId: string; deviceId: string }) {
  taskError.value = "";

  try {
    await enqueueTask.mutateAsync(input);
    await queryClient.invalidateQueries();
    taskModalOpen.value = false;
    taskEmployeeId.value = undefined;
    taskDeviceId.value = undefined;
    toast.add({ title: "录脸任务已创建" });
  } catch (error: any) {
    taskError.value = error?.message || "录脸任务创建失败";
    toast.add({
      title: "录脸任务创建失败",
      description: taskError.value,
      color: "error",
    });
  }
}

async function handleCreateTask() {
  if (!taskEmployeeId.value || !taskDeviceId.value) {
    taskError.value = "请选择员工和设备";
    return;
  }

  await handleEnqueue({ employeeId: taskEmployeeId.value, deviceId: taskDeviceId.value });
}

function openTaskModal(input?: { employeeId?: string; deviceId?: string }) {
  taskError.value = "";
  taskEmployeeId.value = input?.employeeId;
  taskDeviceId.value = input?.deviceId;
  taskModalOpen.value = true;
}

async function goToEmployee(item: any) {
  await navigateTo({
    path: "/employees",
    query: {
      keyword: item.employee?.code,
      manageFace: item.employeeId,
    },
  });
}

function getRowActions(item: any) {
  const actions = [
    {
      label: "查看员工",
      icon: "i-lucide-arrow-up-right",
      onSelect: () => goToEmployee(item),
    },
    ...(item.status === "pending"
      ? [
          {
            label: "取消任务",
            icon: "i-lucide-ban",
            color: "error" as const,
            onSelect: () => handleCancel(item.id),
          },
        ]
      : []),
    ...(item.status === "failed" || item.status === "cancelled" || item.status === "success"
      ? [
          {
            label: item.status === "success" ? "重新录脸" : "重新发起",
            icon: "i-lucide-refresh-cw",
            onSelect: () =>
              openTaskModal({
                employeeId: item.employeeId,
                deviceId: item.device?.status === "active" ? item.deviceId : undefined,
              }),
          },
        ]
      : []),
  ];

  return [actions];
}

function resetFilters() {
  resetPage();
  status.value = undefined;
  employeeId.value = undefined;
  deviceId.value = undefined;
}
</script>

<template>
  <UDashboardPanel id="face-profiles">
    <template #header>
      <PageHeader title="录脸记录" :badges="headerBadges">
        <template #actions>
          <UButton icon="i-lucide-plus" @click="openTaskModal()">新建任务</UButton>
          <UButton variant="outline" icon="i-lucide-refresh-cw" @click="faceProfilesQuery.refetch()"
            >刷新</UButton
          >
        </template>
      </PageHeader>
    </template>

    <template #body>
      <div class="workspace-page-stack">
        <FilterBar>
          <div class="grid gap-3 md:grid-cols-3 xl:grid-cols-3">
            <UFormField label="状态">
              <USelect
                v-model="status"
                :items="statusOptions"
                placeholder="全部状态"
                class="w-full"
              />
            </UFormField>

            <UFormField label="员工">
              <USelect
                v-model="employeeId"
                :items="employeeOptions"
                :loading="employeesQuery.isPending.value"
                placeholder="全部员工"
                class="w-full"
              />
            </UFormField>

            <UFormField label="设备">
              <USelect
                v-model="deviceId"
                :items="deviceOptions"
                :loading="allDevicesQuery.isPending.value"
                placeholder="全部设备"
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

        <DataSurface title="录脸记录列表">
          <QueryGuard
            :status="faceProfilesQuery.status.value"
            :error="faceProfilesQuery.error.value?.message"
            :empty="rows.length === 0"
            empty-title="暂无录脸记录"
            empty-description="暂无匹配的录脸记录。"
          >
            <div class="workspace-surface-table workspace-table-shell hidden md:block">
              <UTable
                :data="rows"
                :columns="faceProfileColumns"
                empty="暂无录脸记录"
                :ui="{ root: 'w-full overflow-x-auto', base: 'w-full min-w-[1040px]' }"
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

                <template #deviceName-cell="{ row }">
                  <div class="font-medium text-highlighted">
                    {{ row.original.device?.name || "-" }}
                  </div>
                </template>

                <template #deviceCode-cell="{ row }">
                  <div class="workspace-code-value mt-0">
                    {{ row.original.device?.deviceCode || "-" }}
                  </div>
                </template>

                <template #status-cell="{ row }">
                  <UBadge
                    :label="labelFaceStatus(row.original.status)"
                    :color="colorFaceStatus(row.original.status)"
                    variant="soft"
                  />
                </template>

                <template #createdAt-cell="{ row }">
                  <div class="text-sm text-toned">{{ formatDateTime(row.original.createdAt) }}</div>
                </template>

                <template #updatedAt-cell="{ row }">
                  <div class="text-sm text-toned">{{ formatDateTime(row.original.updatedAt) }}</div>
                </template>

                <template #actions-cell="{ row }">
                  <RowActions :items="getRowActions(row.original)" />
                </template>
              </UTable>
            </div>

            <div class="grid gap-3 md:hidden">
              <div v-for="item in rows" :key="item.id" class="workspace-mobile-card">
                <div class="flex items-start justify-between gap-3">
                  <div class="min-w-0 space-y-1">
                    <div class="truncate text-base font-semibold text-highlighted">
                      {{ item.employee?.name || "-" }}
                    </div>
                    <div class="workspace-code-value mt-0">{{ item.employee?.code || "-" }}</div>
                  </div>

                  <div class="flex items-center gap-2">
                    <UBadge
                      :label="labelFaceStatus(item.status)"
                      :color="colorFaceStatus(item.status)"
                      variant="soft"
                    />
                    <RowActions :items="getRowActions(item)" trigger-size="sm" />
                  </div>
                </div>

                <div class="mt-4 grid gap-3 text-sm sm:grid-cols-2">
                  <div>
                    <div class="workspace-section-label">设备</div>
                    <div class="workspace-data-value">{{ item.device?.name || "-" }}</div>
                    <div class="text-xs text-toned">{{ item.device?.deviceCode || "-" }}</div>
                  </div>

                  <div>
                    <div class="workspace-section-label">创建时间</div>
                    <div class="workspace-data-value">{{ formatDateTime(item.createdAt) }}</div>
                  </div>

                  <div>
                    <div class="workspace-section-label">更新时间</div>
                    <div class="workspace-data-value">{{ formatDateTime(item.updatedAt) }}</div>
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

      <UModal
        :open="taskModalOpen"
        title="新建录脸任务"
        description="选择员工和启用中的设备，设备端同步后会进入待录入状态。"
        @update:open="taskModalOpen = $event"
      >
        <template #body>
          <div class="flex flex-col gap-4">
            <UFormField label="员工" required>
              <USelect
                v-model="taskEmployeeId"
                :items="employeeOptions"
                :loading="employeesQuery.isPending.value"
                placeholder="请选择员工"
                class="w-full"
              />
            </UFormField>

            <UFormField label="设备" required>
              <USelect
                v-model="taskDeviceId"
                :items="activeDeviceOptions"
                :loading="activeDevicesQuery.isPending.value"
                placeholder="请选择设备"
                class="w-full"
              />
            </UFormField>

            <UAlert
              v-if="!activeDevicesQuery.isPending.value && activeDeviceOptions.length === 0"
              color="warning"
              icon="i-lucide-triangle-alert"
              title="暂无启用设备"
              description="请先在设备管理页启用至少一台设备。"
            />

            <UAlert
              v-if="taskError"
              color="error"
              icon="i-lucide-alert-circle"
              title="操作失败"
              :description="taskError"
            />
          </div>
        </template>

        <template #footer>
          <div class="flex w-full flex-col gap-2 sm:flex-row sm:justify-end">
            <UButton variant="outline" color="neutral" @click="taskModalOpen = false">取消</UButton>
            <UButton
              icon="i-lucide-scan-face"
              :loading="enqueueTask.isPending.value"
              :disabled="activeDeviceOptions.length === 0"
              @click="handleCreateTask()"
            >
              创建任务
            </UButton>
          </div>
        </template>
      </UModal>
    </template>
  </UDashboardPanel>
</template>
