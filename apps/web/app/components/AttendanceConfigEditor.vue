<script setup lang="ts">
import { useMutation, useQuery, useQueryClient } from "@tanstack/vue-query";
import { computed, watch } from "vue";

import { formatTimeRange, minutesToTimeInput, timeInputToMinutes } from "~/utils/format";

const props = withDefaults(
  defineProps<{
    title?: string;
    subtitle?: string;
    showRules?: boolean;
  }>(),
  {
    title: "考勤配置",
    subtitle: "系统仅维护一套全局考勤时间窗口，保存后新的上报会立即按新窗口归类。",
    showRules: true,
  },
);

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
const statusLabel = computed(() => (currentConfig.value ? "已配置" : "待配置"));
const windowCards = computed(() => [
  {
    label: "上班窗口",
    value: formatTimeRange(
      currentConfig.value?.workStartMinute,
      currentConfig.value?.workEndMinute,
    ),
    icon: "i-lucide-sunrise",
  },
  {
    label: "下班窗口",
    value: formatTimeRange(currentConfig.value?.offStartMinute, currentConfig.value?.offEndMinute),
    icon: "i-lucide-sunset",
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
    if (!config) {
      return;
    }

    syncForm(config);
  },
  { immediate: true },
);

function restoreCurrentConfig() {
  actionError.value = "";

  if (!currentConfig.value) {
    form.value = {
      workStart: "09:00",
      workEnd: "10:00",
      offStart: "18:00",
      offEnd: "19:00",
    };
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
  <div id="attendance-config" class="grid gap-6 xl:grid-cols-[minmax(0,1.2fr)_minmax(0,0.8fr)]">
    <DataSurface :title="props.title" :subtitle="props.subtitle" icon="i-lucide-settings-2">
      <template #actions>
        <UBadge
          :label="statusLabel"
          :color="currentConfig ? 'success' : 'warning'"
          variant="subtle"
          class="rounded-full"
        />
        <UButton
          variant="outline"
          color="neutral"
          icon="i-lucide-refresh-cw"
          @click="configQuery.refetch()"
        >
          刷新
        </UButton>
      </template>

      <div class="space-y-5">
        <UAlert
          v-if="configQuery.status.value === 'error'"
          color="error"
          icon="i-lucide-alert-circle"
          title="配置读取失败"
          :description="configQuery.error.value?.message || '无法读取当前考勤配置'"
        />

        <UAlert
          v-else-if="!currentConfig"
          color="warning"
          icon="i-lucide-badge-alert"
          title="尚未保存考勤配置"
          description="首次保存后，设备才能按统一时间窗口生成有效考勤记录。"
        />

        <div class="grid gap-3 sm:grid-cols-2">
          <div
            v-for="item in windowCards"
            :key="item.label"
            class="workspace-mobile-card border border-neutral-200/70 bg-neutral-50/70 dark:border-neutral-800/80 dark:bg-neutral-900/60"
          >
            <div class="flex items-center gap-2 text-xs font-medium tracking-[0.14em] text-muted uppercase">
              <UIcon :name="item.icon" class="h-4 w-4" />
              {{ item.label }}
            </div>
            <div class="mt-2 text-lg font-semibold tracking-tight text-highlighted">
              {{ item.value }}
            </div>
          </div>
        </div>

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
              {{ currentConfig ? "恢复当前值" : "恢复默认建议" }}
            </UButton>
          </div>
        </form>
      </div>
    </DataSurface>

    <DataSurface v-if="props.showRules" title="配置说明" icon="i-lucide-badge-info">
      <div class="divide-y divide-neutral-200/70 text-sm dark:divide-neutral-800/80">
        <div class="py-3 first:pt-0">
          <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">唯一配置</div>
          <div class="mt-1 font-medium text-highlighted">系统只维护一套全局考勤时间段</div>
        </div>

        <div class="py-3">
          <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">统一时区</div>
          <div class="mt-1 font-medium text-highlighted">管理端与设备端统一按上海时区处理</div>
        </div>

        <div class="py-3 last:pb-0">
          <div class="text-xs font-medium tracking-[0.14em] text-muted uppercase">生效方式</div>
          <div class="mt-1 font-medium text-highlighted">
            保存后新的上报会立即按最新窗口归类
          </div>
        </div>
      </div>
    </DataSurface>
  </div>
</template>
