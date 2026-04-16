<script setup lang="ts">
const props = defineProps<{
  open: boolean;
  device: {
    name: string;
    deviceCode: string;
    bootstrapSerial: string;
    bootstrapSecret: string;
  } | null;
}>();

const emit = defineEmits<{
  "copy:code": [];
  "copy:bootstrap-serial": [];
  "copy:bootstrap-secret": [];
  "start-activation": [];
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
      label: "Bootstrap 序列号",
      value: props.device.bootstrapSerial,
      mono: true,
    },
    {
      label: "Bootstrap 密钥",
      value: props.device.bootstrapSecret,
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
    description="请立即保存设备码和 bootstrap 凭据。设备首配只使用 bootstrap 凭据，运行时 apiKey 会在激活成功后直接下发给设备。"
    :ui="{
      content: 'workspace-dialog-content sm:max-w-xl',
      header: 'workspace-dialog-header',
      body: 'workspace-dialog-body',
      footer: 'workspace-dialog-footer',
      title: 'workspace-dialog-title',
      description: 'workspace-dialog-description',
      close: 'workspace-dialog-close',
    }"
    @update:open="emit('update:open', $event)"
  >
    <template #body="{ close }">
      <div class="workspace-dialog-stack">
        <div class="workspace-dialog-panel space-y-4">
          <div
            v-for="item in items"
            :key="item.label"
            class="border-b border-neutral-200/70 pb-4 last:border-b-0 last:pb-0 dark:border-neutral-800/80"
          >
            <div class="workspace-section-label">{{ item.label }}</div>
            <div
              class="sm:text-base"
              :class="item.mono ? 'workspace-code-value font-mono' : 'workspace-data-value'"
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
              class="workspace-secondary-action"
              @click="emit('copy:code')"
            >
              复制设备码
            </UButton>
            <UButton
              variant="outline"
              color="neutral"
              icon="i-lucide-copy"
              class="workspace-secondary-action"
              @click="emit('copy:bootstrap-serial')"
            >
              复制序列号
            </UButton>

            <UButton
              variant="outline"
              color="neutral"
              icon="i-lucide-copy"
              class="workspace-secondary-action"
              @click="emit('copy:bootstrap-secret')"
            >
              复制 Bootstrap 密钥
            </UButton>

            <UButton
              color="primary"
              icon="i-lucide-usb"
              class="workspace-primary-action"
              @click="emit('start-activation')"
            >
              开始激活
            </UButton>
          </div>

          <UButton icon="i-lucide-check" variant="outline" class="workspace-secondary-action" @click="close()">
            完成
          </UButton>
        </div>
      </div>
    </template>
  </UModal>
</template>
