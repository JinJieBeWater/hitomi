<script setup lang="ts">
import { useMutation, useQuery, useQueryClient } from "@tanstack/vue-query";
import { computed, ref } from "vue";

import { usePagedListState } from "~/composables/usePagedListState";
import { colorFaceStatus, formatDateTime } from "~/utils/format";

definePageMeta({
  layout: "dashboard",
  middleware: ["auth"],
});

const { $orpc } = useNuxtApp();
const queryClient = useQueryClient();
const toast = useToast();
const pageSize = 20;

const page = ref(1);
const status = ref<"pending" | "success" | "failed" | "cancelled" | undefined>();

const faceProfilesQuery = useQuery(
  computed(() =>
    $orpc.faceProfile.list.queryOptions({
      input: {
        page: page.value,
        pageSize,
        status: (status.value || undefined) as
          | "pending"
          | "success"
          | "failed"
          | "cancelled"
          | undefined,
      },
    }),
  ),
);

const cancelTask = useMutation($orpc.faceProfile.cancel.mutationOptions());

const rows = computed(() => faceProfilesQuery.data.value?.items ?? []);
const { total, resetPage } = usePagedListState({
  page,
  pageSize,
  getPageInfo: () => faceProfilesQuery.data.value?.pageInfo,
  resetOn: [status],
});

const faceProfileColumns = [
  { accessorKey: "employee", header: "员工" },
  { accessorKey: "device", header: "设备" },
  { accessorKey: "status", header: "状态" },
  { accessorKey: "updatedAt", header: "更新时间" },
  { accessorKey: "actions", header: "操作" },
];

const statusOptions = [
  { label: "待录入", value: "pending", description: "等待设备端完成采集" },
  { label: "录入成功", value: "success", description: "录脸已完成，可参与识别" },
  { label: "录入失败", value: "failed", description: "需要人工介入或重试" },
  { label: "已取消", value: "cancelled", description: "任务已停止，不再继续" },
];

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

async function goToEmployee(item: any) {
  await navigateTo({
    path: "/employees",
    query: {
      keyword: item.employee?.code,
      manageFace: item.employeeId,
    },
  });
}

function resetFilters() {
  resetPage();
  status.value = undefined;
}
</script>

<template>
  <UDashboardPanel id="face-profiles">
    <template #header>
      <PageHeader title="录脸任务">
        <template #actions>
          <UButton variant="outline" icon="i-lucide-refresh-cw" @click="faceProfilesQuery.refetch()">刷新</UButton>
        </template>
      </PageHeader>
    </template>

    <template #body>
      <div class="workspace-page-stack">
        <DataSurface title="任务队列">
          <template #actions>
            <div class="flex w-full flex-col gap-2 sm:w-auto sm:flex-row">
              <USelect v-model="status" :items="statusOptions" placeholder="全部状态" class="w-full sm:min-w-40" />
              <UButton
                variant="ghost"
                color="neutral"
                icon="i-lucide-rotate-ccw"
                :disabled="!status"
                @click="resetFilters()"
              >
                清空
              </UButton>
            </div>
          </template>

          <div v-if="faceProfilesQuery.status.value === 'pending'" class="space-y-3">
            <USkeleton class="h-12 w-full rounded-2xl" />
            <USkeleton class="h-12 w-full rounded-2xl" />
            <USkeleton class="h-12 w-full rounded-2xl" />
          </div>

          <UAlert
            v-else-if="faceProfilesQuery.status.value === 'error'"
            color="error"
            icon="i-lucide-alert-circle"
            title="加载失败"
            :description="faceProfilesQuery.error.value?.message || '无法加载录脸任务列表'"
          />

          <EmptyState
            v-else-if="rows.length === 0"
            title="暂无录脸任务"
            description="当前条件下没有可处理的任务。"
            icon="i-lucide-scan-face"
          />

          <template v-else>
            <div class="workspace-surface-table hidden md:block">
              <UTable
                :data="rows"
                :columns="faceProfileColumns"
                empty="暂无录脸任务"
                :ui="{ root: 'w-full overflow-x-auto', base: 'w-full min-w-[760px]' }"
              >
                <template #employee-cell="{ row }">
                  <div class="space-y-1">
                    <div class="font-medium text-highlighted">{{ row.original.employee?.name || "-" }}</div>
                    <div class="text-xs text-toned">{{ row.original.employee?.code || "-" }}</div>
                  </div>
                </template>

                <template #device-cell="{ row }">
                  <div class="space-y-1">
                    <div class="font-medium text-highlighted">{{ row.original.device?.name || "-" }}</div>
                    <div class="text-xs text-toned">{{ row.original.device?.deviceCode || "-" }}</div>
                  </div>
                </template>

                <template #status-cell="{ row }">
                  <UBadge
                    :label="labelFaceStatus(row.original.status)"
                    :color="colorFaceStatus(row.original.status)"
                    variant="subtle"
                    class="rounded-full"
                  />
                </template>

                <template #updatedAt-cell="{ row }">
                  <div class="text-sm text-toned">{{ formatDateTime(row.original.updatedAt) }}</div>
                </template>

                <template #actions-cell="{ row }">
                  <div class="flex flex-wrap gap-2">
                    <UButton size="xs" variant="outline" icon="i-lucide-arrow-up-right" @click="goToEmployee(row.original)">
                      处理
                    </UButton>

                    <UButton
                      v-if="row.original.status === 'pending'"
                      size="xs"
                      color="error"
                      variant="outline"
                      icon="i-lucide-ban"
                      @click="handleCancel(row.original.id)"
                    >
                      取消
                    </UButton>
                  </div>
                </template>
              </UTable>
            </div>

            <div class="grid gap-3 md:hidden">
              <div v-for="item in rows" :key="item.id" class="workspace-mobile-card">
                <div class="flex items-start justify-between gap-3">
                  <div class="min-w-0 space-y-1">
                    <div class="truncate text-base font-semibold text-highlighted">{{ item.employee?.name || "-" }}</div>
                    <div class="text-sm text-toned">{{ item.employee?.code || "-" }}</div>
                  </div>

                  <UBadge
                    :label="labelFaceStatus(item.status)"
                    :color="colorFaceStatus(item.status)"
                    variant="subtle"
                    class="rounded-full"
                  />
                </div>

                <div class="mt-4 grid gap-3 text-sm sm:grid-cols-2">
                  <div>
                    <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">设备</div>
                    <div class="mt-1 text-highlighted">{{ item.device?.name || "-" }}</div>
                    <div class="text-xs text-toned">{{ item.device?.deviceCode || "-" }}</div>
                  </div>

                  <div>
                    <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">更新时间</div>
                    <div class="mt-1 text-highlighted">{{ formatDateTime(item.updatedAt) }}</div>
                  </div>
                </div>

                <div class="mt-4 flex flex-wrap justify-end gap-2">
                  <UButton size="sm" variant="outline" icon="i-lucide-arrow-up-right" @click="goToEmployee(item)">
                    处理
                  </UButton>

                  <UButton
                    v-if="item.status === 'pending'"
                    size="sm"
                    color="error"
                    variant="outline"
                    icon="i-lucide-ban"
                    @click="handleCancel(item.id)"
                  >
                    取消
                  </UButton>
                </div>
              </div>
            </div>
          </template>

          <template #footer>
            <ListPagination :page="page" :page-size="pageSize" :total="total" @update:page="page = $event" />
          </template>
        </DataSurface>
      </div>
    </template>
  </UDashboardPanel>
</template>
