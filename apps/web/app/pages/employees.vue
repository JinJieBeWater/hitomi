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
const editingId = ref<string | null>(null);
const editorOpen = ref(false);
const editorError = ref("");
const faceTaskEmployeeId = ref<string | null>(null);
const faceTaskOpen = ref(false);
const faceTaskError = ref("");
const deleteEmployeeOpen = ref(false);
const deleteEmployeeLoading = ref(false);
const deleteEmployeeError = ref("");
const form = ref({
  code: "",
  name: "",
});
const faceTaskForm = ref({
  deviceId: undefined as string | undefined,
});
const deleteEmployeeImpact = ref<{
  id: string;
  code: string;
  name: string;
  faceProfileCount: number;
  attendanceRecordCount: number;
  confirmText: string;
} | null>(null);

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

const createEmployee = useMutation($orpc.employee.create.mutationOptions());
const updateEmployee = useMutation($orpc.employee.update.mutationOptions());
const assignFaceTask = useMutation($orpc.faceProfile.enqueue.mutationOptions());
const cancelFaceTask = useMutation($orpc.faceProfile.cancel.mutationOptions());
const removeEmployee = useMutation($orpc.employee.remove.mutationOptions());

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

const employeeColumns = [
  { accessorKey: "code", header: "员工编号" },
  { accessorKey: "name", header: "员工姓名" },
  { accessorKey: "faceTask", header: "录脸任务" },
  { accessorKey: "updatedAt", header: "更新时间" },
  { accessorKey: "actions", header: "操作" },
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

const activeDeviceOptions = computed(() =>
  (activeDevicesQuery.data.value ?? []).map((item) => ({
    label: item.name,
    value: item.id,
    description: item.deviceCode,
  })),
);

const currentFaceEmployee = computed(
  () => rows.value.find((item) => item.id === faceTaskEmployeeId.value) ?? null,
);
const currentEditingEmployee = computed(
  () => rows.value.find((item) => item.id === editingId.value) ?? null,
);
const isSavingEmployee = computed(
  () => createEmployee.isPending.value || updateEmployee.isPending.value,
);

const faceTaskActionLabel = computed(() => {
  const status = currentFaceEmployee.value?.faceProfile?.status;

  if (!status || status === "cancelled" || status === "failed") {
    return "创建录脸任务";
  }

  if (status === "pending") {
    return "更新设备分配";
  }

  return "重新发起录脸";
});

const faceTaskTitle = computed(() => {
  if (!currentFaceEmployee.value) {
    return "录脸任务";
  }

  return `${currentFaceEmployee.value.name} · 录脸`;
});

function resetForm() {
  editingId.value = null;
  editorError.value = "";
  form.value = {
    code: "",
    name: "",
  };
}

function closeEditor() {
  editorOpen.value = false;
}

function resetFaceTask() {
  faceTaskEmployeeId.value = null;
  faceTaskError.value = "";
  faceTaskForm.value = {
    deviceId: undefined,
  };
}

function closeFaceTask() {
  faceTaskOpen.value = false;
}

function getEmployeeDeleteErrorMessage(error: any, fallback = "删除员工失败，请稍后重试") {
  const businessCode = error?.data?.businessCode;

  if (businessCode === "EMPLOYEE_NOT_FOUND") {
    return "员工不存在或已被删除";
  }

  if (businessCode === "EMPLOYEE_DELETE_CONFIRMATION_MISMATCH") {
    return "员工编号不匹配，请重新确认";
  }

  return fallback;
}

function openCreate() {
  resetForm();
  editorOpen.value = true;
}

function startEdit(item: any) {
  editingId.value = item.id;
  editorError.value = "";
  form.value = {
    code: item.code,
    name: item.name,
  };
  editorOpen.value = true;
}

watch(editorOpen, (open) => {
  if (!open) {
    resetForm();
  }
});

watch(faceTaskOpen, (open) => {
  if (!open) {
    resetFaceTask();
  }
});

watch(deleteEmployeeOpen, (open) => {
  if (!open) {
    deleteEmployeeLoading.value = false;
    deleteEmployeeError.value = "";
    deleteEmployeeImpact.value = null;
  }
});

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
    if (typeof manageFace !== "string" || faceTaskOpen.value) {
      return;
    }

    const target = items.find((item) => item.id === manageFace);

    if (!target) {
      return;
    }

    openFaceTask(target);
    clearFaceRouteState();
  },
  { immediate: true },
);

async function handleSubmit() {
  editorError.value = "";
  const isEditing = Boolean(editingId.value);

  try {
    if (editingId.value) {
      await updateEmployee.mutateAsync({
        id: editingId.value,
        code: form.value.code.trim(),
        name: form.value.name.trim(),
      });
    } else {
      await createEmployee.mutateAsync({
        code: form.value.code.trim(),
        name: form.value.name.trim(),
      });
    }

    await queryClient.invalidateQueries();
    closeEditor();
    toast.add({
      title: isEditing ? "员工已更新" : "员工已创建",
    });
  } catch (error: any) {
    editorError.value = error?.message || "保存失败";
  }
}

