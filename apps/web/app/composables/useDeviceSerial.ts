import { computed, ref } from "vue";

import {
  resolveDeviceSerialSupport,
  type DeviceSerialSupportInfo,
} from "~/utils/deviceSerialSupport";

type SerialCommand =
  | { type: "get_config" }
  | {
      type: "set_wifi_profiles";
      profiles: Array<{
        ssid: string;
        password: string;
        priority: number;
        lastSuccessAt?: number;
        disabled?: boolean;
      }>;
    }
  | { type: "set_backend_origin"; origin: string }
  | { type: "set_bootstrap_identity"; deviceSerial: string; bootstrapSecret: string }
  | { type: "clear_runtime_credentials" }
  | { type: "clear_wifi_profiles" }
  | { type: "reset_device_config" };

export type DeviceSerialWifiProfile = {
  ssid: string;
  password: string;
  priority: number;
  lastSuccessAt: number;
  disabled: boolean;
};

type DeviceActivationState = "activated" | "pending_activation" | "unconfigured";

export type DeviceSerialSummary = {
  schemaVersion: number;
  wifiProfileCount: number;
  wifiProfiles: DeviceSerialWifiProfile[];
  backendOrigin: string;
  deviceSerial: string;
  activationState: DeviceActivationState;
  deviceCode: string;
  lastErrorCode: string | null;
};

type DeviceSerialResponse = {
  ok: boolean;
  message: string;
  summary?: DeviceSerialSummary;
};

const DEVICE_SERIAL_RESPONSE_PREFIX = "HITOMI_USB_RESPONSE ";

type BrowserSerialPort = {
  connected?: boolean;
  readable: ReadableStream<Uint8Array> | null;
  writable: WritableStream<Uint8Array> | null;
  open(options: { baudRate: number }): Promise<void>;
  close(): Promise<void>;
};

type BrowserSerialApi = EventTarget & {
  getPorts(): Promise<BrowserSerialPort[]>;
  requestPort(): Promise<BrowserSerialPort>;
};

type RestoreAuthorizedPortResult =
  | { status: "restored"; port: BrowserSerialPort }
  | { status: "none" }
  | { status: "multiple" };

type BrowserBrand = {
  brand?: string;
};

type NavigatorWithSerial = Navigator & {
  serial?: BrowserSerialApi;
  userAgentData?: {
    brands?: BrowserBrand[];
  };
};

function getSerialApi(): BrowserSerialApi | null {
  if (!import.meta.client || typeof navigator === "undefined") {
    return null;
  }

  return (navigator as NavigatorWithSerial).serial ?? null;
}

function isChromiumBrowser() {
  if (!import.meta.client || typeof navigator === "undefined") {
    return false;
  }

  const brands = (navigator as NavigatorWithSerial).userAgentData?.brands ?? [];
  if (
    brands.some((item) =>
      /\b(Chromium|Google Chrome|Microsoft Edge|Opera)\b/i.test(item.brand || ""),
    )
  ) {
    return true;
  }

  const userAgent = navigator.userAgent || "";
  return /\b(Chrome|Chromium|Edg|OPR)\b/i.test(userAgent) && !/\bFirefox\b/i.test(userAgent);
}

function getDeviceSerialSupportInfo(): DeviceSerialSupportInfo {
  if (!import.meta.client || typeof window === "undefined") {
    return resolveDeviceSerialSupport({
      hasSerialApi: false,
      isSecureContext: false,
      isChromiumBrowser: false,
    });
  }

  return resolveDeviceSerialSupport({
    hasSerialApi: getSerialApi() !== null,
    isSecureContext: window.isSecureContext,
    isChromiumBrowser: isChromiumBrowser(),
  });
}

function buildUnsupportedSerialMessage() {
  const support = getDeviceSerialSupportInfo();

  if (support.reason === "insecure_context") {
    return "当前页面不是安全上下文。Chrome 仅在 HTTPS 或 localhost 下可用 Web Serial。";
  }

  if (support.reason === "unsupported_browser") {
    return "当前浏览器不支持 Web Serial，请改用桌面版 Chromium 浏览器。";
  }

  return "当前页面暂时无法访问 Web Serial，请确认你使用的是桌面版 Chromium 浏览器，并通过 HTTPS 或 localhost 打开后台。";
}

function isProvisioningResponse(value: unknown): value is DeviceSerialResponse {
  if (!value || typeof value !== "object") {
    return false;
  }

  const candidate = value as Record<string, unknown>;
  return typeof candidate.ok === "boolean" && typeof candidate.message === "string";
}

