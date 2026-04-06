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
const route = useRoute();
const pageSize = 20;
const optionPageSize = 100;

const page = ref(1);
const keyword = ref("");
const faceProfileState = ref<"pending" | "success" | "failed" | "cancelled" | "none" | undefined>();

const employeesQuery = useQuery(
  computed(() =>
    $orpc.employee.list.queryOptions({
      input: {
        page: page.value,
        pageSize,
        keyword: keyword.value || undefined,
        faceProfileState: faceProfileState.value,
      },
    }),
  ),
);

const activeDevicesQuery = useQuery({
  queryKey: ["employee-face-device-options"],
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

const rows = computed(() => employeesQuery.data.value?.items ?? []);
const { total, resetPage } = usePagedListState({
  page,
  pageSize,
  getPageInfo: () => employeesQuery.data.value?.pageInfo,
  resetOn: [keyword, faceProfileState],
});

const editorOpen = ref(false);
const editingEmployee = ref<{ id: string; code: string; name: string } | null>(null);

function openCreate() {
  editingEmployee.value = null;
  editorOpen.value = true;
}

function startEdit(item: any) {
  editingEmployee.value = { id: item.id, code: item.code, name: item.name };
  editorOpen.value = true;
}

const faceTaskOpen = ref(false);
const faceTaskEmployeeId = ref<string | null>(null);

const currentFaceEmployee = computed(
  () => rows.value.find((item) => item.id === faceTaskEmployeeId.value) ?? null,
);

const activeDeviceOptions = computed(() =>
  (activeDevicesQuery.data.value ?? []).map((item) => ({
    label: item.name,
    value: item.id,
    description: item.deviceCode,
  })),
);

function openFaceTask(item: any) {
  faceTaskEmployeeId.value = item.id;
  faceTaskOpen.value = true;
}

function clearFaceRouteState() {
  if (typeof route.query.manageFace !== "string") return;

  const query = { ...route.query };
  delete query.manageFace;
  navigateTo({ path: route.path, query }, { replace: true });
}

const deleteEmployeeOpen = ref(false);
const deleteEmployeeLoading = ref(false);
const deleteEmployeeError = ref("");
const deleteEmployeeImpact = ref<{
  id: string;
  code: string;
  name: string;
  faceProfileCount: number;
  attendanceRecordCount: number;
  confirmText: string;
} | null>(null);
const removeEmployee = useMutation($orpc.employee.remove.mutationOptions());

watch(deleteEmployeeOpen, (open) => {
  if (!open) {
    deleteEmployeeLoading.value = false;
    deleteEmployeeError.value = "";
    deleteEmployeeImpact.value = null;
  }
});

function getEmployeeDeleteErrorMessage(error: any, fallback = "删除员工失败，请稍后重试") {
  const businessCode = error?.data?.businessCode;

  if (businessCode === "EMPLOYEE_NOT_FOUND") return "员工不存在或已被删除";
  if (businessCode === "EMPLOYEE_DELETE_CONFIRMATION_MISMATCH") return "员工编号不匹配，请重新确认";

  return fallback;
}

async function openDeleteEmployee(item: { id: string } | null | undefined) {
  if (!item?.id) return;

  deleteEmployeeOpen.value = true;
  deleteEmployeeLoading.value = true;
  deleteEmployeeError.value = "";
  deleteEmployeeImpact.value = null;

  try {
    deleteEmployeeImpact.value = await queryClient.fetchQuery(
      $orpc.employee.getDeleteImpact.queryOptions({ input: { id: item.id } }),
    );
  } catch (error: any) {
    deleteEmployeeError.value = getEmployeeDeleteErrorMessage(error, "无法加载删除信息");
  } finally {
    deleteEmployeeLoading.value = false;
  }
}

async function handleDeleteEmployee(confirmText: string) {
  if (!deleteEmployeeImpact.value) return;

  deleteEmployeeError.value = "";

  try {
    const result = await removeEmployee.mutateAsync({
      id: deleteEmployeeImpact.value.id,
      confirmText,
    });

    await queryClient.invalidateQueries();
    deleteEmployeeOpen.value = false;

    if (editingEmployee.value?.id === result.id) editorOpen.value = false;
    if (faceTaskEmployeeId.value === result.id) faceTaskOpen.value = false;

    toast.add({
      title: "员工已删除",
      description: `已同时删除 ${result.deletedFaceProfileCount} 条录脸任务、${result.deletedAttendanceRecordCount} 条考勤记录`,
    });
  } catch (error: any) {
    deleteEmployeeError.value = getEmployeeDeleteErrorMessage(error);
  }
}

const employeeColumns = [
  { accessorKey: "code", header: "员工编号" },
  { accessorKey: "name", header: "员工姓名" },
  { accessorKey: "faceTask", header: "录脸任务" },
  { accessorKey: "updatedAt", header: "更新时间" },
  { accessorKey: "actions", header: "" },
];

const faceProfileStateOptions = [
  { label: "未录入", value: "none", description: "还没有建立录脸档案" },
  { label: "待录入", value: "pending", description: "已分配设备，等待完成录入" },
  { label: "录入成功", value: "success", description: "录脸数据已可用于识别" },
  { label: "录入失败", value: "failed", description: "设备端录入失败" },
  { label: "已取消", value: "cancelled", description: "任务已被取消，不再继续" },
];

const headerBadges = computed(() => {
  const list: Array<{ label: string; color: "primary" | "warning" }> = [];

  if (keyword.value) {
    list.push({ label: `关键词: ${keyword.value}`, color: "primary" as const });
  }

  if (faceProfileState.value) {
    const label =
      faceProfileStateOptions.find((item) => item.value === faceProfileState.value)?.label ||
      faceProfileState.value;
    list.push({ label: `状态: ${label}`, color: "warning" as const });
  }

  return list;
});

function resetFilters() {
  resetPage();
  keyword.value = "";
  faceProfileState.value = undefined;
}

function getRowActions(item: any) {
  return [
    [
      {
        label: "录脸",
        icon: "i-lucide-scan-face",
        onSelect: () => openFaceTask(item),
      },
      {
        label: "编辑",
        icon: "i-lucide-pencil-line",
        onSelect: () => startEdit(item),
      },
    ],
    [
      {
        label: "删除",
        icon: "i-lucide-trash-2",
        color: "error" as const,
        onSelect: () => openDeleteEmployee(item),
      },
    ],
  ];
}

watch(
  () => route.query.keyword,
  (value) => {
    if (typeof value === "string" && value !== keyword.value) {
      keyword.value = value;
    }
  },
  { immediate: true },
);

watch(
  [rows, () => route.query.manageFace],
  ([items, manageFace]) => {
    if (typeof manageFace !== "string" || faceTaskOpen.value) return;

    const target = items.find((item) => item.id === manageFace);
    if (!target) return;

    openFaceTask(target);
    clearFaceRouteState();
  },
  { immediate: true },
);
</script>

<template>
  <UDashboardPanel id="employees">
    <template #header>
      <PageHeader title="员工管理" :badges="headerBadges">
        <template #actions>
          <UButton variant="outline" icon="i-lucide-refresh-cw" @click="employeesQuery.refetch()"
            >刷新</UButton
          >
          <UButton icon="i-lucide-plus" class="rounded-2xl" @click="openCreate()">新增员工</UButton>
        </template>
      </PageHeader>
    </template>

    <template #body>
      <div class="workspace-page-stack">
        <FilterBar>
          <div class="grid gap-3 md:grid-cols-2 xl:grid-cols-[minmax(0,1.3fr)_minmax(0,1fr)]">
            <UFormField label="关键词">
              <UInput
                v-model="keyword"
                placeholder="编号或姓名"
                icon="i-lucide-search"
                class="w-full"
              />
            </UFormField>

            <UFormField label="录脸状态">
              <USelect
                v-model="faceProfileState"
                :items="faceProfileStateOptions"
                placeholder="全部状态"
                class="w-full"
              />
            </UFormField>
          </div>

          <template #actions>
            <UButton
              variant="ghost"
              color="neutral"
              icon="i-lucide-rotate-ccw"
              @click="resetFilters()"
              >清空筛选</UButton
            >
          </template>
        </FilterBar>

        <DataSurface title="员工列表">
          <QueryGuard
            :status="employeesQuery.status.value"
            :error="employeesQuery.error.value?.message"
            :empty="rows.length === 0"
            empty-title="暂无员工"
            empty-description="当前筛选条件下没有可显示的员工记录。"
          >
            <template #empty-actions>
              <UButton icon="i-lucide-plus" class="rounded-2xl" @click="openCreate()"
                >新增员工</UButton
              >
            </template>

            <div class="workspace-surface-table hidden md:block">
              <UTable
                :data="rows"
                :columns="employeeColumns"
                empty="暂无员工数据"
                :ui="{ root: 'w-full overflow-x-auto', base: 'w-full min-w-[720px]' }"
              >
                <template #faceTask-cell="{ row }">
                  <div class="space-y-2">
                    <UBadge
                      :label="labelFaceStatus(row.original.faceProfile?.status)"
                      :color="colorFaceStatus(row.original.faceProfile?.status)"
                      variant="subtle"
                      class="rounded-full"
                    />

                    <div class="text-sm text-toned">
                      {{ row.original.faceProfile?.deviceName || "未分配设备" }}
                    </div>
                  </div>
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
                      {{ item.name }}
                    </div>
                    <div class="text-sm text-toned">{{ item.code }}</div>
                  </div>

                  <div class="flex items-center gap-2">
                    <UBadge
                      :label="labelFaceStatus(item.faceProfile?.status)"
                      :color="colorFaceStatus(item.faceProfile?.status)"
                      variant="subtle"
                      class="rounded-full"
                    />
                    <RowActions :items="getRowActions(item)" trigger-size="sm" />
                  </div>
                </div>

                <div class="mt-4 flex items-start justify-between gap-4">
                  <div>
                    <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">
                      录脸任务
                    </div>
                    <div class="mt-1 text-highlighted">
                      {{ item.faceProfile?.deviceName || "未分配设备" }}
                    </div>
                  </div>

                  <div class="text-right">
                    <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">
                      更新时间
                    </div>
                    <div class="mt-1 text-highlighted">{{ formatDateTime(item.updatedAt) }}</div>
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

      <EmployeeSlideoverEditor
        :open="editorOpen"
        :employee="editingEmployee"
        @update:open="editorOpen = $event"
        @request-delete="openDeleteEmployee(editingEmployee)"
      />

      <EmployeeFaceTaskSlideover
        :open="faceTaskOpen"
        :employee="currentFaceEmployee"
        :device-options="activeDeviceOptions"
        :device-options-loading="activeDevicesQuery.isPending.value"
        @update:open="faceTaskOpen = $event"
      />

      <DeleteConfirmModal
        :open="deleteEmployeeOpen"
        entity-label="员工"
        identifier-label="员工编号"
        :impact="
          deleteEmployeeImpact
            ? {
                name: deleteEmployeeImpact.name,
                identifierValue: deleteEmployeeImpact.code,
                faceProfileCount: deleteEmployeeImpact.faceProfileCount,
                attendanceRecordCount: deleteEmployeeImpact.attendanceRecordCount,
                confirmText: deleteEmployeeImpact.confirmText,
              }
            : null
        "
        :loading="deleteEmployeeLoading"
        :submitting="removeEmployee.isPending.value"
        :error="deleteEmployeeError"
        @update:open="deleteEmployeeOpen = $event"
        @confirm="handleDeleteEmployee($event)"
      />
    </template>
  </UDashboardPanel>
</template>
