<script setup lang="ts">
import { useMutation, useQueryClient } from "@tanstack/vue-query";
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue";

import {
  type DeviceSerialSummary,
  type DeviceSerialWifiProfile,
  formatDeviceSerialConnectError,
  useDeviceSerial,
} from "~/composables/useDeviceSerial";

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

const runtimeConfig = useRuntimeConfig();
const configuredServerUrl =
  typeof runtimeConfig.public.serverUrl === "string" ? runtimeConfig.public.serverUrl.trim() : "";
const chromeFlagsUrl = "chrome://flags/#unsafely-treat-insecure-origin-as-secure";

const deviceId = computed(() => createdDevice.value?.id ?? dbDevice.value?.id ?? null);
const currentOrigin = computed(() => (import.meta.client ? window.location.origin : ""));
const preferredBackendOrigin = computed(() => configuredServerUrl || currentOrigin.value);
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
const hasLoadedDevice = computed(() => Boolean(summary.value || dbDevice.value || createdDevice.value));
const workflowTitle = computed(() => {
  if (view.value === "connecting") {
    return "正在连接设备";
  }

  if (view.value === "new_device") {
    return step.value === "create" ? "新设备首配" : "填写首配参数";
  }

  if (view.value === "provision") {
    return "待激活设备";
  }

  if (view.value === "device_info") {
    return "设备配置中心";
  }

  return "开始串口连接";
});
const workflowDescription = computed(() => {
  if (view.value === "connecting") {
    return statusMessage.value || "正在读取设备状态与配置。";
  }

  if (view.value === "new_device") {
    return "首次接入设备时，在这里创建后台记录并写入网络与后端配置。";
  }

  if (view.value === "provision") {
    return "设备已具备 bootstrap 身份，可继续激活或重新下发配置。";
  }

  if (view.value === "device_info") {
    return "已连接设备的当前配置、日志和重置操作都集中在这里。";
  }

  return "从这里发起串口选择，连接成功后会自动读取设备状态。";
});
const connectionTone = computed(() => {
  if (view.value === "connecting") {
    return "primary" as const;
  }

  return serial.connected.value ? ("neutral" as const) : ("neutral" as const);
});
const connectionLabel = computed(() => {
  if (view.value === "connecting") {
    return "读取中";
  }

  return serial.connected.value ? "串口已连接" : "等待连接";
});
const currentDeviceName = computed(() => {
  if (dbDevice.value?.name) {
    return dbDevice.value.name;
  }

  if (summary.value?.deviceCode) {
    return summary.value.deviceCode;
  }

  return "尚未识别设备";
});
const summaryItems = computed(() => [
  {
    label: "设备码",
    value: summary.value?.deviceCode || dbDevice.value?.deviceCode || "—",
  },
  {
    label: "后台地址",
    value: summary.value?.backendOrigin || backendOrigin.value || "—",
  },
  {
    label: "Wi‑Fi",
    value: `${summary.value?.wifiProfileCount ?? wifiProfiles.value.length} 项`,
  },
  {
    label: "状态",
    value:
      summary.value?.activationState === "activated"
        ? "已激活"
        : summary.value?.activationState === "pending_activation"
          ? "待激活"
          : "未配置",
  },
]);
const showReloadAction = computed(
  () => serial.connected.value && view.value !== "idle" && view.value !== "connecting",
);
const backendOriginDescription = computed(() => {
  if (preferredBackendOrigin.value) {
    return `默认优先填充当前 Web 应用地址 ${preferredBackendOrigin.value}，可按需修改。`;
  }

  return "默认会优先使用当前 Web 应用地址，可按需修改。";
});
const logCount = computed(() => serial.serialLogs.value.length);
const lastLogPreview = computed(() => serial.lastRawLine.value || "等待新的串口输出");
const serialSupportTitle = computed(() => {
  if (serial.supportInfo.value.reason === "insecure_context") {
    return "当前页面不是安全上下文";
  }

  if (serial.supportInfo.value.reason === "unsupported_browser") {
    return "当前浏览器不支持 Web Serial";
  }

  return "当前页面暂时无法访问 Web Serial";
});
const serialSupportDescription = computed(() => {
  if (serial.supportInfo.value.reason === "insecure_context") {
    return "当前页面不是 HTTPS 或 localhost。先把这个地址加入 Chrome 安全列表，再回来连接设备。";
  }

  if (serial.supportInfo.value.reason === "unsupported_browser") {
    return "请改用桌面版 Chrome、Edge 或其他 Chromium 浏览器打开后台。";
  }

  return "请确认你使用的是桌面版 Chromium 浏览器，并通过 HTTPS 或 localhost 打开后台。";
});
const serialSupportTip = computed(() => {
  if (serial.supportInfo.value.reason === "insecure_context") {
    return "也可以直接用 localhost 打开后台，这样不需要额外配置。";
  }

  return "如果已经在 Chromium 浏览器中打开但仍不可用，请优先检查当前地址是否为 HTTPS 或 localhost。";
});
const serialWhitelistOrigin = computed(() => currentOrigin.value || "http://你的局域网IP:3001");

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
  backendOrigin.value = preferredBackendOrigin.value;
  backendStatus.value = "unknown";
  wifiProfiles.value = [createWifiProfileForm({}, 0)];
  if (import.meta.client && backendOrigin.value) {
    checkBackendOrigin();
  }
}