function resetFilters() {
  resetPage();
  keyword.value = "";
  faceProfileState.value = undefined;
}

function clearFaceRouteState() {
  if (typeof route.query.manageFace !== "string") {
    return;
  }

  const query = { ...route.query };
  delete query.manageFace;

  navigateTo({ path: route.path, query }, { replace: true });
}

function openFaceTask(item: any) {
  faceTaskEmployeeId.value = item.id;
  faceTaskError.value = "";
  faceTaskForm.value = {
    deviceId: item.faceProfile?.deviceId || undefined,
  };
  faceTaskOpen.value = true;
}

async function handleFaceTaskSubmit() {
  faceTaskError.value = "";

  if (!currentFaceEmployee.value) {
    return;
  }

  if (!faceTaskForm.value.deviceId) {
    faceTaskError.value = "请选择设备";
    return;
  }

  const employee = currentFaceEmployee.value;
  const hadTask = Boolean(employee.faceProfile);

  try {
    await assignFaceTask.mutateAsync({
      employeeId: employee.id,
      deviceId: faceTaskForm.value.deviceId,
    });

    await queryClient.invalidateQueries();
    closeFaceTask();
    toast.add({
      title: hadTask ? "录脸任务已更新" : "录脸任务已创建",
    });
  } catch (error: any) {
    faceTaskError.value = error?.message || "录脸任务保存失败";
  }
}

async function handleFaceTaskCancel() {
  faceTaskError.value = "";

  const profile = currentFaceEmployee.value?.faceProfile;

  if (!profile) {
    return;
  }

  try {
    await cancelFaceTask.mutateAsync({ id: profile.id });
    await queryClient.invalidateQueries();
    closeFaceTask();
    toast.add({ title: "录脸任务已取消" });
  } catch (error: any) {
    faceTaskError.value = error?.message || "取消录脸任务失败";
  }
}

async function openDeleteEmployee(item: { id: string } | null | undefined) {
  if (!item?.id) {
    return;
  }

  deleteEmployeeOpen.value = true;
  deleteEmployeeLoading.value = true;
  deleteEmployeeError.value = "";
  deleteEmployeeImpact.value = null;

  try {
    deleteEmployeeImpact.value = await queryClient.fetchQuery(
      $orpc.employee.getDeleteImpact.queryOptions({
        input: {
          id: item.id,
        },
      }),
    );
  } catch (error: any) {
    deleteEmployeeError.value = getEmployeeDeleteErrorMessage(error, "无法加载删除信息");
  } finally {
    deleteEmployeeLoading.value = false;
  }
}

