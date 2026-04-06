<script setup lang="ts">
import { useMutation } from "@tanstack/vue-query";
import { computed, onBeforeUnmount, ref, watch } from "vue";

import {
  type ActivationWizardDevice,
  useDeviceActivationWizard,
} from "~/composables/useDeviceActivationWizard";
import { type DeviceSerialSummary, useDeviceSerial } from "~/composables/useDeviceSerial";

const props = defineProps<{
  open: boolean;
  device: ActivationWizardDevice | null;
}>();

const emit = defineEmits<{
  "update:open": [value: boolean];
  completed: [];
}>();

const { $orpc } = useNuxtApp();
const toast = useToast();
const serial = useDeviceSerial();
const wizard = useDeviceActivationWizard();
const issueActivation = useMutation($orpc.device.issueActivation.mutationOptions());

function createLocalId() {
  const randomUuid = globalThis.crypto?.randomUUID?.();
  if (randomUuid) {
    return randomUuid;
  }

  return `local_${Date.now()}_${Math.random().toString(36).slice(2, 10)}`;
}

const backendOrigin = ref("");
const bootstrapSerial = ref("");
const bootstrapSecret = ref("");
const lastSummary = ref<DeviceSerialSummary | null>(null);
const polling = ref(false);
const wifiProfiles = ref([
  {
    id: createLocalId(),
    ssid: "",
    password: "",
    priority: 100,
  },
]);

const fallbackCommands = computed(() => [
  `{"type":"set_wifi_profiles","profiles":[${wifiProfiles.value
    .filter((item) => item.ssid.trim() && item.password.trim())
    .map((item) =>
      JSON.stringify({
        ssid: item.ssid.trim(),
        password: item.password.trim(),
        priority: item.priority,
      }),
    )
    .join(",")}]}`,
  `{"type":"set_backend_origin","origin":"${backendOrigin.value.trim()}"}`,
  `{"type":"set_bootstrap_identity","deviceSerial":"${bootstrapSerial.value.trim()}","bootstrapSecret":"${bootstrapSecret.value.trim()}"}`,
]);

const canWriteConfig = computed(() => {
  return (
    serial.connected.value &&
    wifiProfiles.value.some((item) => item.ssid.trim() && item.password.trim()) &&
    backendOrigin.value.trim() &&
    bootstrapSerial.value.trim() &&
    bootstrapSecret.value.trim()
  );
});

function resetLocalState() {
  backendOrigin.value = import.meta.client ? window.location.origin : "";
  bootstrapSerial.value = props.device?.bootstrapSerial ?? "";
  bootstrapSecret.value = props.device?.bootstrapSecret ?? "";
  lastSummary.value = null;
  polling.value = false;
  wifiProfiles.value = [
    {
      id: createLocalId(),
      ssid: "",
      password: "",
      priority: 100,
    },
  ];
  wizard.reset();
}

watch(
  () => props.open,
  async (open) => {
    if (!open) {
      polling.value = false;
      await serial.disconnect();
      wizard.reset();
      return;
    }

    resetLocalState();
  },
);

onBeforeUnmount(async () => {
  polling.value = false;
  await serial.disconnect();
});

function closeModal() {
  emit("update:open", false);
}

function addWifiProfile() {
  wifiProfiles.value.push({
    id: createLocalId(),
    ssid: "",
    password: "",
    priority: Math.max(10, 100 - wifiProfiles.value.length * 10),
  });
}

function removeWifiProfile(id: string) {
  if (wifiProfiles.value.length === 1) {
    return;
  }
  wifiProfiles.value = wifiProfiles.value.filter((item) => item.id !== id);
}

async function refreshSummary() {
  const response = await serial.sendCommand({ type: "get_config" });
  lastSummary.value = response.summary ?? null;
  return response.summary ?? null;
}

async function handleRefreshSummary() {
  try {
    await refreshSummary();
  } catch (error: any) {
    wizard.fail(error?.message || "读取设备摘要失败。");
  }
}

async function connectDevice() {
  try {
    await serial.connect();
    wizard.markConnected();
    await refreshSummary();
  } catch (error: any) {
    wizard.fail(error?.message || "连接设备失败。");
  }
}

