<script setup lang="ts">
import { computed } from "vue";

import { formatDateTime } from "~/utils/format";

type EditableWifiProfile = {
  id: string;
  ssid: string;
  password: string;
  priority: number;
  lastSuccessAt?: number;
  disabled?: boolean;
};

const props = withDefaults(
  defineProps<{
    wifiProfiles: EditableWifiProfile[];
    backendOrigin: string;
    backendStatus?: "unknown" | "checking" | "ok" | "error";
    title?: string;
    description?: string;
    submitLabel: string;
    submitIcon?: string;
    submitDisabled?: boolean;
    submitLoading?: boolean;
    addLabel?: string;
    showBackendStatus?: boolean;
    validationMessage?: string;
    emptyMessage?: string;
    embedded?: boolean;
  }>(),
  {
    backendStatus: "unknown",
    title: "设备配置",
    description: "",
    submitIcon: "i-lucide-save",
    submitDisabled: false,
    submitLoading: false,
    addLabel: "添加 Wi-Fi",
    showBackendStatus: false,
    validationMessage: "",
    emptyMessage: "当前还没有 Wi-Fi 配置，点击右上角可添加新的网络。",
    embedded: false,
  },
);

const emit = defineEmits<{
  "update:backendOrigin": [string];
  "add:wifi-profile": [];
  "remove:wifi-profile": [string];
  submit: [];
}>();

const backendOriginModel = computed({
  get: () => props.backendOrigin,
  set: (value: string) => emit("update:backendOrigin", value),
});

const hasProfiles = computed(() => props.wifiProfiles.length > 0);
const rootClass = computed(() =>
  props.embedded
    ? "flex flex-col gap-3"
    : "flex flex-col gap-4 rounded-lg border border-default p-4",
);
const profileListClass = computed(() => (props.embedded ? "divide-y divide-default" : "flex flex-col gap-3"));
const profileItemClass = computed(() =>
  props.embedded
    ? "py-3 first:pt-0 last:pb-0"
    : "rounded-lg border border-default p-3",
);
const emptyClass = computed(() =>
  props.embedded
    ? "border border-dashed border-default px-0 py-3 text-sm text-muted"
    : "rounded-lg border border-dashed border-default px-4 py-5 text-sm text-muted",
);
</script>

<template>
  <div :class="rootClass">
    <div class="flex items-start justify-between gap-3">
      <div class="space-y-1">
        <div class="text-sm font-semibold text-highlighted">{{ title }}</div>
        <p v-if="description" class="text-xs text-muted">{{ description }}</p>
      </div>

      <UButton
        size="xs"
        variant="outline"
        icon="i-lucide-plus"
        @click="emit('add:wifi-profile')"
      >
        {{ addLabel }}
      </UButton>
    </div>

    <div v-if="hasProfiles" :class="profileListClass">
      <div
        v-for="(profile, index) in wifiProfiles"
        :key="profile.id"
        :class="profileItemClass"
      >
        <div class="mb-3 flex items-center justify-between gap-3">
          <div class="flex items-center gap-2">
            <span class="text-sm font-medium text-highlighted">网络 {{ index + 1 }}</span>
            <UBadge
              v-if="profile.disabled"
              label="已禁用"
              color="neutral"
              variant="soft"
            />
          </div>

          <UButton
            size="sm"
            color="error"
            variant="outline"
            icon="i-lucide-trash-2"
            @click="emit('remove:wifi-profile', profile.id)"
          />
        </div>

        <div class="grid gap-3 sm:grid-cols-[1fr_1fr_100px]">
          <UFormField label="SSID">
            <UInput v-model="profile.ssid" placeholder="网络名称" class="w-full" />
          </UFormField>

          <UFormField label="密码">
            <UInput
              v-model="profile.password"
              type="password"
              placeholder="密码"
              autocomplete="new-password"
              class="w-full"
            />
          </UFormField>

          <UFormField label="优先级">
            <UInput v-model.number="profile.priority" type="number" class="w-full" />
          </UFormField>
        </div>

        <div
          v-if="profile.lastSuccessAt || profile.disabled"
          class="mt-3 flex flex-wrap gap-x-4 gap-y-1 text-xs text-muted"
        >
          <span v-if="profile.lastSuccessAt">
            最近成功连接：{{ formatDateTime(profile.lastSuccessAt) }}
          </span>
          <span v-if="profile.disabled">保存时会继续保留禁用状态</span>
        </div>
      </div>
    </div>

    <div v-else :class="emptyClass">
      {{ emptyMessage }}
    </div>

    <UFormField label="后台地址">
      <div class="flex items-center gap-2">
        <UInput
          v-model="backendOriginModel"
          placeholder="http://192.168.x.x:3001"
          class="min-w-0 flex-1"
        />
        <UIcon
          v-if="showBackendStatus && backendStatus === 'checking'"
          name="i-lucide-loader-circle"
          class="size-4 shrink-0 animate-spin text-muted"
        />
        <UIcon
          v-else-if="showBackendStatus && backendStatus === 'ok'"
          name="i-lucide-circle-check"
          class="size-4 shrink-0 text-success"
        />
        <UIcon
          v-else-if="showBackendStatus && backendStatus === 'error'"
          name="i-lucide-circle-x"
          class="size-4 shrink-0 text-error"
        />
      </div>
      <p v-if="showBackendStatus && backendStatus === 'error'" class="mt-1 text-xs text-error">
        后台地址不可达，请检查 IP 和端口。
      </p>
    </UFormField>

    <p v-if="validationMessage" class="text-xs text-error">
      {{ validationMessage }}
    </p>

    <UButton
      :icon="submitIcon"
      :loading="submitLoading"
      :disabled="submitDisabled"
      class="w-full"
      @click="emit('submit')"
    >
      {{ submitLabel }}
    </UButton>
  </div>
</template>
