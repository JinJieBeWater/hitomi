<script setup lang="ts">
import { useMutation, useQueryClient } from "@tanstack/vue-query";
import { computed, onBeforeUnmount, ref, watch } from "vue";

import {
  type DeviceSerialSummary,
  type DeviceSerialWifiProfile,
  useDeviceSerial,
} from "~/composables/useDeviceSerial";

const props = defineProps<{ open: boolean }>();
const emit = defineEmits<{ "update:open": [boolean] }>();

const { $orpc } = useNuxtApp();
const toast = useToast();
const serial = useDeviceSerial();
const queryClient = useQueryClient();

const createDevice = useMutation($orpc.device.create.mutationOptions());

type View = "idle" | "connecting" | "new_device" | "provision" | "device_info";
type Step = "create" | "config" | "writing" | "activating" | "done" | "action";
type WifiProfileFormItem = DeviceSerialWifiProfile & {
  id: string;
  originalSsid: string;
  originalPassword: string;
};

const view = ref<View>("idle");
const step = ref<Step>("action");
const errorMessage = ref("");
const statusMessage = ref("");
const busy = ref(false);
const polling = ref(false);
const confirmingReset = ref<"credentials" | "full" | null>(null);
const showProvisionForm = ref(false);
const showSerialLogs = ref(true);

const summary = ref<DeviceSerialSummary | null>(null);
const dbDevice = ref<{ id: string; name: string; deviceCode: string } | null>(null);
const createdDevice = ref<{ id: string; bootstrapSerial: string; bootstrapSecret: string } | null>(
  null,
);

const newDeviceName = ref("");
const backendOrigin = ref("");
const backendStatus = ref<"unknown" | "checking" | "ok" | "error">("unknown");
const wifiProfiles = ref<WifiProfileFormItem[]>([]);

const deviceId = computed(() => createdDevice.value?.id ?? dbDevice.value?.id ?? null);
const currentOrigin = computed(() => (import.meta.client ? window.location.origin : ""));
const hasConfiguredWifiProfile = computed(() =>
  wifiProfiles.value.some((profile) => profile.ssid.trim() && profile.password.trim()),
);
const hasIncompleteWifiProfile = computed(() =>
  wifiProfiles.value.some((profile) => {
    const ssid = profile.ssid.trim();
    const password = profile.password.trim();

    return Boolean(ssid) !== Boolean(password);
  }),
);
const canWriteActivationConfig = computed(
  () =>
    serial.connected.value &&
    Boolean(backendOrigin.value.trim()) &&
    hasConfiguredWifiProfile.value &&
    !hasIncompleteWifiProfile.value,
);
const canSaveCurrentConfig = computed(
  () =>
    serial.connected.value &&
    Boolean(backendOrigin.value.trim()) &&
    !hasIncompleteWifiProfile.value,
);
const wifiDetailsUnavailable = computed(
  () => (summary.value?.wifiProfileCount ?? 0) > 0 && wifiProfiles.value.length === 0,
);
const wifiValidationMessage = computed(() =>
  hasIncompleteWifiProfile.value ? "请完整填写每一项 Wi-Fi 配置后再保存。" : "",
);
const wifiEmptyMessage = computed(() => {
  if (!wifiDetailsUnavailable.value) {
    return "当前还没有 Wi-Fi 配置，点击右上角可添加新的网络。";
  }

  return `设备上已有 ${summary.value?.wifiProfileCount ?? 0} 项 Wi‑Fi 配置，但当前固件没有返回可编辑详情。你可以手动新增并覆盖写入，或升级设备固件后重新读取。`;
});

function mkId() {
  return (
    globalThis.crypto?.randomUUID?.() ??
    `id_${Date.now()}_${Math.random().toString(36).slice(2)}`
  );
}

function defaultWifiPriority(index: number) {
  return Math.max(10, 100 - index * 10);
}