async function writeProvisioning() {
  if (!props.device) {
    return;
  }

  try {
    wizard.markProvisioning();

    const profiles = wifiProfiles.value
      .filter((item) => item.ssid.trim() && item.password.trim())
      .map((item) => ({
        ssid: item.ssid.trim(),
        password: item.password.trim(),
        priority: item.priority,
      }));

    await serial.sendCommand({
      type: "set_wifi_profiles",
      profiles,
    });
    await serial.sendCommand({
      type: "set_backend_origin",
      origin: backendOrigin.value.trim(),
    });
    await serial.sendCommand({
      type: "set_bootstrap_identity",
      deviceSerial: bootstrapSerial.value.trim(),
      bootstrapSecret: bootstrapSecret.value.trim(),
    });
    await refreshSummary();
    wizard.markProvisioned();
  } catch (error: any) {
    wizard.fail(error?.message || "写入设备配置失败。");
  }
}

async function pollForActivation() {
  polling.value = true;
  try {
    for (let attempt = 0; attempt < 12; attempt += 1) {
      if (!polling.value) {
        return;
      }

      const summary = await refreshSummary();
      if (summary?.activationState === "activated") {
        wizard.markActivated();
        emit("completed");
        toast.add({ title: "设备已完成激活" });
        return;
      }

      await new Promise((resolve) => setTimeout(resolve, 2000));
    }

    wizard.fail("设备尚未完成激活，请稍后重新检查。");
  } finally {
    polling.value = false;
  }
}

async function handleIssueActivation() {
  if (!props.device) {
    return;
  }

  try {
    await issueActivation.mutateAsync({
      deviceId: props.device.id,
    });
    wizard.markActivationIssued();
    await pollForActivation();
  } catch (error: any) {
    wizard.fail(error?.message || "发放激活失败。");
  }
}
</script>

