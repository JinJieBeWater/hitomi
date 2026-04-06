<script setup lang="ts">
import { useMutation, useQuery, useQueryClient } from "@tanstack/vue-query";
import { computed, watch } from "vue";

import { formatTimeRange, minutesToTimeInput, timeInputToMinutes } from "~/utils/format";

definePageMeta({
  layout: "dashboard",
  middleware: ["auth"],
});

const { $orpc } = useNuxtApp();
const queryClient = useQueryClient();
const toast = useToast();

const configQuery = useQuery($orpc.attendanceConfig.get.queryOptions());
const saveConfig = useMutation($orpc.attendanceConfig.save.mutationOptions());

const form = ref({
  workStart: "09:00",
  workEnd: "10:00",
  offStart: "18:00",
  offEnd: "19:00",
});
const actionError = ref("");

const currentConfig = computed(() => configQuery.data.value?.config ?? null);

const metrics = computed(() => [
  {
    label: "上班窗口",
    value: formatTimeRange(
      currentConfig.value?.workStartMinute,
      currentConfig.value?.workEndMinute,
    ),
    icon: "i-lucide-sunrise",
    color: "warning" as const,
  },
  {
    label: "下班窗口",
    value: formatTimeRange(currentConfig.value?.offStartMinute, currentConfig.value?.offEndMinute),
    icon: "i-lucide-sunset",
    color: "primary" as const,
  },
  {
    label: "配置状态",
    value: currentConfig.value ? "已配置" : "未配置",
    icon: "i-lucide-settings-2",
    color: currentConfig.value ? ("success" as const) : ("neutral" as const),
  },
  {
    label: "业务时区",
    value: "Asia/Shanghai",
    icon: "i-lucide-globe",
    color: "neutral" as const,
  },
]);

const headerBadges = computed(() => [
  {
    label: currentConfig.value ? "已配置" : "待配置",
    color: currentConfig.value ? ("success" as const) : ("warning" as const),
  },
]);

function syncForm(config: NonNullable<typeof currentConfig.value>) {
  form.value = {
    workStart: minutesToTimeInput(config.workStartMinute),
    workEnd: minutesToTimeInput(config.workEndMinute),
    offStart: minutesToTimeInput(config.offStartMinute),
    offEnd: minutesToTimeInput(config.offEndMinute),
  };
}

watch(
  () => currentConfig.value,
  (config) => {
    if (!config) return;
    syncForm(config);
  },
  { immediate: true },
);

function restoreCurrentConfig() {
  actionError.value = "";

  if (!currentConfig.value) {
    return;
  }

  syncForm(currentConfig.value);
}

async function handleSubmit() {
  actionError.value = "";

  const workStartMinute = timeInputToMinutes(form.value.workStart);
  const workEndMinute = timeInputToMinutes(form.value.workEnd);
  const offStartMinute = timeInputToMinutes(form.value.offStart);
  const offEndMinute = timeInputToMinutes(form.value.offEnd);

  if (
    workStartMinute === null ||
    workEndMinute === null ||
    offStartMinute === null ||
    offEndMinute === null
  ) {
    actionError.value = "请输入有效的时间";
    return;
  }

  try {
    await saveConfig.mutateAsync({
      workStartMinute,
      workEndMinute,
      offStartMinute,
      offEndMinute,
    });

    await queryClient.invalidateQueries();
    toast.add({ title: "考勤配置已保存" });
  } catch (error: any) {
    actionError.value = error?.message || "保存失败";
  }
}
</script>

<template>
  <UDashboardPanel id="attendance-config">
    <template #header>
      <PageHeader title="考勤配置" :badges="headerBadges">
        <template #actions>
          <UButton variant="outline" icon="i-lucide-refresh-cw" @click="configQuery.refetch()"
            >刷新</UButton
          >
        </template>
      </PageHeader>
    </template>

    <template #body>
      <div class="workspace-page-stack">
        <MetricStrip :metrics="metrics" />

        <UAlert
          v-if="configQuery.status.value === 'error'"
          color="error"
          icon="i-lucide-alert-circle"
          title="配置读取失败"
          :description="configQuery.error.value?.message || '无法读取当前考勤配置'"
        />

        <div class="grid gap-6 xl:grid-cols-[minmax(0,1.2fr)_minmax(0,0.8fr)]">
          <DataSurface
            :title="currentConfig ? '时间窗口' : '首次创建配置'"
            class="workspace-form-surface"
          >
            <form class="space-y-5" @submit.prevent="handleSubmit">
              <div class="grid gap-4 xl:grid-cols-2">
                <div class="workspace-mobile-card">
                  <div class="text-sm font-semibold tracking-tight text-highlighted">上班窗口</div>

                  <div class="mt-4 grid gap-4 sm:grid-cols-2">
                    <UFormField label="开始">
                      <UInput
                        v-model="form.workStart"
                        data-testid="attendance-work-start-input"
                        type="time"
                        class="w-full"
                      />
                    </UFormField>

                    <UFormField label="结束">
                      <UInput
                        v-model="form.workEnd"
                        data-testid="attendance-work-end-input"
                        type="time"
                        class="w-full"
                      />
                    </UFormField>
                  </div>
                </div>

                <div class="workspace-mobile-card">
                  <div class="text-sm font-semibold tracking-tight text-highlighted">下班窗口</div>

                  <div class="mt-4 grid gap-4 sm:grid-cols-2">
                    <UFormField label="开始">
                      <UInput
                        v-model="form.offStart"
                        data-testid="attendance-off-start-input"
                        type="time"
                        class="w-full"
                      />
                    </UFormField>

                    <UFormField label="结束">
                      <UInput
                        v-model="form.offEnd"
                        data-testid="attendance-off-end-input"
                        type="time"
                        class="w-full"
                      />
                    </UFormField>
                  </div>
                </div>
              </div>

              <UAlert
                v-if="actionError"
                color="error"
                icon="i-lucide-alert-circle"
                title="保存失败"
                :description="actionError"
              />

              <div class="flex flex-col gap-3 sm:flex-row">
                <UButton
                  type="submit"
                  data-testid="attendance-save-button"
                  :loading="saveConfig.isPending.value"
                  class="w-full rounded-2xl sm:w-auto"
                  icon="i-lucide-save"
                >
                  保存配置
                </UButton>

                <UButton
                  type="button"
                  variant="ghost"
                  color="neutral"
                  class="w-full sm:w-auto"
                  icon="i-lucide-rotate-ccw"
                  @click="restoreCurrentConfig()"
                >
                  恢复当前值
                </UButton>
              </div>
            </form>
          </DataSurface>

          <DataSurface title="规则">
            <div class="divide-y divide-neutral-200/70 text-sm dark:divide-neutral-800/80">
              <div class="py-3 first:pt-0">
                <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">
                  唯一记录
                </div>
                <div class="mt-1 font-medium text-highlighted">
                  每位员工每天每种类型仅保留一条最终记录
                </div>
              </div>

              <div class="py-3">
                <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">
                  日期切分
                </div>
                <div class="mt-1 font-medium text-highlighted">
                  管理端与设备端统一按上海时区处理
                </div>
              </div>

              <div class="py-3 last:pb-0">
                <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">
                  生效方式
                </div>
                <div class="mt-1 font-medium text-highlighted">
                  保存后新的上报会立即按新窗口归类
                </div>
              </div>
            </div>
          </DataSurface>
        </div>
      </div>
    </template>
  </UDashboardPanel>
</template>