function normalizeActivationState(value: unknown): DeviceActivationState {
  if (value === "activated") {
    return "activated";
  }

  if (value === "pending" || value === "pending_activation") {
    return "pending_activation";
  }

  return "unconfigured";
}

function normalizeWifiProfile(value: unknown): DeviceSerialWifiProfile | null {
  if (!value || typeof value !== "object") {
    return null;
  }

  const candidate = value as Record<string, unknown>;

  return {
    ssid: typeof candidate.ssid === "string" ? candidate.ssid : "",
    password: typeof candidate.password === "string" ? candidate.password : "",
    priority: typeof candidate.priority === "number" ? candidate.priority : 0,
    lastSuccessAt: typeof candidate.lastSuccessAt === "number" ? candidate.lastSuccessAt : 0,
    disabled: candidate.disabled === true,
  };
}

function normalizeSummary(value: unknown): DeviceSerialSummary | undefined {
  if (!value || typeof value !== "object") {
    return undefined;
  }

  const candidate = value as Record<string, unknown>;
  const rawWifiProfiles = Array.isArray(candidate.wifiProfiles) ? candidate.wifiProfiles : [];

  return {
    schemaVersion: typeof candidate.schemaVersion === "number" ? candidate.schemaVersion : 1,
    wifiProfileCount:
      typeof candidate.wifiProfileCount === "number" ? candidate.wifiProfileCount : 0,
    wifiProfiles: rawWifiProfiles
      .map((profile) => normalizeWifiProfile(profile))
      .filter((profile): profile is DeviceSerialWifiProfile => profile !== null),
    backendOrigin: typeof candidate.backendOrigin === "string" ? candidate.backendOrigin : "",
    deviceSerial: typeof candidate.deviceSerial === "string" ? candidate.deviceSerial : "",
    activationState: normalizeActivationState(candidate.activationState),
    deviceCode: typeof candidate.deviceCode === "string" ? candidate.deviceCode : "",
    lastErrorCode: typeof candidate.lastErrorCode === "string" ? candidate.lastErrorCode : null,
  };
}

function redactSensitiveLogValue(value: unknown): unknown {
  if (Array.isArray(value)) {
    return value.map((item) => redactSensitiveLogValue(item));
  }

  if (!value || typeof value !== "object") {
    return value;
  }

  const candidate = value as Record<string, unknown>;
  const redacted: Record<string, unknown> = {};

  for (const [key, nestedValue] of Object.entries(candidate)) {
    if (key === "password" || key === "bootstrapSecret" || key === "apiKey") {
      redacted[key] = "***";
      continue;
    }

    redacted[key] = redactSensitiveLogValue(nestedValue);
  }

  return redacted;
}

function sanitizeLogLine(line: string) {
  if (!line.trim().startsWith("{")) {
    return line;
  }

  try {
    return JSON.stringify(redactSensitiveLogValue(JSON.parse(line)));
  } catch {
    return line;
  }
}

function timeoutPromise(timeoutMs: number) {
  return new Promise<never>((_, reject) => {
    setTimeout(() => reject(new Error("串口响应超时")), timeoutMs);
  });
}

function extractJsonObjectFromBuffer(buffer: string) {
  const start = buffer.indexOf("{");
  if (start < 0) {
    return null;
  }

  let depth = 0;
  let inString = false;
  let escaped = false;

  for (let index = start; index < buffer.length; index += 1) {
    const ch = buffer[index];

    if (inString) {
      if (escaped) {
        escaped = false;
      } else if (ch === "\\") {
        escaped = true;
      } else if (ch === '"') {
        inString = false;
      }
      continue;
    }

    if (ch === '"') {
      inString = true;
      continue;
    }

    if (ch === "{") {
      depth += 1;
      continue;
    }

    if (ch === "}") {
      depth -= 1;
      if (depth === 0) {
        return {
          json: buffer.slice(start, index + 1),
          before: buffer.slice(0, start),
          after: buffer.slice(index + 1),
        };
      }
    }
  }

  return null;
}

function extractFramedResponseFromBuffer(buffer: string) {
  const prefixStart = buffer.indexOf(DEVICE_SERIAL_RESPONSE_PREFIX);
  if (prefixStart < 0) {
    return null;
  }

  const jsonStart = prefixStart + DEVICE_SERIAL_RESPONSE_PREFIX.length;
  const extracted = extractJsonObjectFromBuffer(buffer.slice(jsonStart));
  if (!extracted) {
    return null;
  }

  return {
    json: extracted.json,
    before: buffer.slice(0, prefixStart),
    after: extracted.after,
  };
}

