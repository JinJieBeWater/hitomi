<script setup lang="ts">
import { useMutation, useQueryClient } from "@tanstack/vue-query";
import { computed, onBeforeUnmount, ref, watch } from "vue";

import { type DeviceSerialSummary, useDeviceSerial } from "~/composables/useDeviceSerial";

const props = defineProps<{ open: boolean }>();
const emit = defineEmits<{ "update:open": [boolean] }>();

const { $orpc } = useNuxtApp();
const toast = useToast();
const serial = useDeviceSerial();
const queryClient = useQueryClient();

const createDevice = useMutation($orpc.device.create.mutationOptions());

// ─── Types ────────────────────────────────────────────────────────────────────

type View = "idle" | "connecting" | "new_device" | "provision" | "device_info";
// new_device steps: create → config → writing → activating → done
// provision steps:  action → (writing → activating → done)
type Step = "create" | "config" | "writing" | "activating" | "done" | "action";

// ─── State ────────────────────────────────────────────────────────────────────

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
const createdDevice = ref<{ id: string; bootstrapSerial: string; bootstrapSecret: string } | null>(null);

const newDeviceName = ref("");
const backendOrigin = ref("");
const backendStatus = ref<"unknown" | "checking" | "ok" | "error">("unknown");
const wifiProfiles = ref([{ id: mkId(), ssid: "", password: "", priority: 100 }]);

// ─── Computed ─────────────────────────────────────────────────────────────────

const deviceId = computed(() => createdDevice.value?.id ?? dbDevice.value?.id ?? null);
const currentOrigin = computed(() => (import.meta.client ? window.location.origin : ""));

const canWriteConfig = computed(
  () =>
    serial.connected.value &&
    wifiProfiles.value.some((p) => p.ssid.trim() && p.password.trim()) &&
    backendOrigin.value.trim(),
);

// ─── Helpers ──────────────────────────────────────────────────────────────────
function mkId() {
  return globalThis.crypto?.randomUUID?.() ?? `id_${Date.now()}_${Math.random().toString(36).slice(2)}`;
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
  wifiProfiles.value = [{ id: mkId(), ssid: "", password: "", priority: 100 }];
  if (import.meta.client && backendOrigin.value) {
    checkBackendOrigin();
  }
}

function addWifiProfile() {
  wifiProfiles.value.push({
    id: mkId(),
    ssid: "",
    password: "",
    priority: Math.max(10, 100 - wifiProfiles.value.length * 10),
  });
}

function removeWifiProfile(id: string) {
  if (wifiProfiles.value.length === 1) return;
  wifiProfiles.value = wifiProfiles.value.filter((p) => p.id !== id);
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
    const res = await fetch(`${url}/rpc/healthCheck`, {
      method: "POST",
      signal: AbortSignal.timeout(4000),
    });
    backendStatus.value = res.ok ? "ok" : "error";
  } catch {
    backendStatus.value = "error";
  }
}

watch(backendOrigin, () => {
  backendStatus.value = "unknown";
  if (backendCheckTimer) clearTimeout(backendCheckTimer);
  backendCheckTimer = setTimeout(checkBackendOrigin, 600);
});

async function refreshSummary() {
  const res = await serial.sendCommand({ type: "get_config" });
  summary.value = res.summary ?? null;
  return res.summary ?? null;
}

// ─── Core Actions ─────────────────────────────────────────────────────────────

async function connect() {
  errorMessage.value = "";
  view.value = "connecting";
  try {
    await serial.connect();
    statusMessage.value = "正在读取设备信息…";
    const s = await refreshSummary();

    if (!s) {
      throw new Error("未能读取设备配置摘要。");
    }

    if (s.activationState === "unconfigured") {
      // Device has no bootstrapSecret — needs full provisioning
      view.value = "new_device";
      step.value = "create";
      statusMessage.value = "全新设备，请先创建设备记录。";
      return;
    }

    // Device has a valid bootstrap identity — look up in DB
    const found = await queryClient.fetchQuery(
      $orpc.device.findByBootstrapSerial.queryOptions({ input: { serial: s.deviceSerial } }),
    );
    dbDevice.value = found ? { id: found.id, name: found.name, deviceCode: found.deviceCode } : null;

    if (s.activationState === "activated") {
      view.value = "device_info";
      statusMessage.value = found ? `已识别：${found.name}` : "已激活设备（后台未找到记录）";
    } else {
      view.value = "provision";
      step.value = "action";
      statusMessage.value = found
        ? `已识别：${found.name}，待激活`
        : "设备已有 Bootstrap 配置但后台未找到记录，可创建新设备。";
    }
  } catch (err: any) {
    errorMessage.value = err?.message || "连接失败。";
    view.value = "idle";
  }
}

