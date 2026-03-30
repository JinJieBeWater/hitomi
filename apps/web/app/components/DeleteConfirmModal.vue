<script setup lang="ts">
const props = withDefaults(
  defineProps<{
    open: boolean;
    entityLabel: string;
    identifierLabel: string;
    impact: {
      name: string;
      identifierValue: string;
      faceProfileCount: number;
      attendanceRecordCount: number;
      confirmText: string;
    } | null;
    loading?: boolean;
    submitting?: boolean;
    error?: string;
  }>(),
  {
    loading: false,
    submitting: false,
    error: "",
  },
);

const emit = defineEmits<{
  confirm: [value: string];
  "update:open": [value: boolean];
}>();

const confirmValue = ref("");

const isMatched = computed(() => {
  if (!props.impact) {
    return false;
  }

  return confirmValue.value.trim() === props.impact.confirmText;
});

watch(
  () => props.open,
  (open) => {
    if (!open) {
      confirmValue.value = "";
    }
  },
);

watch(
  () => props.impact?.confirmText,
  () => {
    confirmValue.value = "";
  },
);

function handleConfirm() {
  if (!props.impact || !isMatched.value || props.loading || props.submitting) {
    return;
  }

  emit("confirm", confirmValue.value.trim());
}
</script>

<template>
  <UModal
    :open="props.open"
    :dismissible="!props.submitting"
    :close="!props.submitting"
    :title="`确认删除${props.entityLabel}`"
    description="删除后不可恢复，相关录脸任务和考勤记录会一并移除。"
    :content="{ class: 'sm:max-w-xl' }"
    @update:open="emit('update:open', $event)"
  >
    <template #body>
      <div class="space-y-5">
        <div v-if="props.loading" class="space-y-3">
          <USkeleton class="h-5 w-40 rounded-full" />
          <USkeleton class="h-16 w-full rounded-2xl" />
          <USkeleton class="h-16 w-full rounded-2xl" />
          <USkeleton class="h-10 w-full rounded-2xl" />
        </div>

        <template v-else-if="props.impact">
          <div class="space-y-4">
            <div class="border-b border-neutral-200/70 pb-4 dark:border-neutral-800/80">
              <div class="text-[11px] font-medium tracking-[0.16em] text-muted uppercase">{{ props.entityLabel }}</div>
              <div class="mt-2 text-base font-semibold tracking-tight text-highlighted">{{ props.impact.name }}</div>
              <div class="mt-1 text-sm text-toned">
                {{ props.identifierLabel }}: {{ props.impact.identifierValue }}
              </div>
            </div>

            <div class="grid gap-3 sm:grid-cols-2">
              <div class="rounded-2xl border border-neutral-200/70 px-4 py-4 dark:border-neutral-800/80">
                <div class="text-[11px] font-medium tracking-[0.16em] text-muted uppercase">录脸任务</div>
                <div class="mt-2 text-2xl font-semibold tracking-tight text-highlighted">
                  {{ props.impact.faceProfileCount }}
                </div>
              </div>

              <div class="rounded-2xl border border-neutral-200/70 px-4 py-4 dark:border-neutral-800/80">
                <div class="text-[11px] font-medium tracking-[0.16em] text-muted uppercase">考勤记录</div>
                <div class="mt-2 text-2xl font-semibold tracking-tight text-highlighted">
                  {{ props.impact.attendanceRecordCount }}
                </div>
              </div>
            </div>
          </div>

          <UFormField :label="`请输入${props.identifierLabel}以确认删除`" required>
            <UInput
              v-model="confirmValue"
              data-testid="delete-confirm-input"
              :placeholder="props.impact.confirmText"
              class="w-full"
            />
          </UFormField>
        </template>

        <UAlert
          v-if="props.error"
          color="error"
          icon="i-lucide-alert-circle"
          title="操作失败"
          :description="props.error"
        />

        <div class="flex flex-col-reverse gap-3 sm:flex-row sm:justify-end">
          <UButton
            variant="ghost"
            color="neutral"
            :disabled="props.submitting"
            @click="emit('update:open', false)"
          >
            取消
          </UButton>

          <UButton
            color="error"
            icon="i-lucide-trash-2"
            data-testid="delete-confirm-submit-button"
            :loading="props.submitting"
            :disabled="!isMatched || props.loading || !props.impact"
            @click="handleConfirm()"
          >
            {{ `删除${props.entityLabel}` }}
          </UButton>
        </div>
      </div>
    </template>
  </UModal>
</template>