<template>
  <UModal
    :open="props.open"
    :ui="{ content: 'sm:max-w-4xl' }"
    title="USB 设备激活向导"
    description="在后台页面内直接通过浏览器连接设备串口，完成首配与激活。首版仅支持 Chromium 浏览器。"
    @update:open="emit('update:open', $event)"
  >
    <template #body>
      <div class="space-y-5">
        <UAlert
          v-if="wizard.errorMessage.value"
          color="error"
          icon="i-lucide-alert-circle"
          title="执行失败"
          :description="wizard.errorMessage.value"
        />

        <div
          v-if="!serial.supported.value"
          class="rounded-2xl border border-amber-200/80 bg-amber-50/80 p-4 text-sm dark:border-amber-800/60 dark:bg-amber-950/40"
        >
          <div class="flex gap-3">
            <UIcon name="i-lucide-monitor-warning" class="mt-0.5 size-5 shrink-0 text-amber-600 dark:text-amber-400" />
            <div class="space-y-3">
              <div>
                <div class="font-semibold text-amber-900 dark:text-amber-200">当前页面不支持 Web Serial</div>
                <div class="mt-1 text-amber-800 dark:text-amber-300">
                  Web Serial API 仅在 <strong>安全上下文</strong>（HTTPS 或 localhost）下可用。
                  通过局域网 IP 访问时需手动开启 Chrome 实验性标志。
                </div>
              </div>
              <div class="space-y-1.5">
                <div class="font-medium text-amber-900 dark:text-amber-200">解决方法：</div>
                <ol class="list-inside list-decimal space-y-1 text-amber-800 dark:text-amber-300">
                  <li>在 Chrome 地址栏打开 <code class="rounded bg-amber-100 px-1 font-mono text-xs dark:bg-amber-900/60">chrome://flags/#unsafely-treat-insecure-origin-as-secure</code></li>
                  <li>将当前页面地址（如 <code class="rounded bg-amber-100 px-1 font-mono text-xs dark:bg-amber-900/60">http://192.168.x.x:3000</code>）填入输入框</li>
                  <li>点击 <strong>Enabled</strong>，然后点击 <strong>Relaunch</strong> 重启浏览器</li>
                  <li>重新打开本页面</li>
                </ol>
              </div>
              <div class="text-amber-700 dark:text-amber-400">
                或者直接在运行服务的机器上用 <code class="rounded bg-amber-100 px-1 font-mono text-xs dark:bg-amber-900/60">http://localhost</code> 访问，无需额外配置。
              </div>
            </div>
          </div>
        </div>

        <div class="grid gap-4 lg:grid-cols-[1.1fr_0.9fr]">
          <div class="space-y-4">
            <div class="rounded-2xl border border-neutral-200/70 p-4 dark:border-neutral-800/80">
              <div class="text-xs font-medium tracking-[0.16em] text-muted uppercase">目标设备</div>
              <div class="mt-2 text-sm font-semibold text-highlighted">
                {{ props.device?.name || "未选择设备" }}
              </div>
              <div class="mt-1 text-xs text-muted">
                设备码：{{ props.device?.deviceCode || "—" }}
              </div>
            </div>

            <div class="rounded-2xl border border-neutral-200/70 p-4 dark:border-neutral-800/80">
              <div class="flex items-center justify-between gap-3">
                <div>
                  <div class="text-xs font-medium tracking-[0.16em] text-muted uppercase">
                    步骤 1
                  </div>
                  <div class="mt-1 text-sm font-semibold text-highlighted">连接 USB 串口</div>
                </div>
                <UButton
                  color="primary"
                  icon="i-lucide-usb"
                  :disabled="!serial.supported.value || serial.connected.value"
                  @click="connectDevice"
                >
                  {{ serial.connected.value ? "已连接" : "连接设备" }}
                </UButton>
              </div>
              <p class="mt-3 text-sm text-toned">{{ wizard.statusMessage.value }}</p>
              <div class="mt-3 flex flex-wrap gap-2">
                <UButton
                  size="sm"
                  variant="outline"
                  icon="i-lucide-refresh-cw"
                  :disabled="!serial.connected.value"
                  @click="handleRefreshSummary"
                >
                  读取设备摘要
                </UButton>
                <UButton
                  size="sm"
                  variant="ghost"
                  color="neutral"
                  icon="i-lucide-unplug"
                  :disabled="!serial.connected.value"
                  @click="serial.disconnect"
                >
                  断开
                </UButton>
              </div>
            </div>

            <div class="rounded-2xl border border-neutral-200/70 p-4 dark:border-neutral-800/80">
              <div class="text-xs font-medium tracking-[0.16em] text-muted uppercase">步骤 2</div>
              <div class="mt-1 text-sm font-semibold text-highlighted">写入网络与引导身份</div>

              <div class="mt-4 space-y-4">
                <div class="space-y-3">
                  <div class="flex items-center justify-between gap-3">
                    <div class="text-sm font-medium text-highlighted">Wi‑Fi Profiles</div>
                    <UButton
                      size="xs"
                      variant="outline"
                      icon="i-lucide-plus"
                      @click="addWifiProfile"
                    >
                      添加网络
                    </UButton>
                  </div>

                  <div
                    v-for="profile in wifiProfiles"
                    :key="profile.id"
                    class="rounded-2xl border border-neutral-200/70 p-3 dark:border-neutral-800/80"
                  >
                    <div class="grid gap-3 md:grid-cols-[minmax(0,1fr)_minmax(0,1fr)_110px_auto]">
                      <UFormField label="SSID">
                        <UInput v-model="profile.ssid" placeholder="例如 Lab-Wifi" class="w-full" />
                      </UFormField>
                      <UFormField label="密码">
                        <UInput
                          v-model="profile.password"
                          type="password"
                          placeholder="输入 Wi‑Fi 密码"
                          class="w-full"
                        />
                      </UFormField>
                      <UFormField label="优先级">
                        <UInput v-model.number="profile.priority" type="number" class="w-full" />
                      </UFormField>
                      <div class="flex items-end">
                        <UButton
                          size="sm"
                          color="error"
                          variant="outline"
                          icon="i-lucide-trash-2"
                          :disabled="wifiProfiles.length === 1"
                          @click="removeWifiProfile(profile.id)"
                        >
                          删除
                        </UButton>
                      </div>
                    </div>
                  </div>
                </div>

                <UFormField label="后台地址">
                  <UInput
                    v-model="backendOrigin"
                    placeholder="http://127.0.0.1:3001"
                    class="w-full"
                  />
                </UFormField>

                <div class="grid gap-3 md:grid-cols-2">
                  <UFormField label="Bootstrap 序列号">
                    <UInput v-model="bootstrapSerial" placeholder="BOOT-001" class="w-full" />
                  </UFormField>
                  <UFormField label="Bootstrap 密钥">
                    <UInput
                      v-model="bootstrapSecret"
                      placeholder="输入 Bootstrap 密钥"
                      class="w-full"
                    />
                  </UFormField>
                </div>

                <UButton
                  color="primary"
                  icon="i-lucide-download"
                  :disabled="!canWriteConfig || wizard.provisioningComplete.value"
                  @click="writeProvisioning"
                >
                  写入设备配置
                </UButton>
              </div>
            </div>

            <div class="rounded-2xl border border-neutral-200/70 p-4 dark:border-neutral-800/80">
              <div class="text-xs font-medium tracking-[0.16em] text-muted uppercase">步骤 3</div>
              <div class="mt-1 text-sm font-semibold text-highlighted">发放激活并等待设备完成</div>
              <div class="mt-3 flex flex-wrap gap-2">
                <UButton
                  color="primary"
                  icon="i-lucide-key-round"
                  :loading="issueActivation.isPending.value || polling"
                  :disabled="!wizard.provisioningComplete.value || wizard.isComplete.value"
                  @click="handleIssueActivation"
                >
                  {{ wizard.activationIssued.value ? "等待设备领取…" : "发放激活" }}
                </UButton>
                <UButton
                  size="sm"
                  variant="outline"
                  icon="i-lucide-refresh-cw"
                  :disabled="!serial.connected.value"
                  @click="pollForActivation"
                >
                  检查设备状态
                </UButton>
              </div>
            </div>
          </div>

          <div class="space-y-4">
            <div class="rounded-2xl border border-neutral-200/70 p-4 dark:border-neutral-800/80">
              <div class="text-xs font-medium tracking-[0.16em] text-muted uppercase">设备摘要</div>
              <div v-if="lastSummary" class="mt-3 space-y-2 text-sm text-toned">
                <div>Schema：{{ lastSummary.schemaVersion }}</div>
                <div>Wi‑Fi 数量：{{ lastSummary.wifiProfileCount }}</div>
                <div>后台地址：{{ lastSummary.backendOrigin || "未配置" }}</div>
                <div>设备序列号：{{ lastSummary.deviceSerial || "未配置" }}</div>
                <div>激活状态：{{ lastSummary.activationState }}</div>
                <div>运行时设备码：{{ lastSummary.deviceCode || "未领取" }}</div>
              </div>
              <p v-else class="mt-3 text-sm text-muted">连接设备后可以读取当前配置摘要。</p>
            </div>

            <div class="rounded-2xl border border-neutral-200/70 p-4 dark:border-neutral-800/80">
              <div class="text-xs font-medium tracking-[0.16em] text-muted uppercase">
                Fallback 指引
              </div>
              <p class="mt-3 text-sm text-toned">
                如果浏览器不支持 Web Serial，可回退到串口工具手动发送以下命令：
              </p>
              <div
                class="mt-3 space-y-2 rounded-2xl bg-neutral-50 p-3 font-mono text-xs dark:bg-neutral-900/60"
              >
                <div v-for="command in fallbackCommands" :key="command" class="break-all">
                  {{ command }}
                </div>
              </div>
            </div>

            <div class="rounded-2xl border border-neutral-200/70 p-4 dark:border-neutral-800/80">
              <div class="text-xs font-medium tracking-[0.16em] text-muted uppercase">当前状态</div>
              <div class="mt-3 text-sm text-toned">{{ wizard.statusMessage.value }}</div>
              <p v-if="wizard.isComplete.value" class="mt-3 text-sm text-success">
                设备已完成激活，可以关闭向导。
              </p>
            </div>
          </div>
        </div>
      </div>
    </template>

    <template #footer>
      <div class="flex justify-end">
        <UButton variant="ghost" color="neutral" @click="closeModal"> 关闭 </UButton>
      </div>
    </template>
  </UModal>
</template>