const MAX_LOG_LINES = 500;

const sharedPort = ref<BrowserSerialPort | null>(null);
const sharedReader = ref<ReadableStreamDefaultReader<Uint8Array> | null>(null);
const sharedWriter = ref<WritableStreamDefaultWriter<Uint8Array> | null>(null);
const sharedLineBuffer = ref("");
const sharedLastRawLine = ref("");
const sharedSerialLogs = ref<string[]>([]);
const sharedSupportInfo = computed(() => getDeviceSerialSupportInfo());
const sharedSupported = computed(() => sharedSupportInfo.value.available);
const sharedConnected = computed(() => sharedPort.value !== null);
const encoder = new TextEncoder();
const decoder = new TextDecoder();
let sharedEventsBound = false;

export function formatDeviceSerialConnectError(
  error: any,
  fallback = "连接设备失败，请检查 USB 连接后重试。",
) {
  const message = error?.message || "";
  const name = error?.name || "";

  if (
    name === "NotFoundError" ||
    /No port selected/i.test(message) ||
    /requestPort/i.test(message)
  ) {
    return "未选择串口设备。";
  }

  if (name === "NetworkError") {
    return "串口连接失败，请确认设备未被其他程序占用。";
  }

  return message || fallback;
}

function shouldResetConnection(error: any) {
  const message = error?.message || "";
  const name = error?.name || "";

  return (
    name === "NetworkError" ||
    name === "InvalidStateError" ||
    /串口已断开/.test(message) ||
    /device has been lost/i.test(message)
  );
}

function isPortLogicallyConnected(port: BrowserSerialPort | null | undefined) {
  return Boolean(port && port.connected !== false);
}

function isSharedConnectionHealthy() {
  return Boolean(
    sharedPort.value &&
    isPortLogicallyConnected(sharedPort.value) &&
    sharedPort.value.readable &&
    sharedPort.value.writable &&
    sharedReader.value &&
    sharedWriter.value,
  );
}

function resetSharedConnectionState() {
  sharedPort.value = null;
  sharedReader.value = null;
  sharedWriter.value = null;
  sharedLineBuffer.value = "";
}

function resetSharedSessionOutput() {
  sharedSerialLogs.value = [];
  sharedLastRawLine.value = "";
}

async function teardownSharedConnection(options: { closePort?: boolean } = {}) {
  const { closePort = true } = options;
  const activePort = sharedPort.value;

  try {
    await sharedReader.value?.cancel();
  } catch {}
  try {
    sharedReader.value?.releaseLock();
  } catch {}
  try {
    sharedWriter.value?.releaseLock();
  } catch {}
  if (closePort && activePort) {
    try {
      await activePort.close();
    } catch {}
  }

  resetSharedConnectionState();
}

async function openSharedPort(port: BrowserSerialPort) {
  if (!port.readable || !port.writable) {
    await port.open({ baudRate: 115200 });
  }

  if (!port.readable || !port.writable) {
    throw new Error("串口不可读写。");
  }

  sharedPort.value = port;
  sharedReader.value = port.readable.getReader();
  sharedWriter.value = port.writable.getWriter();
  sharedLineBuffer.value = "";
}

function ensureSerialEventHandlers() {
  if (sharedEventsBound) {
    return;
  }

  const serialApi = getSerialApi();
  if (!serialApi) {
    return;
  }

  serialApi.addEventListener("disconnect", (event) => {
    const disconnectedPort =
      (event as Event & { port?: BrowserSerialPort | null }).port ??
      ((event.target as BrowserSerialPort | null) || null);

    if (disconnectedPort && disconnectedPort === sharedPort.value) {
      void teardownSharedConnection({ closePort: false });
    }
  });

  sharedEventsBound = true;
}

