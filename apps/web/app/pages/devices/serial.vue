<script setup lang="ts">
import { computed } from "vue";

import { useDeviceSerial } from "~/composables/useDeviceSerial";

definePageMeta({
  layout: "dashboard",
  middleware: ["auth"],
});

const serial = useDeviceSerial();

const headerBadges = computed(() => [
  {
    label: serial.connected.value ? "串口已连接" : "串口未连接",
    color: serial.connected.value ? ("success" as const) : ("warning" as const),
  },
]);

async function backToDevices() {
  await navigateTo("/devices");
}
</script>

<template>
  <UDashboardPanel id="device-serial">
    <template #header>
      <PageHeader title="串口配置" :badges="headerBadges">
        <template #actions>
          <UButton
            variant="outline"
            color="neutral"
            icon="i-lucide-arrow-left"
            @click="backToDevices"
          >
            返回设备管理
          </UButton>
        </template>
      </PageHeader>
    </template>

    <template #body>
      <div class="workspace-page-stack">
        <DeviceSerialWorkspace />
      </div>
    </template>
  </UDashboardPanel>
</template>