async function detectBackendOriginDefault() {
  if (!import.meta.client || !configuredServerUrl || summary.value?.backendOrigin?.trim()) {
    return;
  }

  const current = backendOrigin.value.trim();
  if (!current || current === currentOrigin.value) {
    backendOrigin.value = configuredServerUrl;
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

async function loadConnectedDevice() {
  errorMessage.value = "";
  view.value = "connecting";
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
}

async function connect() {
  errorMessage.value = "";
  view.value = "connecting";

  try {
    await serial.requestPortConnection();
    await loadConnectedDevice();
  } catch (error: any) {
    errorMessage.value = formatDeviceSerialConnectError(error);
    view.value = "idle";
  }
}

async function disconnectDevice() {
  polling.value = false;
  await serial.disconnect();
  resetState();
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

onMounted(async () => {
  try {
    await detectBackendOriginDefault();

    if (!serial.connected.value) {
      const restoreResult = await serial.restoreAuthorizedPort();

      if (restoreResult.status === "none") {
        resetState();
        return;
      }

      if (restoreResult.status === "multiple") {
        resetState();
        errorMessage.value = "检测到多个已授权串口，请手动选择要连接的设备。";
        return;
      }
    }

    await loadConnectedDevice();
  } catch (error: any) {
    if (serial.connected.value) {
      await serial.disconnect();
    }
    resetState();
    errorMessage.value = formatDeviceSerialConnectError(error, "读取设备配置失败。");
    view.value = "idle";
  }
});

onBeforeUnmount(() => {
  polling.value = false;
  if (backendCheckTimer) {
    clearTimeout(backendCheckTimer);
  }
});
</script>

<template>
  <div class="space-y-5">
    <section class="workspace-surface overflow-hidden">
      <div class="workspace-surface-header">
        <div class="space-y-3">
          <div class="flex flex-wrap items-center gap-2">
            <UBadge
              :label="connectionLabel"
              :color="connectionTone"
              variant="outline"
              class="workspace-status-chip"
            />
            <UBadge
              v-if="summary?.activationState"
              :label="
                summary.activationState === 'activated'
                  ? '已激活'
                  : summary.activationState === 'pending_activation'
                    ? '待激活'
                    : '未配置'
              "
              color="neutral"
              variant="outline"
              class="workspace-status-chip"
            />
          </div>

          <div>
            <h2 class="text-xl font-semibold tracking-tight text-highlighted">
              {{ workflowTitle }}
            </h2>
            <p class="mt-1 text-sm text-toned">
              {{ workflowDescription }}
            </p>
          </div>

          <div class="text-sm text-toned">
            当前设备：
            <span class="font-medium text-highlighted">{{ currentDeviceName }}</span>
          </div>
        </div>

        <div class="flex flex-wrap gap-2">
          <UButton
            icon="i-lucide-usb"
            :loading="view === 'connecting'"
            :disabled="!serial.supported.value"
            class="workspace-primary-action"
            @click="connect"
          >
            {{ serial.connected.value ? "重新选择串口" : "连接设备" }}
          </UButton>
          <UButton
            v-if="showReloadAction"
            variant="outline"
            color="neutral"
            icon="i-lucide-refresh-cw"
            :loading="busy"
            class="workspace-secondary-action"
            @click="reloadCurrentConfig"
          >
            重新读取
          </UButton>
          <UButton
            variant="outline"
            color="neutral"
            icon="i-lucide-unplug"
            :disabled="!serial.connected.value"
            class="workspace-secondary-action"
            @click="disconnectDevice"
          >
            断开串口
          </UButton>
        </div>
      </div>

      <div v-if="hasLoadedDevice" class="workspace-surface-body border-t border-neutral-200/70 py-4 dark:border-neutral-800/80">
        <div class="grid gap-3 sm:grid-cols-2 xl:grid-cols-4">
          <div v-for="item in summaryItems" :key="item.label" class="min-w-0">
            <div class="text-[11px] font-medium tracking-[0.16em] text-muted uppercase">
              {{ item.label }}
            </div>
            <div class="mt-1 break-all text-sm font-medium text-highlighted">
              {{ item.value }}
            </div>
          </div>
        </div>
      </div>
    </section>

    <div class="grid gap-5 xl:grid-cols-[minmax(0,1.72fr)_minmax(360px,0.9fr)] 2xl:grid-cols-[minmax(0,1.85fr)_minmax(360px,0.82fr)]">
      <section class="workspace-surface overflow-hidden xl:order-1">
        <div class="workspace-surface-header border-b-0 pb-3">
          <div class="space-y-2">
            <div class="flex flex-wrap items-center gap-2">
              <UBadge
                label="实时串口输出"
                color="neutral"
                variant="outline"
                class="workspace-status-chip"
              />
              <UBadge
                :label="`${logCount} 行`"
                :color="logCount ? 'primary' : 'neutral'"
                variant="outline"
                class="workspace-status-chip"
              />
              <span
                class="text-[11px] font-medium tracking-[0.16em] text-neutral-500 uppercase dark:text-neutral-400"
              >
                hardware link
              </span>
            </div>
            <div>
              <h3 class="text-base font-semibold tracking-tight text-highlighted">
                终端主视区
              </h3>
              <p class="text-sm text-toned">
                持续观察设备回包、运行状态和配置写入反馈。
              </p>
            </div>
          </div>

          <div class="flex flex-wrap gap-2">
            <UButton
              size="xs"
              variant="outline"
              color="neutral"
              icon="i-lucide-trash-2"
              class="workspace-secondary-action"
              @click="serial.clearLogs()"
            >
              清空
            </UButton>
            <UButton
              size="xs"
              variant="outline"
              color="neutral"
              :icon="showSerialLogs ? 'i-lucide-minimize-2' : 'i-lucide-maximize-2'"
              class="workspace-secondary-action"
              @click="showSerialLogs = !showSerialLogs"
            >
              {{ showSerialLogs ? "折叠日志" : "展开日志" }}
            </UButton>
          </div>
        </div>

        <div class="px-5 pt-5 pb-5">
          <div class="overflow-hidden rounded-[24px] border border-neutral-900/90 bg-neutral-950 shadow-[inset_0_1px_0_rgba(255,255,255,0.06)]">
            <div class="flex items-center justify-between border-b border-white/8 px-4 py-3">
              <div class="flex items-center gap-2">
                <span
                  class="size-2 rounded-full bg-amber-500"
                  :class="serial.connected.value ? 'animate-pulse' : 'bg-neutral-500'"
                />
                <span class="text-[11px] font-medium tracking-[0.16em] text-neutral-400 uppercase">
                  serial session
                </span>
              </div>
              <div class="font-mono text-[11px] text-neutral-400 uppercase">
                {{ connectionLabel }}
              </div>
            </div>

            <div
              v-if="showSerialLogs"
              class="h-[360px] border-t-0 xl:h-[440px] 2xl:h-[520px]"
            >
              <SerialLogTerminal :logs="serial.serialLogs.value" />
            </div>
            <div
              v-else
              class="grid h-[220px] place-items-center text-sm text-neutral-400"
            >
              日志已折叠，仍会持续接收新的串口输出。
            </div>
          </div>

          <div class="mt-4 grid gap-4 border-t border-neutral-200/70 pt-4 text-sm dark:border-neutral-800/80 lg:grid-cols-[minmax(0,1fr)_220px]">
            <div class="min-w-0">
              <div class="text-[11px] font-medium tracking-[0.16em] text-muted uppercase">
                最近一条输出
              </div>
              <div class="mt-2 break-all font-mono text-xs leading-6 text-toned">
                {{ lastLogPreview }}
              </div>
            </div>

            <div class="min-w-0">
              <div class="text-[11px] font-medium tracking-[0.16em] text-muted uppercase">
                操作提示
              </div>
              <div class="mt-2 space-y-1.5 text-sm text-toned">
                <p>重新选择串口可切换另一台设备。</p>
                <p>写入配置后日志会直接展示设备回包。</p>
              </div>
            </div>
          </div>
        </div>
      </section>

      <aside class="workspace-surface overflow-hidden xl:order-2 xl:sticky xl:top-4 xl:self-start">
        <div class="workspace-surface-header py-3">
          <div class="flex min-w-0 items-center justify-between gap-3">
            <div class="min-w-0">
              <div class="text-[11px] font-medium tracking-[0.16em] text-muted uppercase">
                control inspector
              </div>
              <div class="mt-1 truncate text-sm font-semibold text-highlighted">
                {{ workflowTitle }}
              </div>
            </div>
            <div class="text-right text-xs text-toned">
              {{ workflowDescription }}
            </div>
          </div>
        </div>

        <div class="workspace-surface-body space-y-4">
          <div
            v-if="!serial.supported.value"
            class="rounded-2xl border border-amber-200/80 bg-amber-50/80 p-4 text-sm dark:border-amber-800/60 dark:bg-amber-950/40"
          >
            <div class="flex gap-3">
              <UIcon name="i-lucide-monitor-warning" class="mt-0.5 size-5 shrink-0 text-amber-600 dark:text-amber-400" />
              <div class="space-y-3">
                <div>
                  <div class="font-semibold text-amber-900 dark:text-amber-200">
                    {{ serialSupportTitle }}
                  </div>
                  <div class="mt-1 text-amber-800 dark:text-amber-300">
                    {{ serialSupportDescription }}
                  </div>
                </div>
                <div
                  v-if="serial.supportInfo.value.reason === 'insecure_context'"
                  class="space-y-3 rounded-2xl border border-amber-200/80 bg-white/70 p-3 dark:border-amber-800/60 dark:bg-black/10"
                >
                  <div class="space-y-1">
                    <div class="font-medium text-amber-900 dark:text-amber-200">当前需要加入安全列表的地址</div>
                    <div class="rounded-xl bg-amber-100/80 px-3 py-2 font-mono text-xs break-all text-amber-950 dark:bg-amber-900/50 dark:text-amber-100">
                      {{ serialWhitelistOrigin }}
                    </div>
                    <div class="text-xs text-amber-700 dark:text-amber-400">
                      请手动复制这个地址。
                    </div>
                  </div>

                  <div class="space-y-1">
                    <div class="font-medium text-amber-900 dark:text-amber-200">操作步骤</div>
                    <ol class="list-inside list-decimal space-y-1 text-amber-800 dark:text-amber-300">
                      <li>新开 Chrome 标签页，手动打开 <code class="rounded bg-amber-100 px-1 font-mono text-xs dark:bg-amber-900/60">{{ chromeFlagsUrl }}</code></li>
                      <li>把 <code class="rounded bg-amber-100 px-1 font-mono text-xs dark:bg-amber-900/60">{{ serialWhitelistOrigin }}</code> 粘贴到 <code class="rounded bg-amber-100 px-1 font-mono text-xs dark:bg-amber-900/60">Insecure origins treated as secure</code> 输入框</li>
                      <li>设为 <strong>Enabled</strong>，点击 <strong>Relaunch</strong>，然后回到本页重试</li>
                    </ol>
                  </div>
                </div>
                <div class="text-amber-700 dark:text-amber-400">
                  {{ serialSupportTip }}
                </div>
              </div>
            </div>
          </div>

          <UAlert
            v-if="errorMessage"
            color="error"
            icon="i-lucide-alert-circle"
            title="操作失败"
            :description="errorMessage"
          />

          <template v-if="view === 'idle'">
            <div class="space-y-4">
              <div class="text-sm text-toned">
                从这里发起串口选择，连接成功后会自动读取设备状态与当前配置。
              </div>
              <UButton
                icon="i-lucide-usb"
                :disabled="!serial.supported.value"
                class="workspace-primary-action w-full"
                @click="connect"
              >
                连接设备
              </UButton>
            </div>
          </template>

          <div v-else-if="view === 'connecting'" class="flex items-center gap-3 text-sm text-toned">
            <UIcon name="i-lucide-loader-circle" class="size-4 animate-spin" />
            {{ statusMessage || "正在连接…" }}
          </div>

          <template v-else-if="view === 'new_device'">
            <div class="flex items-center gap-2 text-sm font-medium text-primary">
              <UIcon name="i-lucide-sparkles" class="size-4 text-primary" />
              <span>全新设备 — 需要配置</span>
            </div>

            <div v-if="step === 'create'" class="space-y-4">
              <UFormField label="设备名称" required>
                <UInput v-model="newDeviceName" placeholder="例如 前台一号机" icon="i-lucide-monitor" class="w-full" @keydown.enter="handleCreateDevice" />
              </UFormField>
              <UButton icon="i-lucide-plus" :loading="busy" :disabled="!newDeviceName.trim()" class="workspace-primary-action w-full" @click="handleCreateDevice">
                创建设备
              </UButton>
            </div>

            <template v-else-if="step === 'config' || step === 'writing'">
              <div class="text-xs text-muted">
                <span class="font-medium text-highlighted">{{ dbDevice?.name }}</span>
                <span class="ml-2">Bootstrap：{{ createdDevice?.bootstrapSerial }}</span>
              </div>

              <DeviceUsbConfigEditor
                embedded
                v-model:backend-origin="backendOrigin"
                :wifi-profiles="wifiProfiles"
                title="首配参数"
                :description="`支持动态添加、删除和修改设备当前保存的 Wi-Fi 与后台地址。${backendOriginDescription}`"
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

            <template v-else-if="step === 'activating'">
              <div class="flex items-center gap-3 text-sm text-toned">
                <UIcon name="i-lucide-loader-circle" class="size-4 animate-spin" />
                {{ statusMessage }}
              </div>
              <div v-if="summary?.lastErrorCode" class="rounded-xl border border-amber-200/80 bg-amber-50/60 px-3 py-2 text-xs text-amber-700 dark:border-amber-800/60 dark:bg-amber-950/40 dark:text-amber-300">
                设备最近错误：{{ summary.lastErrorCode }}
              </div>
            </template>

            <div v-else-if="step === 'done'" class="flex items-center gap-2 rounded-2xl border border-neutral-300/80 bg-[var(--workspace-panel-muted)] px-4 py-3 text-sm font-medium text-highlighted dark:border-neutral-700/80">
              <UIcon name="i-lucide-circle-check" class="size-4" />
              {{ statusMessage }}
            </div>
          </template>

          <template v-else-if="view === 'provision'">
            <div class="border-b border-neutral-200/70 pb-3 text-sm dark:border-neutral-800/80">
              <template v-if="dbDevice">
                <div class="font-semibold text-highlighted">{{ dbDevice.name }}</div>
                <div class="mt-1 text-xs text-muted">设备码：{{ dbDevice.deviceCode || "—" }}</div>
                <div class="text-xs text-muted">Bootstrap：{{ summary?.deviceSerial }}</div>
              </template>
              <template v-else>
                <div class="font-semibold text-highlighted">未知设备</div>
                <div class="mt-1 text-xs text-muted">Bootstrap：{{ summary?.deviceSerial }}</div>
                <p class="mt-2 text-xs text-amber-600 dark:text-amber-400">后台未找到对应记录。</p>
              </template>
            </div>

            <template v-if="dbDevice && (step === 'action' || step === 'config')">
              <UButton icon="i-lucide-key-round" :loading="polling" class="workspace-primary-action w-full" @click="pollUntilActivated">
                开始激活
              </UButton>

              <div class="border-t border-neutral-200/70 pt-2 dark:border-neutral-800/80">
                <button class="flex w-full items-center justify-between py-2 text-sm font-medium text-highlighted" @click="showProvisionForm = !showProvisionForm">
                  重新配网（可选）
                  <UIcon :name="showProvisionForm ? 'i-lucide-chevron-up' : 'i-lucide-chevron-down'" class="size-4 text-muted" />
                </button>
                <div v-if="showProvisionForm" class="border-t border-neutral-200/70 pt-4 dark:border-neutral-800/80">
                  <DeviceUsbConfigEditor
                    embedded
                    v-model:backend-origin="backendOrigin"
                    :wifi-profiles="wifiProfiles"
                    title="当前待写入配置"
                    :description="`可直接调整设备上已保存的 Wi-Fi 列表，再重新激活。${backendOriginDescription}`"
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

            <template v-else-if="!dbDevice && step === 'action'">
              <UFormField label="设备名称" required>
                <UInput v-model="newDeviceName" placeholder="例如 前台一号机" icon="i-lucide-monitor" class="w-full" />
              </UFormField>
              <UButton icon="i-lucide-plus" :loading="busy" :disabled="!newDeviceName.trim()" class="workspace-primary-action w-full" @click="handleCreateDevice">
                创建设备并覆盖凭证
              </UButton>
            </template>

            <template v-if="step === 'writing' || step === 'activating'">
              <div class="flex items-center gap-3 text-sm text-toned">
                <UIcon name="i-lucide-loader-circle" class="size-4 animate-spin" />
                {{ statusMessage }}
              </div>
              <div v-if="step === 'activating' && summary?.lastErrorCode" class="rounded-xl border border-amber-200/80 bg-amber-50/60 px-3 py-2 text-xs text-amber-700 dark:border-amber-800/60 dark:bg-amber-950/40 dark:text-amber-300">
                设备最近错误：{{ summary.lastErrorCode }}
              </div>
            </template>
            <div v-else-if="step === 'done'" class="flex items-center gap-2 rounded-2xl border border-neutral-300/80 bg-[var(--workspace-panel-muted)] px-4 py-3 text-sm font-medium text-highlighted dark:border-neutral-700/80">
              <UIcon name="i-lucide-circle-check" class="size-4" />
              {{ statusMessage }}
            </div>
          </template>

          <template v-else-if="view === 'device_info'">
            <UAlert
              v-if="wifiDetailsUnavailable"
              color="warning"
              icon="i-lucide-triangle-alert"
              title="当前固件未返回 Wi‑Fi 详情"
              :description="`设备回包显示已有 ${summary?.wifiProfileCount ?? 0} 项 Wi‑Fi 配置，但没有返回可编辑明细。现在可以新增并覆盖写入；如果要读取现有 SSID/密码，请升级到包含 wifiProfiles 返回字段的固件后重新读取。`"
            />

            <DeviceUsbConfigEditor
              embedded
              v-model:backend-origin="backendOrigin"
              :wifi-profiles="wifiProfiles"
              title="设备配置表单"
              :description="`读取当前设备配置后可直接修改，支持动态添加和删除 Wi-Fi 项。${backendOriginDescription}`"
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

            <div class="border-t border-neutral-200/70 pt-5 dark:border-neutral-800/80">
              <p class="text-xs font-medium tracking-[0.14em] text-muted uppercase">重置操作</p>

              <div class="mt-3 space-y-3">
                <div class="border-b border-neutral-200/70 pb-3 dark:border-neutral-800/80">
                  <div class="text-sm font-medium text-highlighted">清除运行时凭证</div>
                  <p class="mt-1 text-xs text-muted">保留 WiFi 和后台地址，仅清除激活凭证，设备将重新激活。</p>
                  <div class="mt-3">
                    <template v-if="confirmingReset === 'credentials'">
                      <p class="mb-2 text-xs text-amber-600 dark:text-amber-400">确认执行？</p>
                      <div class="flex gap-2">
                        <UButton size="sm" color="warning" icon="i-lucide-check" :loading="busy" @click="executeReset('credentials')">确认清除</UButton>
                        <UButton size="sm" variant="outline" color="neutral" class="workspace-secondary-action" :disabled="busy" @click="confirmingReset = null">取消</UButton>
                      </div>
                    </template>
                    <UButton v-else size="sm" color="warning" variant="outline" icon="i-lucide-key-round" :disabled="!deviceId" @click="confirmingReset = 'credentials'">
                      清除凭证
                    </UButton>
                  </div>
                </div>

                <div class="pt-1">
                  <div class="text-sm font-medium text-red-700 dark:text-red-400">完全重置</div>
                  <p class="mt-1 text-xs text-muted">清除全部配置，设备恢复出厂状态。</p>
                  <div class="mt-3">
                    <template v-if="confirmingReset === 'full'">
                      <p class="mb-2 text-xs text-red-600 dark:text-red-400">此操作不可撤销，确认完全重置？</p>
                      <div class="flex gap-2">
                        <UButton size="sm" color="error" icon="i-lucide-check" :loading="busy" @click="executeReset('full')">确认重置</UButton>
                        <UButton size="sm" variant="outline" color="neutral" class="workspace-secondary-action" :disabled="busy" @click="confirmingReset = null">取消</UButton>
                      </div>
                    </template>
                    <UButton v-else size="sm" color="error" variant="outline" icon="i-lucide-rotate-ccw" :disabled="!deviceId" @click="confirmingReset = 'full'">
                      完全重置
                    </UButton>
                  </div>
                </div>
              </div>
            </div>
          </template>

          <div class="border-t border-neutral-200/70 pt-4 text-xs text-toned dark:border-neutral-800/80">
            控制面板负责配置与设备状态，左侧终端持续提供实时反馈。
          </div>
        </div>
      </aside>

      <p v-if="statusMessage && !errorMessage && view !== 'connecting'" class="text-xs text-toned xl:col-span-2">
        {{ statusMessage }}
      </p>
    </div>
  </div>
</template>