function createWifiProfileForm(
  profile: Partial<DeviceSerialWifiProfile> = {},
  index = wifiProfiles.value.length,
): WifiProfileFormItem {
  const ssid = profile.ssid ?? "";
  const password = profile.password ?? "";

  return {
    id: mkId(),
    ssid,
    password,
    priority: profile.priority ?? defaultWifiPriority(index),
    lastSuccessAt: profile.lastSuccessAt ?? 0,
    disabled: profile.disabled ?? false,
    originalSsid: ssid,
    originalPassword: password,
  };
}

function applyConfigDraftFromSummary(
  nextSummary: DeviceSerialSummary | null | undefined,
  options: { ensureWifiInput?: boolean } = {},
) {
  if (!nextSummary) {
    return;
  }

  if (nextSummary.backendOrigin.trim()) {
    backendOrigin.value = nextSummary.backendOrigin.trim();
  }

  const nextProfiles = nextSummary.wifiProfiles.map((profile, index) =>
    createWifiProfileForm(profile, index),
  );

  wifiProfiles.value =
    nextProfiles.length > 0
      ? nextProfiles
      : options.ensureWifiInput
        ? [createWifiProfileForm({}, 0)]
        : [];
}

function resetState() {
  view.value = "idle";
  step.value = "action";
  errorMessage.value = "";
  statusMessage.value = "";
  busy.value = false;
  polling.value = false;
  confirmingReset.value = null;
  showProvisionForm.value = false;
  serial.clearLogs();
  summary.value = null;
  dbDevice.value = null;
  createdDevice.value = null;
  newDeviceName.value = "";
  backendOrigin.value = import.meta.client ? window.location.origin : "";
  backendStatus.value = "unknown";
  wifiProfiles.value = [createWifiProfileForm({}, 0)];
  if (import.meta.client && backendOrigin.value) {
    checkBackendOrigin();
  }
}

function addWifiProfile() {
  wifiProfiles.value.push(createWifiProfileForm({}, wifiProfiles.value.length));
}

function removeWifiProfile(id: string) {
  wifiProfiles.value = wifiProfiles.value.filter((profile) => profile.id !== id);
}

function buildWifiProfilesPayload() {
  if (hasIncompleteWifiProfile.value) {
    throw new Error("请完整填写每一项 Wi-Fi 配置。");
  }

  return wifiProfiles.value
    .filter((profile) => profile.ssid.trim() && profile.password.trim())
    .map((profile, index) => {
      const ssid = profile.ssid.trim();
      const password = profile.password.trim();
      const keepLastSuccessAt =
        profile.originalSsid === ssid && profile.originalPassword === password;

      return {
        ssid,
        password,
        priority: Number.isFinite(profile.priority) ? profile.priority : defaultWifiPriority(index),
        lastSuccessAt: keepLastSuccessAt ? profile.lastSuccessAt : 0,
        disabled: profile.disabled,
      };
    });
}

let backendCheckTimer: ReturnType<typeof setTimeout> | null = null;

async function checkBackendOrigin() {
  const url = backendOrigin.value.trim();
  if (!url) {
    backendStatus.value = "unknown";
    return;
  }

  backendStatus.value = "checking";

  try {
    const response = await fetch(`${url}/rpc/healthCheck`, {
      method: "POST",
      signal: AbortSignal.timeout(4000),
    });
    backendStatus.value = response.ok ? "ok" : "error";
  } catch {
    backendStatus.value = "error";
  }
}

watch(backendOrigin, () => {
  backendStatus.value = "unknown";
  if (backendCheckTimer) {
    clearTimeout(backendCheckTimer);
  }
  backendCheckTimer = setTimeout(checkBackendOrigin, 600);
});

async function refreshSummary(options: { syncEditor?: boolean; ensureWifiInput?: boolean } = {}) {
  const response = await serial.sendCommand({ type: "get_config" });
  summary.value = response.summary ?? null;

  if (options.syncEditor) {
    applyConfigDraftFromSummary(summary.value, { ensureWifiInput: options.ensureWifiInput });
  }

  return summary.value;
}

