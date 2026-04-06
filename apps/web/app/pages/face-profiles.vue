<script setup lang="ts">
import { useMutation, useQuery, useQueryClient } from "@tanstack/vue-query";
import { computed, ref } from "vue";

import { usePagedListState } from "~/composables/usePagedListState";
import { colorFaceStatus, formatDateTime, labelFaceStatus } from "~/utils/format";

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

const faceProfilesQuery = useQuery(
  computed(() =>
    $orpc.faceProfile.list.queryOptions({
      input: {
        page: page.value,
        pageSize,
        status: status.value,
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
  { accessorKey: "actions", header: "" },
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
  ];

  return [actions];
}

function resetFilters() {
  resetPage();
  status.value = undefined;
}
</script>

<template>
  <UDashboardPanel id="face-profiles">
    <template #header>
      <PageHeader title="录脸记录">
        <template #actions>
          <UButton variant="outline" icon="i-lucide-refresh-cw" @click="faceProfilesQuery.refetch()"
            >刷新</UButton
          >
        </template>
      </PageHeader>
    </template>

    <template #body>
      <div class="workspace-page-stack">
        <DataSurface title="录脸记录列表">
          <template #actions>
            <div class="flex w-full flex-col gap-2 sm:w-auto sm:flex-row">
              <USelect
                v-model="status"
                :items="statusOptions"
                placeholder="全部状态"
                class="w-full sm:min-w-40"
              />
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

          <QueryGuard
            :status="faceProfilesQuery.status.value"
            :error="faceProfilesQuery.error.value?.message"
            :empty="rows.length === 0"
            empty-title="暂无录脸记录"
            empty-description="暂无匹配的录脸记录。"
          >
            <div class="workspace-surface-table hidden md:block">
              <UTable
                :data="rows"
                :columns="faceProfileColumns"
                empty="暂无录脸记录"
                :ui="{ root: 'w-full overflow-x-auto', base: 'w-full min-w-[760px]' }"
              >
                <template #employee-cell="{ row }">
                  <div class="space-y-1">
                    <div class="font-medium text-highlighted">
                      {{ row.original.employee?.name || "-" }}
                    </div>
                    <div class="text-xs text-toned">{{ row.original.employee?.code || "-" }}</div>
                  </div>
                </template>

                <template #device-cell="{ row }">
                  <div class="space-y-1">
                    <div class="font-medium text-highlighted">
                      {{ row.original.device?.name || "-" }}
                    </div>
                    <div class="text-xs text-toned">
                      {{ row.original.device?.deviceCode || "-" }}
                    </div>
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
                    <div class="text-sm text-toned">{{ item.employee?.code || "-" }}</div>
                  </div>

                  <div class="flex items-center gap-2">
                    <UBadge
                      :label="labelFaceStatus(item.status)"
                      :color="colorFaceStatus(item.status)"
                      variant="subtle"
                      class="rounded-full"
                    />
                    <RowActions :items="getRowActions(item)" trigger-size="sm" />
                  </div>
                </div>

                <div class="mt-4 grid gap-3 text-sm sm:grid-cols-2">
                  <div>
                    <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">
                      设备
                    </div>
                    <div class="mt-1 text-highlighted">{{ item.device?.name || "-" }}</div>
                    <div class="text-xs text-toned">{{ item.device?.deviceCode || "-" }}</div>
                  </div>

                  <div>
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
    </template>
  </UDashboardPanel>
</template>
