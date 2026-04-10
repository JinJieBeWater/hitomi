<script setup lang="ts">
import { useMutation, useQuery, useQueryClient } from "@tanstack/vue-query";
import { computed, ref, watch } from "vue";

import { formatDeviceSerialConnectError, useDeviceSerial } from "~/composables/useDeviceSerial";
import { usePagedListState } from "~/composables/usePagedListState";
import { colorDeviceStatus, formatDateTime, labelDeviceStatus } from "~/utils/format";

definePageMeta({
  layout: "dashboard",
  middleware: ["auth"],
});

const { $orpc } = useNuxtApp();
const queryClient = useQueryClient();
const toast = useToast();
const serial = useDeviceSerial();
const pageSize = 20;

const page = ref(1);
const keyword = ref("");
const status = ref<"active" | "disabled" | undefined>();
const editorOpen = ref(false);
const editingDevice = ref<{ id: string; name: string; status: string } | null>(null);
const actionError = ref("");
const deleteDeviceOpen = ref(false);
const deleteDeviceLoading = ref(false);
const deleteDeviceError = ref("");
const lastCreated = ref<{
  id: string;
  deviceCode: string;
  bootstrapSerial: string;
  bootstrapSecret: string;
  name: string;
} | null>(null);
const serialConnectLoading = ref(false);
const deleteDeviceImpact = ref<{
  id: string;
  deviceCode: string;
  name: string;
  faceProfileCount: number;
  attendanceRecordCount: number;
  confirmText: string;
} | null>(null);

const devicesQuery = useQuery(
  computed(() =>
    $orpc.device.list.queryOptions({
      input: {
        page: page.value,
        pageSize,
        keyword: keyword.value || undefined,
        status: status.value,
      },
    }),
  ),
);

const removeDevice = useMutation($orpc.device.remove.mutationOptions());
const updateDevice = useMutation($orpc.device.update.mutationOptions());

const rows = computed(() => devicesQuery.data.value?.items ?? []);
const { total, resetPage } = usePagedListState({
  page,
  pageSize,
  getPageInfo: () => devicesQuery.data.value?.pageInfo,
  resetOn: [keyword, status],
});

const metrics = computed(() => {
  const list = rows.value;
  const now = Date.now();
  const day = 24 * 60 * 60 * 1000;

  return [
    {
      label: "设备总数",
      value: total.value,
      caption: "当前筛选",
      icon: "i-lucide-monitor-smartphone",
      color: "primary" as const,
    },
    {
      label: "启用中",
      value: list.filter((item) => item.status === "active").length,
      caption: "当前页",
      icon: "i-lucide-circle-check-big",
      color: "success" as const,
    },
    {
      label: "已禁用",
      value: list.filter((item) => item.status === "disabled").length,
      caption: "当前页",
      icon: "i-lucide-circle-off",
      color: "neutral" as const,
    },
    {
      label: "近 24 小时在线",
      value: list.filter((item) => item.lastSeenAt && now - item.lastSeenAt <= day).length,
      caption: "当前页",
      icon: "i-lucide-activity",
      color: "warning" as const,
    },
  ];
});

const deviceColumns = [
  { accessorKey: "name", header: "设备名称" },
  { accessorKey: "deviceCode", header: "设备码" },
  { accessorKey: "status", header: "状态" },
  { accessorKey: "lastSeenAt", header: "最近在线" },
  { accessorKey: "createdAt", header: "创建时间" },
  { accessorKey: "actions", header: "" },
];

const deviceStatusOptions = [
  { label: "启用", value: "active", description: "设备可继续上报业务数据" },
  { label: "禁用", value: "disabled", description: "设备被停用，但历史数据保留" },
];

const headerBadges = computed(() => {
  const list: Array<{ label: string; color: "primary" | "warning" | "neutral" }> = [
    { label: `${total.value} 条记录`, color: "neutral" },
  ];

  if (keyword.value) {
    list.push({ label: `关键词: ${keyword.value}`, color: "primary" });
  }

  if (status.value) {
    const label =
      deviceStatusOptions.find((item) => item.value === status.value)?.label || status.value;
    list.push({ label: `状态: ${label}`, color: "warning" });
  }

  return list;
});

const createdResultOpen = computed({
  get: () => Boolean(lastCreated.value),
  set: (open: boolean) => {
    if (!open) {
      lastCreated.value = null;
    }
  },
});

watch(deleteDeviceOpen, (open) => {
  if (!open) {
    deleteDeviceLoading.value = false;
    deleteDeviceError.value = "";
    deleteDeviceImpact.value = null;
  }
});