async function connect() {
  errorMessage.value = "";
  view.value = "connecting";

  try {
    await serial.connect();
    statusMessage.value = "正在读取设备信息…";
    const nextSummary = await refreshSummary();

    if (!nextSummary) {
      throw new Error("未能读取设备配置摘要。");
    }

    applyConfigDraftFromSummary(nextSummary, {
      ensureWifiInput: nextSummary.activationState !== "activated",
    });

    if (nextSummary.activationState === "unconfigured") {
      view.value = "new_device";
      step.value = "create";
      statusMessage.value = "全新设备，请先创建设备记录。";
      return;
    }

    const found = await queryClient.fetchQuery(
      $orpc.device.findByBootstrapSerial.queryOptions({
        input: { serial: nextSummary.deviceSerial },
      }),
    );
    dbDevice.value = found ? { id: found.id, name: found.name, deviceCode: found.deviceCode } : null;

    if (nextSummary.activationState === "activated") {
      view.value = "device_info";
      statusMessage.value = found ? `已识别：${found.name}` : "已激活设备（后台未找到记录）";
      return;
    }

    view.value = "provision";
    step.value = "action";
    statusMessage.value = found
      ? `已识别：${found.name}，待激活`
      : "设备已有 Bootstrap 配置但后台未找到记录，可创建新设备。";
  } catch (error: any) {
    errorMessage.value = error?.message || "连接失败。";
    view.value = "idle";
  }
}

async function handleCreateDevice() {
  if (!newDeviceName.value.trim()) {
    return;
  }

  busy.value = true;
  errorMessage.value = "";

  try {
    const result = await createDevice.mutateAsync({ name: newDeviceName.value.trim() });
    createdDevice.value = {
      id: result.device.id,
      bootstrapSerial: result.bootstrapSerial,
      bootstrapSecret: result.bootstrapSecret,
    };
    dbDevice.value = {
      id: result.device.id,
      name: result.device.name,
      deviceCode: result.device.deviceCode,
    };
    step.value = "config";
    statusMessage.value = `设备「${result.device.name}」已创建，请填写网络配置。`;
    if (wifiProfiles.value.length === 0) {
      wifiProfiles.value = [createWifiProfileForm({}, 0)];
    }
  } catch (error: any) {
    errorMessage.value = error?.message || "创建设备失败。";
  } finally {
    busy.value = false;
  }
}

async function writeConfig(options: {
  writeBootstrap?: boolean;
  waitForActivation?: boolean;
  requireWifi?: boolean;
  pendingMessage: string;
  successMessage: string;
  successToastTitle: string;
}) {
  if (!deviceId.value) {
    return;
  }

  const profiles = buildWifiProfilesPayload();
  const origin = backendOrigin.value.trim();

  if (!origin) {
    throw new Error("请填写后台地址。");
  }

  if (options.requireWifi && profiles.length === 0) {
    throw new Error("请至少配置一个 Wi-Fi 网络。");
  }

  busy.value = true;
  errorMessage.value = "";
  if (options.waitForActivation) {
    step.value = "writing";
  }
  statusMessage.value = options.pendingMessage;

  try {
    await serial.sendCommand({ type: "set_wifi_profiles", profiles });
    await serial.sendCommand({ type: "set_backend_origin", origin });

    if (options.writeBootstrap && createdDevice.value) {
      await serial.sendCommand({
        type: "set_bootstrap_identity",
        deviceSerial: createdDevice.value.bootstrapSerial,
        bootstrapSecret: createdDevice.value.bootstrapSecret,
      });
    }

    await refreshSummary({ syncEditor: true, ensureWifiInput: options.requireWifi });

    if (options.waitForActivation) {
      await pollUntilActivated();
      return;
    }

    statusMessage.value = options.successMessage;
    toast.add({ title: options.successToastTitle });
  } catch (error: any) {
    errorMessage.value = error?.message || "写入配置失败。";
    if (options.waitForActivation) {
      step.value = "config";
    }
  } finally {
    busy.value = false;
  }
}

