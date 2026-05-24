<script setup lang="ts">
import { useMutation, useQuery, useQueryClient } from "@tanstack/vue-query";
import { computed, watch } from "vue";

import { formatTimeRange, minutesToTimeInput, timeInputToMinutes } from "~/utils/format";

type FieldKey = "workStart" | "workEnd" | "offStart" | "offEnd";

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
const fieldErrors = ref<Record<FieldKey, string>>({
  workStart: "",
  workEnd: "",
  offStart: "",
  offEnd: "",
});

const currentConfig = computed(() => configQuery.data.value?.config ?? null);
const statusLabel = computed(() => (currentConfig.value ? "编辑配置" : "首次创建配置"));
const windowCards = computed(() => [
  {
    label: "上班窗口",
    value: currentConfig.value
      ? formatTimeRange(currentConfig.value.workStartMinute, currentConfig.value.workEndMinute)
      : `${form.value.workStart} - ${form.value.workEnd}`,
    icon: "i-lucide-sunrise",
  },
  {
    label: "下班窗口",
    value: currentConfig.value
      ? formatTimeRange(currentConfig.value.offStartMinute, currentConfig.value.offEndMinute)
      : `${form.value.offStart} - ${form.value.offEnd}`,
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
  clearFieldErrors();

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

function clearFieldErrors() {
  fieldErrors.value = {
    workStart: "",
    workEnd: "",
    offStart: "",
    offEnd: "",
  };
}

function setFieldError(key: FieldKey, message: string) {
  fieldErrors.value[key] = fieldErrors.value[key] || message;
}

function validateForm() {
  clearFieldErrors();

  const times = {
    workStartMinute: timeInputToMinutes(form.value.workStart),
    workEndMinute: timeInputToMinutes(form.value.workEnd),
    offStartMinute: timeInputToMinutes(form.value.offStart),
    offEndMinute: timeInputToMinutes(form.value.offEnd),
  };

  const requiredFields: Array<[FieldKey, number | null]> = [
    ["workStart", times.workStartMinute],
    ["workEnd", times.workEndMinute],
    ["offStart", times.offStartMinute],
    ["offEnd", times.offEndMinute],
  ];

  for (const [key, value] of requiredFields) {
    if (value === null) {
      setFieldError(key, "请选择有效时间");
    }
  }

  if (requiredFields.some(([, value]) => value === null)) {
    return { ok: false as const, message: "请完整填写四个有效时间" };
  }

  const { workStartMinute, workEndMinute, offStartMinute, offEndMinute } = times as Record<
    keyof typeof times,
    number
  >;

  if (workStartMinute >= workEndMinute) {
    setFieldError("workStart", "开始必须早于结束");
    setFieldError("workEnd", "结束必须晚于开始");
  }

  if (offStartMinute >= offEndMinute) {
    setFieldError("offStart", "开始必须早于结束");
    setFieldError("offEnd", "结束必须晚于开始");
  }

  const hasRangeError = Object.values(fieldErrors.value).some(Boolean);

  if (hasRangeError) {
    return { ok: false as const, message: "时间窗口不允许跨天，且开始必须早于结束" };
  }

  const hasOverlap = workStartMinute < offEndMinute && offStartMinute < workEndMinute;

  if (hasOverlap) {
    setFieldError("workStart", "两个窗口不能重叠");
    setFieldError("workEnd", "两个窗口不能重叠");
    setFieldError("offStart", "两个窗口不能重叠");
    setFieldError("offEnd", "两个窗口不能重叠");

    return { ok: false as const, message: "上班窗口和下班窗口不能重叠或相互包含" };
  }

  return {
    ok: true as const,
    values: { workStartMinute, workEndMinute, offStartMinute, offEndMinute },
  };
}

watch(
  form,
  () => {
    if (actionError.value) actionError.value = "";
    if (Object.values(fieldErrors.value).some(Boolean)) clearFieldErrors();
  },
  { deep: true },
);

async function handleSubmit() {
  actionError.value = "";
  const validation = validateForm();

  if (!validation.ok) {
    actionError.value = validation.message;
    return;
  }

  try {
    await saveConfig.mutateAsync(validation.values);

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
          :color="currentConfig ? 'neutral' : 'primary'"
          variant="soft"
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
          color="primary"
          variant="soft"
          icon="i-lucide-clock-3"
          title="首次创建配置"
          description="表单中的 09:00 - 10:00 / 18:00 - 19:00 仅为首次填写建议；点击保存后才会作为系统配置生效。"
        />

        <div class="grid gap-3 sm:grid-cols-2">
          <div v-for="item in windowCards" :key="item.label" class="workspace-mobile-card">
            <div class="flex items-center gap-2 text-sm font-medium text-muted">
              <UIcon :name="item.icon" class="size-4" />
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
                <UFormField label="开始" required :error="fieldErrors.workStart">
                  <UInput
                    v-model="form.workStart"
                    data-testid="attendance-work-start-input"
                    type="time"
                    class="w-full"
                  />
                </UFormField>

                <UFormField label="结束" required :error="fieldErrors.workEnd">
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
                <UFormField label="开始" required :error="fieldErrors.offStart">
                  <UInput
                    v-model="form.offStart"
                    data-testid="attendance-off-start-input"
                    type="time"
                    class="w-full"
                  />
                </UFormField>

                <UFormField label="结束" required :error="fieldErrors.offEnd">
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
              class="w-full sm:w-auto"
              icon="i-lucide-save"
            >
              保存配置
            </UButton>

            <UButton
              type="button"
              variant="outline"
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
      <div class="divide-y divide-default text-sm">
        <div class="py-3 first:pt-0">
          <div class="text-sm font-medium text-muted">唯一配置</div>
          <div class="mt-1 font-medium text-highlighted">系统只维护一套全局考勤时间段</div>
        </div>

        <div class="py-3">
          <div class="text-sm font-medium text-muted">统一时区</div>
          <div class="mt-1 font-medium text-highlighted">管理端与设备端统一按上海时区处理</div>
        </div>

        <div class="py-3 last:pb-0">
          <div class="text-sm font-medium text-muted">生效方式</div>
          <div class="mt-1 font-medium text-highlighted">保存后新的上报会立即按最新窗口归类</div>
        </div>
      </div>
    </DataSurface>
  </div>
</template>