export function useDeviceSerial() {
  async function restoreAuthorizedPort(): Promise<RestoreAuthorizedPortResult> {
    ensureSerialEventHandlers();

    if (isSharedConnectionHealthy()) {
      return { status: "restored", port: sharedPort.value! };
    }

    const serialApi = getSerialApi();
    if (!serialApi) {
      return { status: "none" };
    }

    const ports = await serialApi.getPorts();
    if (ports.length === 0) {
      return { status: "none" };
    }

    if (ports.length > 1) {
      return { status: "multiple" };
    }

    const nextPort = ports[0];
    if (!nextPort) {
      return { status: "none" };
    }

    if (nextPort !== sharedPort.value) {
      resetSharedSessionOutput();
    }

    await teardownSharedConnection({ closePort: false });
    await openSharedPort(nextPort);
    return { status: "restored", port: nextPort };
  }

  async function requestPortConnection() {
    ensureSerialEventHandlers();

    const serialApi = getSerialApi();
    if (!serialApi) {
      throw new Error(buildUnsupportedSerialMessage());
    }

    const nextPort = await serialApi.requestPort();
    if (nextPort === sharedPort.value && isSharedConnectionHealthy()) {
      return nextPort;
    }

    if (nextPort !== sharedPort.value) {
      resetSharedSessionOutput();
    }

    await teardownSharedConnection({ closePort: nextPort !== sharedPort.value });
    await openSharedPort(nextPort);
    return nextPort;
  }

  async function disconnect() {
    await teardownSharedConnection();
  }

  function clearLogs() {
    sharedSerialLogs.value = [];
  }

  function appendLog(line: string) {
    if (!line) return;
    sharedSerialLogs.value.push(sanitizeLogLine(line));
    if (sharedSerialLogs.value.length > MAX_LOG_LINES) {
      sharedSerialLogs.value.splice(0, sharedSerialLogs.value.length - MAX_LOG_LINES);
    }
  }

  async function readLine(timeoutMs = 5000): Promise<string> {
    const activeReader = sharedReader.value;
    if (!activeReader) {
      throw new Error("串口尚未连接。");
    }

    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const newlineIndex = sharedLineBuffer.value.indexOf("\n");
      if (newlineIndex >= 0) {
        const line = sharedLineBuffer.value.slice(0, newlineIndex).replace(/\r$/, "");
        sharedLineBuffer.value = sharedLineBuffer.value.slice(newlineIndex + 1);
        sharedLastRawLine.value = sanitizeLogLine(line);
        appendLog(line);
        return line;
      }

      const result = await Promise.race([
        activeReader.read(),
        timeoutPromise(Math.max(1, deadline - Date.now())),
      ]);
      if (result.done) {
        await teardownSharedConnection({ closePort: false });
        throw new Error("串口已断开。");
      }
      sharedLineBuffer.value += decoder.decode(result.value, { stream: true });
    }

    throw new Error("读取串口超时。");
  }

  async function sendCommand(command: SerialCommand, timeoutMs = 8000) {
    const activeWriter = sharedWriter.value;
    if (!activeWriter) {
      throw new Error("串口尚未连接。");
    }

    try {
      await activeWriter.write(encoder.encode(`${JSON.stringify(command)}\n`));

      const deadline = Date.now() + timeoutMs;
      while (Date.now() < deadline) {
        const extracted =
          extractFramedResponseFromBuffer(sharedLineBuffer.value) ??
          extractJsonObjectFromBuffer(sharedLineBuffer.value);
        if (extracted) {
          if (extracted.before.trim()) {
            appendLog(extracted.before.replace(/\r?\n$/, ""));
          }
          sharedLineBuffer.value = extracted.after;
          sharedLastRawLine.value = sanitizeLogLine(extracted.json);
          appendLog(extracted.json);

          try {
            const parsed = JSON.parse(extracted.json);
            if (isProvisioningResponse(parsed)) {
              return {
                ...parsed,
                summary: normalizeSummary(parsed.summary),
              };
            }
          } catch {
            continue;
          }
        }

        const line = await readLine(Math.max(1, deadline - Date.now()));
        if (!line.trim().startsWith("{")) {
          continue;
        }

        try {
          const parsed = JSON.parse(line);
          if (isProvisioningResponse(parsed)) {
            return {
              ...parsed,
              summary: normalizeSummary(parsed.summary),
            };
          }
        } catch {
          // ignore runtime log lines
        }
      }
    } catch (error) {
      if (shouldResetConnection(error)) {
        await teardownSharedConnection({ closePort: false });
      }
      throw error;
    }

    throw new Error("未收到有效的设备响应。");
  }

  return {
    supportInfo: sharedSupportInfo,
    supported: sharedSupported,
    connected: sharedConnected,
    lastRawLine: sharedLastRawLine,
    serialLogs: sharedSerialLogs,
    clearLogs,
    restoreAuthorizedPort,
    requestPortConnection,
    disconnect,
    sendCommand,
  };
}