async function writeAndActivate(writeBootstrap: boolean) {
  await writeConfig({
    writeBootstrap,
    waitForActivation: true,
    requireWifi: true,
    pendingMessage: "正在写入配置…",
    successMessage: "设备激活成功！",
    successToastTitle: "设备已激活",
  });
}

async function saveCurrentConfig() {
  await writeConfig({
    waitForActivation: false,
    requireWifi: false,
    pendingMessage: "正在保存设备配置…",
    successMessage: "设备配置已保存，设备将按新参数重新连接。",
    successToastTitle: "设备配置已保存",
  });
}

async function reloadCurrentConfig() {
  busy.value = true;
  errorMessage.value = "";
  statusMessage.value = "正在读取设备当前配置…";

  try {
    const nextSummary = await refreshSummary({ syncEditor: true });
    if (!nextSummary) {
      throw new Error("未能读取设备当前配置。");
    }

    statusMessage.value = "已同步设备当前配置。";
    toast.add({ title: "已刷新设备配置" });
  } catch (error: any) {
    errorMessage.value = error?.message || "读取设备配置失败。";
  } finally {
    busy.value = false;
  }
}

async function pollUntilActivated() {
  step.value = "activating";
  statusMessage.value = "等待设备自动激活…";
  polling.value = true;

  try {
    for (let i = 0; i < 30; i++) {
      if (!polling.value) {
        return;
      }

      const nextSummary = await refreshSummary();
      if (nextSummary?.activationState === "activated") {
        applyConfigDraftFromSummary(nextSummary);
        step.value = "done";
        view.value = "device_info";
        statusMessage.value = "设备激活成功！";
        toast.add({ title: "设备已激活" });
        await queryClient.invalidateQueries({
          queryKey: $orpc.device.list.queryOptions({ input: {} }).queryKey,
        });
        return;
      }

      await new Promise((resolve) => setTimeout(resolve, 2000));
    }

    errorMessage.value = "等待超时，设备未在预期时间内完成激活。";
    step.value = "action";
  } catch (error: any) {
    errorMessage.value = error?.message || "激活失败。";
    step.value = "action";
  } finally {
    polling.value = false;
  }
}

async function executeReset(type: "credentials" | "full") {
  if (!deviceId.value) {
    return;
  }

  busy.value = true;
  errorMessage.value = "";
  confirmingReset.value = null;

  try {
    if (type === "credentials") {
      await serial.sendCommand({ type: "clear_runtime_credentials" });
    } else {
      await serial.sendCommand({ type: "reset_device_config" });
    }

    await refreshSummary({
      syncEditor: true,
      ensureWifiInput: type === "full",
    });

    statusMessage.value =
      type === "credentials" ? "凭证已清除，设备将重新激活。" : "设备已完全重置，进入待激活状态。";
    toast.add({ title: type === "credentials" ? "凭证已清除" : "设备已完全重置" });
    view.value = "provision";
    step.value = "action";
    await pollUntilActivated();
  } catch (error: any) {
    errorMessage.value = error?.message || "重置失败。";
  } finally {
    busy.value = false;
  }
}

watch(
  () => props.open,
  async (open) => {
    if (!open) {
      polling.value = false;
      await serial.disconnect();
      resetState();
      return;
    }

    resetState();
  },
);

onBeforeUnmount(async () => {
  polling.value = false;
  await serial.disconnect();
});
</script>