async function handleDeleteEmployee(confirmText: string) {
  if (!deleteEmployeeImpact.value) {
    return;
  }

  deleteEmployeeError.value = "";

  try {
    const result = await removeEmployee.mutateAsync({
      id: deleteEmployeeImpact.value.id,
      confirmText,
    });

    await queryClient.invalidateQueries();
    deleteEmployeeOpen.value = false;

    if (editingId.value === result.id) {
      closeEditor();
    }

    if (faceTaskEmployeeId.value === result.id) {
      closeFaceTask();
    }

    toast.add({
      title: "员工已删除",
      description: `已同时删除 ${result.deletedFaceProfileCount} 条录脸任务、${result.deletedAttendanceRecordCount} 条考勤记录`,
    });
  } catch (error: any) {
    deleteEmployeeError.value = getEmployeeDeleteErrorMessage(error);
  }
}
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
          <div v-if="employeesQuery.status.value === 'pending'" class="space-y-3">
            <USkeleton class="h-12 w-full rounded-2xl" />
            <USkeleton class="h-12 w-full rounded-2xl" />
            <USkeleton class="h-12 w-full rounded-2xl" />
          </div>

          <UAlert
            v-else-if="employeesQuery.status.value === 'error'"
            color="error"
            icon="i-lucide-alert-circle"
            title="加载失败"
            :description="employeesQuery.error.value?.message || '无法加载员工列表'"
          />

          <EmptyState
            v-else-if="rows.length === 0"
            title="暂无员工"
            description="当前筛选条件下没有可显示的员工记录。"
          >
            <template #actions>
              <UButton icon="i-lucide-plus" class="rounded-2xl" @click="openCreate()"
                >新增员工</UButton
              >
            </template>
          </EmptyState>

          <template v-else>
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
                  <div class="flex flex-wrap gap-2">
                    <UButton size="xs" icon="i-lucide-scan-face" @click="openFaceTask(row.original)"
                      >录脸</UButton
                    >
                    <UButton
                      size="xs"
                      variant="outline"
                      icon="i-lucide-pencil-line"
                      @click="startEdit(row.original)"
                    >
                      编辑
                    </UButton>
                    <UButton
                      size="xs"
                      color="error"
                      variant="outline"
                      icon="i-lucide-trash-2"
                      @click="openDeleteEmployee(row.original)"
                    >
                      删除
                    </UButton>
                  </div>
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

                  <UBadge
                    :label="labelFaceStatus(item.faceProfile?.status)"
                    :color="colorFaceStatus(item.faceProfile?.status)"
                    variant="subtle"
                    class="rounded-full"
                  />
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

                <div class="mt-4 flex flex-wrap justify-end gap-2">
                  <UButton size="sm" icon="i-lucide-scan-face" @click="openFaceTask(item)"
                    >录脸</UButton
                  >
                  <UButton
                    size="sm"
                    variant="outline"
                    icon="i-lucide-pencil-line"
                    @click="startEdit(item)"
                  >
                    编辑
                  </UButton>
                  <UButton
                    size="sm"
                    color="error"
                    variant="outline"
                    icon="i-lucide-trash-2"
                    @click="openDeleteEmployee(item)"
                  >
                    删除
                  </UButton>
                </div>
              </div>
            </div>
          </template>

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

      <USlideover v-model:open="faceTaskOpen" :title="faceTaskTitle" side="right">
        <template #body>
          <div v-if="currentFaceEmployee" class="space-y-5">
            <div class="space-y-1">
              <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">员工</div>
              <div class="text-lg font-semibold tracking-tight text-highlighted">
                {{ currentFaceEmployee.name }}
              </div>
              <div class="text-sm text-toned">{{ currentFaceEmployee.code }}</div>
            </div>

            <div class="grid gap-4 sm:grid-cols-2">
              <div>
                <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">
                  当前状态
                </div>
                <div class="mt-2">
                  <UBadge
                    :label="labelFaceStatus(currentFaceEmployee.faceProfile?.status)"
                    :color="colorFaceStatus(currentFaceEmployee.faceProfile?.status)"
                    variant="subtle"
                    class="rounded-full"
                  />
                </div>
              </div>

              <div>
                <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">
                  当前设备
                </div>
                <div class="mt-2 text-sm font-medium text-highlighted">
                  {{ currentFaceEmployee.faceProfile?.deviceName || "未分配设备" }}
                </div>
              </div>
            </div>

            <UFormField label="录脸设备" required>
              <USelect
                v-model="faceTaskForm.deviceId"
                data-testid="employee-face-device-select"
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
              title="暂无可用设备"
              description="请先启用至少一台设备。"
            />

            <UAlert
              v-if="faceTaskError"
              color="error"
              icon="i-lucide-alert-circle"
              title="操作失败"
              :description="faceTaskError"
            />

            <div class="flex flex-col gap-3 pt-2">
              <UButton
                type="button"
                data-testid="employee-face-submit-button"
                :loading="assignFaceTask.isPending.value"
                :disabled="activeDeviceOptions.length === 0"
                class="w-full rounded-2xl"
                icon="i-lucide-scan-face"
                @click="handleFaceTaskSubmit()"
              >
                {{ faceTaskActionLabel }}
              </UButton>

              <UButton
                v-if="currentFaceEmployee.faceProfile?.status === 'pending'"
                type="button"
                color="error"
                variant="outline"
                :loading="cancelFaceTask.isPending.value"
                class="w-full rounded-2xl"
                icon="i-lucide-ban"
                @click="handleFaceTaskCancel()"
              >
                取消待录入
              </UButton>

              <UButton
                type="button"
                variant="ghost"
                color="neutral"
                class="w-full"
                @click="closeFaceTask()"
                >关闭</UButton
              >
            </div>
          </div>
        </template>
      </USlideover>

      <USlideover
        v-model:open="editorOpen"
        :title="editingId ? '编辑员工' : '新增员工'"
        side="right"
      >
        <template #body>
          <form class="space-y-5" @submit.prevent="handleSubmit">
            <UFormField label="员工编号" required>
              <UInput
                v-model="form.code"
                data-testid="employee-code-input"
                placeholder="例如 E1001"
                icon="i-lucide-id-card"
                class="w-full"
              />
            </UFormField>

            <UFormField label="员工姓名" required>
              <UInput
                v-model="form.name"
                data-testid="employee-name-input"
                placeholder="请输入员工姓名"
                icon="i-lucide-user"
                class="w-full"
              />
            </UFormField>

            <UAlert
              v-if="editorError"
              color="error"
              icon="i-lucide-alert-circle"
              title="操作失败"
              :description="editorError"
            />

            <div class="flex flex-col gap-3 pt-2">
              <UButton
                type="submit"
                data-testid="employee-submit-button"
                :loading="isSavingEmployee"
                class="w-full rounded-2xl"
                icon="i-lucide-save"
              >
                {{ editingId ? "保存修改" : "创建员工" }}
              </UButton>

              <UButton
                type="button"
                variant="ghost"
                color="neutral"
                class="w-full"
                @click="closeEditor()"
                >取消</UButton
              >
            </div>

            <div
              v-if="editingId"
              class="border-t border-neutral-200/70 pt-5 dark:border-neutral-800/80"
            >
              <UButton
                type="button"
                color="error"
                variant="outline"
                class="w-full rounded-2xl"
                icon="i-lucide-trash-2"
                :disabled="!currentEditingEmployee"
                @click="openDeleteEmployee(currentEditingEmployee)"
              >
                删除员工
              </UButton>
            </div>
          </form>
        </template>
      </USlideover>

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
