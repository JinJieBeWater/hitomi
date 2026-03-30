<script setup lang="ts">
const props = defineProps<{
  open: boolean;
  device: {
    name: string;
    deviceCode: string;
    initialApiKey: string;
  } | null;
}>();

const emit = defineEmits<{
  "copy:code": [];
  "copy:key": [];
  "update:open": [value: boolean];
}>();

const items = computed(() => {
  if (!props.device) {
    return [];
  }

  return [
    {
      label: "设备名称",
      value: props.device.name,
      mono: false,
    },
    {
      label: "设备码",
      value: props.device.deviceCode,
      mono: true,
    },
    {
      label: "初始化密钥",
      value: props.device.initialApiKey,
      mono: true,
    },
  ];
});
</script>

<template>
  <UModal
    :open="props.open"
    :dismissible="false"
    :close="false"
    title="设备创建成功"
    description="请立即保存设备码和初始化密钥。初始化密钥只会显示这一次。"
    :content="{ class: 'sm:max-w-xl' }"
    @update:open="emit('update:open', $event)"
  >
    <template #body="{ close }">
      <div class="space-y-5">
        <div class="space-y-4">
          <div
            v-for="item in items"
            :key="item.label"
            class="border-b border-neutral-200/70 pb-4 last:border-b-0 last:pb-0 dark:border-neutral-800/80"
          >
            <div class="text-[11px] font-medium tracking-[0.16em] text-muted uppercase">
              {{ item.label }}
            </div>
            <div
              class="mt-2 text-sm text-highlighted sm:text-base"
              :class="item.mono ? 'break-all font-mono' : 'font-semibold tracking-tight'"
            >
              {{ item.value }}
            </div>
          </div>
        </div>

        <div class="flex flex-col gap-3 sm:flex-row sm:justify-between">
          <div class="flex flex-col gap-2 sm:flex-row">
            <UButton
              variant="outline"
              color="neutral"
              icon="i-lucide-copy"
              @click="emit('copy:code')"
            >
              复制设备码
            </UButton>

            <UButton
              variant="outline"
              color="neutral"
              icon="i-lucide-copy"
              @click="emit('copy:key')"
            >
              复制密钥
            </UButton>
          </div>

          <UButton icon="i-lucide-check" @click="close()">
            完成
          </UButton>
        </div>
      </div>
    </template>
  </UModal>
</template>