<template>
  <USlideover
    :open="props.open"
    title="USB 设备管理"
    description="插上 USB 线后连接设备，自动识别状态并引导配置、激活或重置。"
    side="right"
    @update:open="emit('update:open', $event)"
  >
    <template #body>
      <div class="space-y-5">

        <!-- Web Serial unsupported -->
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
                  Web Serial API 仅在<strong>安全上下文</strong>（HTTPS 或 localhost）下可用。
                  通过局域网 IP 访问时需手动开启 Chrome 实验性标志。
                </div>
              </div>
              <div class="space-y-1.5">
                <div class="font-medium text-amber-900 dark:text-amber-200">解决方法：</div>
                <ol class="list-inside list-decimal space-y-1 text-amber-800 dark:text-amber-300">
                  <li>在 Chrome 地址栏打开 <code class="rounded bg-amber-100 px-1 font-mono text-xs dark:bg-amber-900/60">chrome://flags/#unsafely-treat-insecure-origin-as-secure</code></li>
                  <li>将当前页面地址（<code class="rounded bg-amber-100 px-1 font-mono text-xs dark:bg-amber-900/60">{{ currentOrigin }}</code>）填入输入框</li>
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

        <!-- Error -->
        <UAlert v-if="errorMessage" color="error" icon="i-lucide-alert-circle" title="操作失败" :description="errorMessage" />

        <!-- ════ IDLE ════ -->
        <template v-if="view === 'idle'">
          <p class="text-sm text-toned">用 USB 线连接设备后点击下方按钮，系统将自动读取状态并引导后续操作。</p>
          <UButton icon="i-lucide-usb" :disabled="!serial.supported.value" class="w-full rounded-2xl" @click="connect">
            连接设备
          </UButton>
        </template>

        <!-- ════ CONNECTING ════ -->
        <div v-else-if="view === 'connecting'" class="flex items-center gap-3 text-sm text-toned">
          <UIcon name="i-lucide-loader-circle" class="size-4 animate-spin" />
          {{ statusMessage || "正在连接…" }}
        </div>

        <!-- ════ NEW DEVICE ════ -->
        <template v-else-if="view === 'new_device'">
          <div class="flex items-center gap-2 rounded-2xl border border-primary-200/70 bg-primary-50/50 px-4 py-3 dark:border-primary-800/50 dark:bg-primary-950/30">
            <UIcon name="i-lucide-sparkles" class="size-4 text-primary" />
            <span class="text-sm font-medium text-primary-700 dark:text-primary-300">全新设备 — 需要配置</span>
          </div>

          <!-- step: create device -->
          <div v-if="step === 'create'" class="space-y-4">
            <UFormField label="设备名称" required>
              <UInput v-model="newDeviceName" placeholder="例如 前台一号机" icon="i-lucide-monitor" class="w-full" @keydown.enter="handleCreateDevice" />
            </UFormField>
            <UButton icon="i-lucide-plus" :loading="busy" :disabled="!newDeviceName.trim()" class="w-full rounded-2xl" @click="handleCreateDevice">
              创建设备
            </UButton>
          </div>

          <!-- step: fill config -->
          <template v-else-if="step === 'config' || step === 'writing'">
            <div class="rounded-2xl border border-neutral-200/70 p-3 text-xs text-muted dark:border-neutral-800/80">
              <span class="font-medium text-highlighted">{{ dbDevice?.name }}</span>
              <span class="ml-2">Bootstrap：{{ createdDevice?.bootstrapSerial }}</span>
            </div>

            <DeviceUsbConfigEditor
              v-model:backend-origin="backendOrigin"
              :wifi-profiles="wifiProfiles"
              title="首配参数"
              description="支持动态添加、删除和修改设备当前保存的 Wi-Fi 与后台地址。"
              :empty-message="wifiEmptyMessage"
              submit-label="写入配置并激活"
              submit-icon="i-lucide-zap"
              :submit-loading="step === 'writing' || busy"
              :submit-disabled="!canWriteActivationConfig"
              :show-backend-status="true"
              :validation-message="wifiValidationMessage"
              @add:wifi-profile="addWifiProfile"
              @remove:wifi-profile="removeWifiProfile"
              @submit="writeAndActivate(true)"
            />
          </template>

          <!-- step: activating -->
          <template v-else-if="step === 'activating'">
            <div class="flex items-center gap-3 text-sm text-toned">
              <UIcon name="i-lucide-loader-circle" class="size-4 animate-spin" />
              {{ statusMessage }}
            </div>
            <div v-if="summary?.lastErrorCode" class="rounded-xl border border-amber-200/80 bg-amber-50/60 px-3 py-2 text-xs text-amber-700 dark:border-amber-800/60 dark:bg-amber-950/40 dark:text-amber-300">
              设备最近错误：{{ summary.lastErrorCode }}
            </div>
          </template>

          <!-- step: done -->
          <div v-else-if="step === 'done'" class="flex items-center gap-2 rounded-2xl border border-green-200/70 bg-green-50/50 px-4 py-3 text-sm font-medium text-green-700 dark:border-green-800/50 dark:bg-green-950/30 dark:text-green-400">
            <UIcon name="i-lucide-circle-check" class="size-4" />
            {{ statusMessage }}
          </div>
        </template>

        <!-- ════ PROVISION (pending_activation) ════ -->
        <template v-else-if="view === 'provision'">
          <!-- Device card -->
          <div class="rounded-2xl border border-neutral-200/70 p-4 dark:border-neutral-800/80">
            <template v-if="dbDevice">
              <div class="text-sm font-semibold text-highlighted">{{ dbDevice.name }}</div>
              <div class="mt-1 text-xs text-muted">设备码：{{ dbDevice.deviceCode || "—" }}</div>
              <div class="text-xs text-muted">Bootstrap：{{ summary?.deviceSerial }}</div>
            </template>
            <template v-else>
              <div class="text-sm font-semibold text-highlighted">未知设备</div>
              <div class="mt-1 text-xs text-muted">Bootstrap：{{ summary?.deviceSerial }}</div>
              <p class="mt-2 text-xs text-amber-600 dark:text-amber-400">后台未找到对应记录。</p>
            </template>
          </div>

          <!-- Known device: issue activation -->
          <template v-if="dbDevice && (step === 'action' || step === 'config')">
            <UButton icon="i-lucide-key-round" :loading="polling" class="w-full rounded-2xl" @click="pollUntilActivated">
              开始激活
            </UButton>

            <!-- Optional reprovi sion -->
            <div class="overflow-hidden rounded-2xl border border-neutral-200/70 dark:border-neutral-800/80">
              <button class="flex w-full items-center justify-between px-4 py-3 text-sm font-medium text-highlighted" @click="showProvisionForm = !showProvisionForm">
                重新配网（可选）
                <UIcon :name="showProvisionForm ? 'i-lucide-chevron-up' : 'i-lucide-chevron-down'" class="size-4 text-muted" />
              </button>
              <div v-if="showProvisionForm" class="border-t border-neutral-200/70 p-4 dark:border-neutral-800/80">
                <DeviceUsbConfigEditor
                  v-model:backend-origin="backendOrigin"
                  :wifi-profiles="wifiProfiles"
                  title="当前待写入配置"
                  description="可直接调整设备上已保存的 Wi-Fi 列表，再重新激活。"
                  :empty-message="wifiEmptyMessage"
                  submit-label="写入配置并激活"
                  submit-icon="i-lucide-zap"
                  :submit-loading="busy"
                  :submit-disabled="!canWriteActivationConfig"
                  :show-backend-status="true"
                  :validation-message="wifiValidationMessage"
                  @add:wifi-profile="addWifiProfile"
                  @remove:wifi-profile="removeWifiProfile"
                  @submit="writeAndActivate(false)"
                />
              </div>
            </div>
          </template>

          <!-- Unknown device: create new -->
          <template v-else-if="!dbDevice && step === 'action'">
            <UFormField label="设备名称" required>
              <UInput v-model="newDeviceName" placeholder="例如 前台一号机" icon="i-lucide-monitor" class="w-full" />
            </UFormField>
            <UButton icon="i-lucide-plus" :loading="busy" :disabled="!newDeviceName.trim()" class="w-full rounded-2xl" @click="handleCreateDevice">
              创建设备并覆盖凭证
            </UButton>
          </template>

          <!-- Shared: writing / activating / done -->
          <template v-if="step === 'writing' || step === 'activating'">
            <div class="flex items-center gap-3 text-sm text-toned">
              <UIcon name="i-lucide-loader-circle" class="size-4 animate-spin" />
              {{ statusMessage }}
            </div>
            <div v-if="step === 'activating' && summary?.lastErrorCode" class="rounded-xl border border-amber-200/80 bg-amber-50/60 px-3 py-2 text-xs text-amber-700 dark:border-amber-800/60 dark:bg-amber-950/40 dark:text-amber-300">
              设备最近错误：{{ summary.lastErrorCode }}
            </div>
          </template>
          <div v-else-if="step === 'done'" class="flex items-center gap-2 rounded-2xl border border-green-200/70 bg-green-50/50 px-4 py-3 text-sm font-medium text-green-700 dark:border-green-800/50 dark:bg-green-950/30 dark:text-green-400">
            <UIcon name="i-lucide-circle-check" class="size-4" />
            {{ statusMessage }}
          </div>
        </template>

        <!-- ════ DEVICE INFO (activated) ════ -->
        <template v-else-if="view === 'device_info'">
          <div class="space-y-2 rounded-2xl border border-neutral-200/70 p-4 dark:border-neutral-800/80">
            <div class="text-sm font-semibold text-highlighted">{{ dbDevice?.name ?? "已激活设备" }}</div>
            <div class="text-xs text-muted">设备码：{{ summary?.deviceCode || dbDevice?.deviceCode || "—" }}</div>
            <div class="text-xs text-muted">后台地址：{{ summary?.backendOrigin || "—" }}</div>
            <div class="text-xs text-muted">Wi‑Fi 数量：{{ summary?.wifiProfileCount ?? "—" }}</div>
          </div>

          <div class="space-y-3">
            <div class="flex items-center justify-between">
              <p class="text-xs font-medium tracking-[0.14em] text-muted uppercase">当前配置</p>
              <UButton
                size="xs"
                variant="ghost"
                color="neutral"
                icon="i-lucide-refresh-cw"
                :loading="busy"
                @click="reloadCurrentConfig"
              >
                重新读取
              </UButton>
            </div>

            <UAlert
              v-if="wifiDetailsUnavailable"
              color="warning"
              icon="i-lucide-triangle-alert"
              title="当前固件未返回 Wi‑Fi 详情"
              :description="`设备回包显示已有 ${summary?.wifiProfileCount ?? 0} 项 Wi‑Fi 配置，但没有返回可编辑明细。现在可以新增并覆盖写入；如果要读取现有 SSID/密码，请升级到包含 wifiProfiles 返回字段的固件后重新读取。`"
            />

            <DeviceUsbConfigEditor
              v-model:backend-origin="backendOrigin"
              :wifi-profiles="wifiProfiles"
              title="设备配置表单"
              description="读取当前设备配置后可直接修改，支持动态添加和删除 Wi-Fi 项。"
              :empty-message="wifiEmptyMessage"
              submit-label="保存到设备"
              submit-icon="i-lucide-save"
              :submit-loading="busy"
              :submit-disabled="!canSaveCurrentConfig"
              :show-backend-status="true"
              :validation-message="wifiValidationMessage"
              @add:wifi-profile="addWifiProfile"
              @remove:wifi-profile="removeWifiProfile"
              @submit="saveCurrentConfig"
            />
          </div>

          <div class="space-y-3">
            <p class="text-xs font-medium tracking-[0.14em] text-muted uppercase">重置操作</p>

            <!-- Clear credentials -->
            <div class="rounded-2xl border border-neutral-200/70 p-3 dark:border-neutral-800/80">
              <div class="text-sm font-medium text-highlighted">清除运行时凭证</div>
              <p class="mt-1 text-xs text-muted">保留 WiFi 和后台地址，仅清除激活凭证，设备将重新激活。</p>
              <div class="mt-3">
                <template v-if="confirmingReset === 'credentials'">
                  <p class="mb-2 text-xs text-amber-600 dark:text-amber-400">确认执行？</p>
                  <div class="flex gap-2">
                    <UButton size="sm" color="warning" icon="i-lucide-check" :loading="busy" @click="executeReset('credentials')">确认清除</UButton>
                    <UButton size="sm" variant="ghost" color="neutral" :disabled="busy" @click="confirmingReset = null">取消</UButton>
                  </div>
                </template>
                <UButton v-else size="sm" color="warning" variant="outline" icon="i-lucide-key-round" :disabled="!deviceId" @click="confirmingReset = 'credentials'">
                  清除凭证
                </UButton>
              </div>
            </div>

            <!-- Full reset -->
            <div class="rounded-2xl border border-red-200/70 p-3 dark:border-red-900/50">
              <div class="text-sm font-medium text-red-700 dark:text-red-400">完全重置</div>
              <p class="mt-1 text-xs text-muted">清除全部配置，设备恢复出厂状态。</p>
              <div class="mt-3">
                <template v-if="confirmingReset === 'full'">
                  <p class="mb-2 text-xs text-red-600 dark:text-red-400">此操作不可撤销，确认完全重置？</p>
                  <div class="flex gap-2">
                    <UButton size="sm" color="error" icon="i-lucide-check" :loading="busy" @click="executeReset('full')">确认重置</UButton>
                    <UButton size="sm" variant="ghost" color="neutral" :disabled="busy" @click="confirmingReset = null">取消</UButton>
                  </div>
                </template>
                <UButton v-else size="sm" color="error" variant="outline" icon="i-lucide-rotate-ccw" :disabled="!deviceId" @click="confirmingReset = 'full'">
                  完全重置
                </UButton>
              </div>
            </div>
          </div>
        </template>

        <!-- ════ Serial log panel ════ -->
        <div v-if="view !== 'idle' && view !== 'connecting'" class="overflow-hidden rounded-2xl border border-neutral-200/70 dark:border-neutral-800/80">
          <button class="flex w-full items-center justify-between px-4 py-2.5 text-xs font-medium text-highlighted" @click="showSerialLogs = !showSerialLogs">
            <span class="flex items-center gap-1.5">
              <UIcon name="i-lucide-terminal" class="size-3.5 text-muted" />
              串口日志
            </span>
            <div class="flex items-center gap-2">
              <UButton size="xs" variant="ghost" color="neutral" icon="i-lucide-trash-2" @click.stop="serial.clearLogs()">清空</UButton>
              <UIcon :name="showSerialLogs ? 'i-lucide-chevron-up' : 'i-lucide-chevron-down'" class="size-3.5 text-muted" />
            </div>
          </button>
          <div v-if="showSerialLogs" class="h-56 border-t border-neutral-200/70 dark:border-neutral-800/80">
            <SerialLogTerminal :logs="serial.serialLogs.value" />
          </div>
        </div>

        <!-- ════ Serial footer + close ════ -->
        <div v-if="view !== 'idle' && view !== 'connecting'" class="flex items-center justify-between border-t border-neutral-200/70 pt-3 dark:border-neutral-800/80">
          <span class="text-xs text-muted">串口：{{ serial.connected.value ? "已连接" : "已断开" }}</span>
          <UButton size="xs" variant="ghost" color="neutral" icon="i-lucide-unplug" :disabled="!serial.connected.value" @click="serial.disconnect">断开</UButton>
        </div>

        <p v-if="statusMessage && !errorMessage && view !== 'connecting'" class="text-xs text-toned">{{ statusMessage }}</p>

        <UButton variant="ghost" color="neutral" class="w-full" @click="emit('update:open', false)">关闭</UButton>
      </div>
    </template>
  </USlideover>
</template>