function openCreate() {
  editingDevice.value = null;
  editorOpen.value = true;
}

function startEdit(item: any) {
  editingDevice.value = { id: item.id, name: item.name, status: item.status };
  editorOpen.value = true;
}

function getDeviceDeleteErrorMessage(error: any, fallback = "删除设备失败，请稍后重试") {
  const businessCode = error?.data?.businessCode;

  if (businessCode === "DEVICE_NOT_FOUND") {
    return "设备不存在或已被删除";
  }

  if (businessCode === "DEVICE_DELETE_CONFIRMATION_MISMATCH") {
    return "设备码不匹配，请重新确认";
  }

  return fallback;
}

async function quickToggle(item: any) {
  actionError.value = "";

  try {
    await updateDevice.mutateAsync({
      id: item.id,
      status: item.status === "active" ? "disabled" : "active",
    });

    await queryClient.invalidateQueries();
    toast.add({
      title: item.status === "active" ? "设备已禁用" : "设备已启用",
    });
  } catch (error: any) {
    actionError.value = error?.message || "切换状态失败";
  }
}

async function copyText(label: string, value: string) {
  if (!import.meta.client || !navigator.clipboard) {
    toast.add({
      title: "当前环境不支持复制",
      description: `请手动复制${label}`,
    });
    return;
  }

  try {
    await navigator.clipboard.writeText(value);
    toast.add({ title: `${label}已复制` });
  } catch (error: any) {
    toast.add({
      title: "复制失败",
      description: error?.message || `请手动复制${label}`,
    });
  }
}

async function openSerialConfigPage() {
  serialConnectLoading.value = true;

  try {
    await serial.requestPortConnection();
    await navigateTo("/devices/serial");
  } catch (error: any) {
    toast.add({
      title: "连接设备失败",
      description: formatDeviceSerialConnectError(error),
      color: "error",
    });
  } finally {
    serialConnectLoading.value = false;
  }
}

function resetFilters() {
  resetPage();
  keyword.value = "";
  status.value = undefined;
}

async function openDeleteDevice(item: { id: string } | null | undefined) {
  if (!item?.id) {
    return;
  }

  deleteDeviceOpen.value = true;
  deleteDeviceLoading.value = true;
  deleteDeviceError.value = "";
  deleteDeviceImpact.value = null;

  try {
    deleteDeviceImpact.value = await queryClient.fetchQuery(
      $orpc.device.getDeleteImpact.queryOptions({
        input: {
          id: item.id,
        },
      }),
    );
  } catch (error: any) {
    deleteDeviceError.value = getDeviceDeleteErrorMessage(error, "无法加载删除信息");
  } finally {
    deleteDeviceLoading.value = false;
  }
}

async function handleDeleteDevice(confirmText: string) {
  if (!deleteDeviceImpact.value) {
    return;
  }

  deleteDeviceError.value = "";

  try {
    const result = await removeDevice.mutateAsync({
      id: deleteDeviceImpact.value.id,
      confirmText,
    });

    await queryClient.invalidateQueries();
    deleteDeviceOpen.value = false;

    if (editingDevice.value?.id === result.id) {
      editorOpen.value = false;
    }

    toast.add({
      title: "设备已删除",
      description: `已同时删除 ${result.deletedFaceProfileCount} 条录脸任务、${result.deletedAttendanceRecordCount} 条考勤记录`,
    });
  } catch (error: any) {
    deleteDeviceError.value = getDeviceDeleteErrorMessage(error);
  }
}

function getRowActions(item: any) {
  return [
    [
      {
        label: "编辑",
        icon: "i-lucide-pencil-line",
        onSelect: () => startEdit(item),
      },
      {
        label: item.status === "active" ? "禁用" : "启用",
        icon: "i-lucide-power",
        onSelect: () => quickToggle(item),
      },
    ],
    [
      {
        label: "删除",
        icon: "i-lucide-trash-2",
        color: "error" as const,
        onSelect: () => openDeleteDevice(item),
      },
    ],
  ];
}
</script>