async function handleCreateDevice() {
  if (!newDeviceName.value.trim()) return;
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
    // Move to config step in whichever view we're in
    step.value = "config";
    statusMessage.value = `设备「${result.device.name}」已创建，请填写网络配置。`;
  } catch (err: any) {
    errorMessage.value = err?.message || "创建设备失败。";
  } finally {
    busy.value = false;
  }
}

// writeBootstrap=true for new devices (need to write set_bootstrap_identity)
// writeBootstrap=false for provision flow (device already has bootstrap identity)
async function writeAndActivate(writeBootstrap: boolean) {
  if (!deviceId.value) return;
  busy.value = true;
  errorMessage.value = "";
  step.value = "writing";
  statusMessage.value = "正在写入配置…";
  try {
    const profiles = wifiProfiles.value
      .filter((p) => p.ssid.trim() && p.password.trim())
      .map((p) => ({ ssid: p.ssid.trim(), password: p.password.trim(), priority: p.priority }));

    await serial.sendCommand({ type: "set_wifi_profiles", profiles });
    await serial.sendCommand({ type: "set_backend_origin", origin: backendOrigin.value.trim() });
    if (writeBootstrap && createdDevice.value) {
      await serial.sendCommand({
        type: "set_bootstrap_identity",
        deviceSerial: createdDevice.value.bootstrapSerial,
        bootstrapSecret: createdDevice.value.bootstrapSecret,
      });
    }
    await refreshSummary();
    await pollUntilActivated();
  } catch (err: any) {
    errorMessage.value = err?.message || "写入配置失败。";
    step.value = "config";
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
      if (!polling.value) return;
      const s = await refreshSummary();
      if (s?.activationState === "activated") {
        step.value = "done";
        statusMessage.value = "设备激活成功！";
        toast.add({ title: "设备已激活" });
        await queryClient.invalidateQueries({
          queryKey: $orpc.device.list.queryOptions({ input: {} }).queryKey,
        });
        return;
      }
      await new Promise((r) => setTimeout(r, 2000));
    }
    errorMessage.value = "等待超时，设备未在预期时间内完成激活。";
    step.value = "action";
  } catch (err: any) {
    errorMessage.value = err?.message || "激活失败。";
    step.value = "action";
  } finally {
    polling.value = false;
  }
}

async function executeReset(type: "credentials" | "full") {
  if (!deviceId.value) return;
  busy.value = true;
  errorMessage.value = "";
  confirmingReset.value = null;
  try {
    if (type === "credentials") {
      await serial.sendCommand({ type: "clear_runtime_credentials" });
    } else {
      await serial.sendCommand({ type: "reset_device_config" });
    }
    await refreshSummary();
    statusMessage.value = type === "credentials"
      ? "凭证已清除，设备将重新激活。"
      : "设备已完全重置，进入待激活状态。";
    toast.add({ title: type === "credentials" ? "凭证已清除" : "设备已完全重置" });
    view.value = "provision";
    step.value = "action";
    await pollUntilActivated();
  } catch (err: any) {
    errorMessage.value = err?.message || "重置失败。";
  } finally {
    busy.value = false;
  }
}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

