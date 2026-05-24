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
  "copy:all": [];
  "copy:code": [];
  "copy:bootstrap-serial": [];
  "copy:bootstrap-secret": [];
  "start-activation": [];
  "update:open": [value: boolean];
}>();

type CopyEvent = "copy:code" | "copy:bootstrap-serial" | "copy:bootstrap-secret";
type CredentialItem = {
  label: string;
  value: string;
  mono: boolean;
  copyLabel?: string;
  copyEvent?: CopyEvent;
};

const items = computed<CredentialItem[]>(() => {
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
      copyLabel: "复制设备码",
      copyEvent: "copy:code",
    },
    {
      label: "Bootstrap 序列号",
      value: props.device.bootstrapSerial,
      mono: true,
      copyLabel: "复制序列号",
      copyEvent: "copy:bootstrap-serial",
    },
    {
      label: "Bootstrap 密钥",
      value: props.device.bootstrapSecret,
      mono: true,
      copyLabel: "复制密钥",
      copyEvent: "copy:bootstrap-secret",
    },
  ];
});

function copyItem(event: CopyEvent) {
  if (event === "copy:code") {
    emit("copy:code");
    return;
  }

  if (event === "copy:bootstrap-serial") {
    emit("copy:bootstrap-serial");
    return;
  }

  emit("copy:bootstrap-secret");
}
</script>

<template>
  <UModal
    :open="props.open"
    :dismissible="false"
    :close="false"
    title="设备创建成功"
    description="请立即保存设备码和 bootstrap 凭据。设备首配只使用 bootstrap 凭据，运行时 apiKey 会在激活成功后直接下发给设备。"
    :ui="{ content: 'sm:max-w-xl' }"
    @update:open="emit('update:open', $event)"
  >
    <template #body="{ close }">
      <div class="flex flex-col gap-4">
        <div class="space-y-4 rounded-lg border border-default bg-default p-4">
          <div
            v-for="item in items"
            :key="item.label"
            class="border-b border-neutral-200/70 pb-4 last:border-b-0 last:pb-0 dark:border-neutral-800/80"
          >
            <div class="flex items-start justify-between gap-3">
              <div class="min-w-0 flex-1">
                <div class="workspace-section-label">{{ item.label }}</div>
                <div
                  class="sm:text-base"
                  :class="item.mono ? 'workspace-code-value font-mono' : 'workspace-data-value'"
                >
                  {{ item.value }}
                </div>
              </div>

              <UButton
                v-if="item.copyEvent"
                size="xs"
                variant="ghost"
                color="neutral"
                icon="i-lucide-copy"
                :aria-label="item.copyLabel"
                @click="copyItem(item.copyEvent)"
              >
                复制
              </UButton>
            </div>
          </div>
        </div>

        <div class="flex flex-col gap-3 sm:flex-row sm:items-center sm:justify-between">
          <UButton
            color="primary"
            icon="i-lucide-copy"
            @click="emit('copy:all')"
          >
            复制全部首配信息
          </UButton>

          <div class="flex flex-col gap-2 sm:flex-row sm:justify-end">
            <UButton
              variant="outline"
              color="neutral"
              icon="i-lucide-usb"
              @click="emit('start-activation')"
            >
              开始激活
            </UButton>

            <UButton icon="i-lucide-check" variant="ghost" color="neutral" @click="close()">
              完成
            </UButton>
          </div>
        </div>
      </div>
    </template>
  </UModal>
</template>