<template>
  <UDashboardPanel id="devices">
    <template #header>
      <PageHeader title="设备管理" :badges="headerBadges">
        <template #actions>
          <UButton variant="outline" icon="i-lucide-refresh-cw" @click="devicesQuery.refetch()">刷新</UButton>
          <UButton
            variant="outline"
            icon="i-lucide-usb"
            :loading="serialConnectLoading"
            @click="openSerialConfigPage"
          >
            连接设备
          </UButton>
          <UButton icon="i-lucide-plus" class="rounded-2xl" @click="openCreate()">创建设备</UButton>
        </template>
      </PageHeader>
    </template>

    <template #body>
      <div class="workspace-page-stack">
        <MetricStrip :metrics="metrics" />

        <UAlert
          v-if="actionError"
          color="error"
          icon="i-lucide-alert-circle"
          title="操作失败"
          :description="actionError"
        />

        <FilterBar>
          <div class="grid gap-3 md:grid-cols-2 xl:grid-cols-[minmax(0,1.3fr)_minmax(0,1fr)]">
            <UFormField label="关键词">
              <UInput
                v-model="keyword"
                placeholder="设备名称或设备码"
                icon="i-lucide-search"
                class="w-full"
              />
            </UFormField>

            <UFormField label="状态">
              <USelect
                v-model="status"
                :items="deviceStatusOptions"
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

        <DataSurface title="设备列表">
          <QueryGuard
            :status="devicesQuery.status.value"
            :error="devicesQuery.error.value?.message"
            :empty="rows.length === 0"
            empty-title="暂无设备"
            empty-description="当前筛选条件下没有可显示的设备记录。"
          >
            <template #empty-actions>
              <UButton icon="i-lucide-plus" class="rounded-2xl" @click="openCreate()"
                >创建设备</UButton
              >
            </template>

            <div class="workspace-surface-table hidden md:block">
              <UTable
                :data="rows"
                :columns="deviceColumns"
                empty="暂无设备数据"
                :ui="{ root: 'w-full overflow-x-auto', base: 'w-full min-w-[820px]' }"
              >
                <template #status-cell="{ row }">
                  <UBadge
                    :label="labelDeviceStatus(row.original.status)"
                    :color="colorDeviceStatus(row.original.status)"
                    variant="subtle"
                    class="rounded-full"
                  />
                </template>

                <template #lastSeenAt-cell="{ row }">
                  <div class="text-sm text-toned">
                    {{ formatDateTime(row.original.lastSeenAt) }}
                  </div>
                </template>

                <template #createdAt-cell="{ row }">
                  <div class="text-sm text-toned">{{ formatDateTime(row.original.createdAt) }}</div>
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
                    <div class="text-sm text-toned">{{ item.deviceCode }}</div>
                  </div>

                  <div class="flex items-center gap-2">
                    <UBadge
                      :label="labelDeviceStatus(item.status)"
                      :color="colorDeviceStatus(item.status)"
                      variant="subtle"
                      class="rounded-full"
                    />
                    <RowActions :items="getRowActions(item)" trigger-size="sm" />
                  </div>
                </div>

                <div class="mt-4 grid gap-3 text-sm sm:grid-cols-2">
                  <div>
                    <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">
                      最近在线
                    </div>
                    <div class="mt-1 text-highlighted">{{ formatDateTime(item.lastSeenAt) }}</div>
                  </div>

                  <div>
                    <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">
                      创建时间
                    </div>
                    <div class="mt-1 text-highlighted">{{ formatDateTime(item.createdAt) }}</div>
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

      <DeviceSlideoverEditor
        :open="editorOpen"
        :device="editingDevice"
        @update:open="editorOpen = $event"
        @created="lastCreated = $event"
        @request-delete="openDeleteDevice(editingDevice)"
      />

      <DeviceCredentialsModal
        v-model:open="createdResultOpen"
        :device="lastCreated"
        @copy:code="lastCreated && copyText('设备码', lastCreated.deviceCode)"
        @copy:bootstrap-serial="lastCreated && copyText('Bootstrap 序列号', lastCreated.bootstrapSerial)"
        @copy:bootstrap-secret="lastCreated && copyText('Bootstrap 密钥', lastCreated.bootstrapSecret)"
        @start-activation="createdResultOpen = false; openSerialConfigPage()"
      />

      <DeleteConfirmModal
        :open="deleteDeviceOpen"
        entity-label="设备"
        identifier-label="设备码"
        :impact="
          deleteDeviceImpact
            ? {
                name: deleteDeviceImpact.name,
                identifierValue: deleteDeviceImpact.deviceCode,
                faceProfileCount: deleteDeviceImpact.faceProfileCount,
                attendanceRecordCount: deleteDeviceImpact.attendanceRecordCount,
                confirmText: deleteDeviceImpact.confirmText,
              }
            : null
        "
        :loading="deleteDeviceLoading"
        :submitting="removeDevice.isPending.value"
        :error="deleteDeviceError"
        @update:open="deleteDeviceOpen = $event"
        @confirm="handleDeleteDevice($event)"
      />
    </template>
  </UDashboardPanel>
</template>