watch(
  () => props.open,
  async (open) => {
    if (!open) {
      polling.value = false;
      await serial.disconnect();
      resetState();
    } else {
      resetState();
    }
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

            <!-- WiFi profiles -->
            <div class="space-y-3">
              <div class="flex items-center justify-between">
                <span class="text-sm font-medium text-highlighted">Wi‑Fi 网络</span>
                <UButton size="xs" variant="outline" icon="i-lucide-plus" @click="addWifiProfile">添加</UButton>
              </div>
              <div v-for="profile in wifiProfiles" :key="profile.id" class="rounded-2xl border border-neutral-200/70 p-3 dark:border-neutral-800/80">
                <div class="grid gap-3 sm:grid-cols-[1fr_1fr_80px_auto]">
                  <UFormField label="SSID">
                    <UInput v-model="profile.ssid" placeholder="网络名称" class="w-full" />
                  </UFormField>
                  <UFormField label="密码">
                    <UInput v-model="profile.password" type="password" placeholder="密码" class="w-full" />
                  </UFormField>
                  <UFormField label="优先级">
                    <UInput v-model.number="profile.priority" type="number" class="w-full" />
                  </UFormField>
                  <div class="flex items-end">
                    <UButton size="sm" color="error" variant="outline" icon="i-lucide-trash-2" :disabled="wifiProfiles.length === 1" @click="removeWifiProfile(profile.id)" />
                  </div>
                </div>
              </div>
            </div>

            <UFormField label="后台地址">
              <div class="flex items-center gap-2">
                <UInput v-model="backendOrigin" placeholder="http://192.168.x.x:3001" class="min-w-0 flex-1" />
                <UIcon
                  v-if="backendStatus === 'checking'"
                  name="i-lucide-loader-circle"
                  class="size-4 shrink-0 animate-spin text-muted"
                />
                <UIcon
                  v-else-if="backendStatus === 'ok'"
                  name="i-lucide-circle-check"
                  class="size-4 shrink-0 text-success"
                />
                <UIcon
                  v-else-if="backendStatus === 'error'"
                  name="i-lucide-circle-x"
                  class="size-4 shrink-0 text-error"
                />
              </div>
              <p v-if="backendStatus === 'error'" class="mt-1 text-xs text-error">后台地址不可达，请检查 IP 和端口。</p>
            </UFormField>

            <UButton icon="i-lucide-zap" :loading="step === 'writing' || busy" :disabled="!canWriteConfig" class="w-full rounded-2xl" @click="writeAndActivate(true)">
              写入配置并激活
            </UButton>
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
              <div v-if="showProvisionForm" class="space-y-4 border-t border-neutral-200/70 p-4 dark:border-neutral-800/80">
                <div class="space-y-3">
                  <div class="flex items-center justify-between">
                    <span class="text-sm font-medium text-highlighted">Wi‑Fi 网络</span>
                    <UButton size="xs" variant="outline" icon="i-lucide-plus" @click="addWifiProfile">添加</UButton>
                  </div>
                  <div v-for="profile in wifiProfiles" :key="profile.id" class="rounded-2xl border border-neutral-200/70 p-3 dark:border-neutral-800/80">
                    <div class="grid gap-3 sm:grid-cols-[1fr_1fr_80px_auto]">
                      <UFormField label="SSID">
                        <UInput v-model="profile.ssid" placeholder="网络名称" class="w-full" />
                      </UFormField>
                      <UFormField label="密码">
                        <UInput v-model="profile.password" type="password" placeholder="密码" class="w-full" />
                      </UFormField>
                      <UFormField label="优先级">
                        <UInput v-model.number="profile.priority" type="number" class="w-full" />
                      </UFormField>
                      <div class="flex items-end">
                        <UButton size="sm" color="error" variant="outline" icon="i-lucide-trash-2" :disabled="wifiProfiles.length === 1" @click="removeWifiProfile(profile.id)" />
                      </div>
                    </div>
                  </div>
                </div>
                <UFormField label="后台地址">
                  <UInput v-model="backendOrigin" placeholder="http://192.168.x.x:3001" class="w-full" />
                </UFormField>
                <UButton size="sm" variant="outline" icon="i-lucide-zap" :disabled="!canWriteConfig" @click="writeAndActivate(false)">
                  写入配置并激活
                </UButton>
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
