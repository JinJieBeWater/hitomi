import { computed, ref } from "vue";

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

export type DeviceSerialSummary = {
  schemaVersion: number;
  wifiProfileCount: number;
  backendOrigin: string;
  deviceSerial: string;
  activationState: "activated" | "pending_activation" | "unconfigured";
  deviceCode: string;
  lastErrorCode: string | null;
};

type DeviceSerialResponse = {
  ok: boolean;
  message: string;
  summary?: DeviceSerialSummary;
};

type BrowserSerialPort = {
  readable: ReadableStream<Uint8Array> | null;
  writable: WritableStream<Uint8Array> | null;
  open(options: { baudRate: number }): Promise<void>;
  close(): Promise<void>;
};

type BrowserSerialApi = {
  requestPort(): Promise<BrowserSerialPort>;
};

function getSerialApi(): BrowserSerialApi | null {
  if (!import.meta.client || typeof navigator === "undefined") {
    return null;
  }

  return (navigator as Navigator & { serial?: BrowserSerialApi }).serial ?? null;
}

function isProvisioningResponse(value: unknown): value is DeviceSerialResponse {
  if (!value || typeof value !== "object") {
    return false;
  }

  const candidate = value as Record<string, unknown>;
  return typeof candidate.ok === "boolean" && typeof candidate.message === "string";
}

function timeoutPromise(timeoutMs: number) {
  return new Promise<never>((_, reject) => {
    setTimeout(() => reject(new Error("串口响应超时")), timeoutMs);
  });
}

const MAX_LOG_LINES = 500;

export function useDeviceSerial() {
  const port = ref<BrowserSerialPort | null>(null);
  const reader = ref<ReadableStreamDefaultReader<Uint8Array> | null>(null);
  const writer = ref<WritableStreamDefaultWriter<Uint8Array> | null>(null);
  const lineBuffer = ref("");
  const lastRawLine = ref("");
  const serialLogs = ref<string[]>([]);
  const supported = computed(() => getSerialApi() !== null);
  const connected = computed(() => port.value !== null);
  const encoder = new TextEncoder();
  const decoder = new TextDecoder();

  async function connect() {
    const serialApi = getSerialApi();
    if (!serialApi) {
      throw new Error("当前浏览器不支持 Web Serial，请改用 Chromium 浏览器。");
    }

    const nextPort = await serialApi.requestPort();
    await nextPort.open({ baudRate: 115200 });

    if (!nextPort.readable || !nextPort.writable) {
      await nextPort.close();
      throw new Error("串口不可读写。");
    }

    port.value = nextPort;
    reader.value = nextPort.readable.getReader();
    writer.value = nextPort.writable.getWriter();
    lineBuffer.value = "";
  }

  async function disconnect() {
    try {
      await reader.value?.cancel();
    } catch {}
    try {
      reader.value?.releaseLock();
    } catch {}
    try {
      writer.value?.releaseLock();
    } catch {}
    try {
      await port.value?.close();
    } catch {}

    port.value = null;
    reader.value = null;
    writer.value = null;
    lineBuffer.value = "";
  }

  function clearLogs() {
    serialLogs.value = [];
  }

  function appendLog(line: string) {
    if (!line) return;
    serialLogs.value.push(line);
    if (serialLogs.value.length > MAX_LOG_LINES) {
      serialLogs.value.splice(0, serialLogs.value.length - MAX_LOG_LINES);
    }
  }

  async function readLine(timeoutMs = 5000): Promise<string> {
    const activeReader = reader.value;
    if (!activeReader) {
      throw new Error("串口尚未连接。");
    }

    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const newlineIndex = lineBuffer.value.indexOf("\n");
      if (newlineIndex >= 0) {
        const line = lineBuffer.value.slice(0, newlineIndex).replace(/\r$/, "");
        lineBuffer.value = lineBuffer.value.slice(newlineIndex + 1);
        lastRawLine.value = line;
        appendLog(line);
        return line;
      }

      const result = await Promise.race([
        activeReader.read(),
        timeoutPromise(Math.max(1, deadline - Date.now())),
      ]);
      if (result.done) {
        throw new Error("串口已断开。");
      }
      lineBuffer.value += decoder.decode(result.value, { stream: true });
    }

    throw new Error("读取串口超时。");
  }

  async function sendCommand(command: SerialCommand, timeoutMs = 8000) {
    const activeWriter = writer.value;
    if (!activeWriter) {
      throw new Error("串口尚未连接。");
    }

    await activeWriter.write(encoder.encode(`${JSON.stringify(command)}\n`));

    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
      const line = await readLine(Math.max(1, deadline - Date.now()));
      if (!line.trim().startsWith("{")) {
        continue;
      }

      try {
        const parsed = JSON.parse(line);
        if (isProvisioningResponse(parsed)) {
          return parsed;
        }
      } catch {
        // ignore runtime log lines
      }
    }

    throw new Error("未收到有效的设备响应。");
  }

  return {
    supported,
    connected,
    lastRawLine,
    serialLogs,
    clearLogs,
    connect,
    disconnect,
    sendCommand,
  };
}
