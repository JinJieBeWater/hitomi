import { spawn } from "node:child_process";
import { mkdir } from "node:fs/promises";
import { createServer } from "node:net";
import { dirname, resolve } from "node:path";
import { setTimeout as delay } from "node:timers/promises";
import { fileURLToPath, pathToFileURL } from "node:url";
import { config } from "dotenv";

process.env.TZ = "Asia/Shanghai";

const repoRoot = fileURLToPath(new URL("../../..", import.meta.url));
const appRoot = fileURLToPath(new URL("../../../apps/web", import.meta.url));
const envPath = fileURLToPath(new URL("../../../apps/web/.env", import.meta.url));
const localDbUrl = pathToFileURL(fileURLToPath(new URL("../../../local.db", import.meta.url))).href;
const artifactsDir = resolve(repoRoot, "output/playwright");
const devServerTempDir = resolve(repoRoot, "output/tmp/smoke-admin-ui");
const defaultAttendanceConfigId = "default";

config({
  path: envPath,
});

function normalizeDatabaseUrl(value: string | undefined) {
  if (!value) {
    return localDbUrl;
  }

  if (!value.startsWith("file:")) {
    return value;
  }

  const filePath = value.slice("file:".length);

  if (!filePath) {
    return localDbUrl;
  }

  if (filePath.startsWith("//")) {
    return value;
  }

  if (filePath.startsWith("/")) {
    return pathToFileURL(filePath).href;
  }

  return pathToFileURL(resolve(dirname(envPath), filePath)).href;
}

process.env.DATABASE_URL = normalizeDatabaseUrl(process.env.DATABASE_URL);

const [{ eq }, { chromium }, dbModule] = await Promise.all([
  import("drizzle-orm"),
  import("playwright-core"),
  import("@hitomi/db"),
]);

const {
  attendanceConfig,
  attendanceRecord,
  db,
  device,
  employee,
  faceProfile,
  user,
  verification,
} = dbModule;

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

function createId(prefix: string) {
  return `${prefix}_${Date.now()}_${Math.random().toString(36).slice(2, 8)}`;
}

function dateAtMinute(minute: number) {
  const date = new Date();

  date.setHours(0, 0, 0, 0);
  date.setMinutes(minute, 0, 0);

  return date;
}

function localDateString(date: Date) {
  const year = date.getFullYear();
  const month = `${date.getMonth() + 1}`.padStart(2, "0");
  const day = `${date.getDate()}`.padStart(2, "0");

  return `${year}-${month}-${day}`;
}

async function isServerReachable(baseUrl: string) {
  try {
    const response = await fetch(`${baseUrl}/login`, {
      redirect: "manual",
    });

    return response.ok || response.status === 302;
  } catch {
    return false;
  }
}

async function waitForServer(baseUrl: string, timeoutMs = 60_000) {
  const startedAt = Date.now();

  while (Date.now() - startedAt < timeoutMs) {
    if (await isServerReachable(baseUrl)) {
      return;
    }

    await delay(500);
  }

  throw new Error(`Timed out waiting for server: ${baseUrl}`);
}

async function findAvailablePort(host = "127.0.0.1") {
  return await new Promise<number>((resolvePromise, rejectPromise) => {
    const server = createServer();

    server.once("error", rejectPromise);
    server.once("listening", () => {
      const address = server.address();

      if (!address || typeof address === "string") {
        server.close(() => {
          rejectPromise(new Error("Unable to determine admin UI smoke port"));
        });
        return;
      }

      server.close((error) => {
        if (error) {
          rejectPromise(error);
          return;
        }

        resolvePromise(address.port);
      });
    });
    server.listen(0, host);
  });
}

async function startServer(baseUrl: string) {
  const url = new URL(baseUrl);
  await mkdir(devServerTempDir, { recursive: true });
  await runAppCommand(["run", "build"], "Nuxt build", {
    TMPDIR: devServerTempDir,
  });

  let stdout = "";
  let stderr = "";
  const child = spawn("bun", ["run", "preview", "--", "--port", url.port], {
    cwd: appRoot,
    env: {
      ...process.env,
      HOST: url.hostname,
      NITRO_HOST: url.hostname,
      PORT: url.port,
      NITRO_PORT: url.port,
      TMPDIR: devServerTempDir,
    },
    stdio: ["ignore", "pipe", "pipe"],
  });

  child.stdout?.on("data", (chunk) => {
    stdout += chunk.toString();
  });
  child.stderr?.on("data", (chunk) => {
    stderr += chunk.toString();
  });

  try {
    await waitForServer(baseUrl);
  } catch (error) {
    child.kill("SIGTERM");
    await delay(500);
    const logs = [stderr.trim(), stdout.trim()].filter(Boolean).join("\n");
    throw new Error(logs ? `${String(error)}\nNuxt server output:\n${logs}` : String(error));
  }

  return child;
}

async function runAppCommand(args: string[], label: string, envOverrides: Record<string, string>) {
  await new Promise<void>((resolve, reject) => {
    let stdout = "";
    let stderr = "";

    const child = spawn("bun", args, {
      cwd: appRoot,
      env: {
        ...process.env,
        ...envOverrides,
      },
      stdio: ["ignore", "pipe", "pipe"],
    });

    child.stdout?.on("data", (chunk) => {
      stdout += chunk.toString();
    });
    child.stderr?.on("data", (chunk) => {
      stderr += chunk.toString();
    });
    child.once("error", reject);
    child.once("exit", (code) => {
      if (code === 0) {
        resolve();
        return;
      }

      const logs = [stderr.trim(), stdout.trim()].filter(Boolean).join("\n");
      reject(
        new Error(
          logs ? `${label} failed with code ${code}\n${logs}` : `${label} failed with code ${code}`,
        ),
      );
    });
  });
}

async function stopServer(server: ReturnType<typeof spawn>) {
  if (server.exitCode !== null) {
    return;
  }

  const waitForExit = (timeoutMs: number) =>
    new Promise<void>((resolve) => {
      if (server.exitCode !== null) {
        resolve();
        return;
      }

      const timeout = setTimeout(resolve, timeoutMs);
      server.once("exit", () => {
        clearTimeout(timeout);
        resolve();
      });
    });

  server.kill("SIGTERM");
  await waitForExit(2_000);

  if (server.exitCode === null) {
    server.kill("SIGKILL");
    await waitForExit(1_000);
  }

  if (server.exitCode === null) {
    server.stdout?.destroy();
    server.stderr?.destroy();
    server.unref();
  }
}

async function launchBrowser() {
  try {
    return await chromium.launch({
      channel: "chrome",
      headless: true,
    });
  } catch (chromeError) {
    try {
      return await chromium.launch({
        headless: true,
      });
    } catch {
      throw new Error(
        `Unable to launch browser for admin UI smoke. Install Google Chrome or run "bun x playwright install chromium". Original error: ${String(
          chromeError,
        )}`,
      );
    }
  }
}

async function clickNav(page: any, href: string, title: string) {
  await page.goto(`${baseUrl}${href}`);
  await page.waitForURL((url: URL) => url.pathname === href);
  await page.getByRole("heading", { name: title, exact: true }).waitFor();
}

async function clickRefresh(page: any) {
  await page.getByRole("button", { name: "刷新" }).click();
}

async function selectRowAction(page: any, row: any, actionLabel: string) {
  await row.getByRole("button", { name: "更多操作" }).click();
  await page.getByRole("menuitem", { name: actionLabel, exact: true }).click();
}

async function cleanupData(input: {
  email: string;
  employeeCode: string;
  deviceName: string;
  initialConfig: null | {
    workStartMinute: number;
    workEndMinute: number;
    offStartMinute: number;
    offEndMinute: number;
  };
}) {
  const currentEmployee =
    (await db.query.employee.findFirst({
      where: eq(employee.code, input.employeeCode),
    })) ?? null;
  const currentDevice =
    (await db.query.device.findFirst({
      where: eq(device.name, input.deviceName),
    })) ?? null;
  const currentUser =
    (await db.query.user.findFirst({
      where: eq(user.email, input.email),
    })) ?? null;

  if (currentEmployee) {
    await db.delete(attendanceRecord).where(eq(attendanceRecord.employeeId, currentEmployee.id));
    await db.delete(faceProfile).where(eq(faceProfile.employeeId, currentEmployee.id));
  }

  if (currentDevice) {
    await db.delete(attendanceRecord).where(eq(attendanceRecord.deviceId, currentDevice.id));
    await db.delete(faceProfile).where(eq(faceProfile.deviceId, currentDevice.id));
    await db.delete(device).where(eq(device.id, currentDevice.id));
  }

  if (currentEmployee) {
    await db.delete(employee).where(eq(employee.id, currentEmployee.id));
  }

  await db.delete(verification).where(eq(verification.identifier, input.email));

  if (currentUser) {
    await db.delete(user).where(eq(user.id, currentUser.id));
  }

  if (input.initialConfig) {
    const currentConfig =
      (await db.query.attendanceConfig.findFirst({
        where: eq(attendanceConfig.id, defaultAttendanceConfigId),
      })) ?? null;

    if (!currentConfig) {
      await db.insert(attendanceConfig).values({
        id: defaultAttendanceConfigId,
        workStartMinute: input.initialConfig.workStartMinute,
        workEndMinute: input.initialConfig.workEndMinute,
        offStartMinute: input.initialConfig.offStartMinute,
        offEndMinute: input.initialConfig.offEndMinute,
      });
    } else {
      await db
        .update(attendanceConfig)
        .set({
          workStartMinute: input.initialConfig.workStartMinute,
          workEndMinute: input.initialConfig.workEndMinute,
          offStartMinute: input.initialConfig.offStartMinute,
          offEndMinute: input.initialConfig.offEndMinute,
        })
        .where(eq(attendanceConfig.id, defaultAttendanceConfigId));
    }
  } else {
    await db.delete(attendanceConfig).where(eq(attendanceConfig.id, defaultAttendanceConfigId));
  }
}

const host = "127.0.0.1";
const port = await findAvailablePort(host);
const baseUrl = `http://${host}:${port}`;
const server = await startServer(baseUrl);

const suffix = Date.now().toString(36);
const authName = `Smoke Admin ${suffix}`;
const authEmail = `smoke-admin-ui-${suffix}@example.com`;
const authPassword = `SmokePass_${suffix}!`;
const employeeCode = `UI${Date.now().toString().slice(-6)}`;
const employeeName = `UI Smoke Employee ${suffix}`;
const employeeUpdatedName = `${employeeName} Updated`;
const deviceCode = `DEV-UI-${Date.now().toString().slice(-8)}`;
const deviceName = `UI Smoke Device ${suffix}`;
const attendanceRecordId = createId("smoke_ui_ar");
const initialConfig =
  (await db.query.attendanceConfig.findFirst({
    where: eq(attendanceConfig.id, defaultAttendanceConfigId),
  })) ?? null;

let browser: Awaited<ReturnType<typeof launchBrowser>> | null = null;
let context: Awaited<ReturnType<Awaited<ReturnType<typeof launchBrowser>>["newContext"]>> | null =
  null;
let page: Awaited<
  ReturnType<
    Awaited<ReturnType<Awaited<ReturnType<typeof launchBrowser>>["newContext"]>>["newPage"]
  >
> | null = null;

await mkdir(artifactsDir, { recursive: true });

try {
  browser = await launchBrowser();
  context = await browser.newContext({
    viewport: {
      width: 1440,
      height: 960,
    },
  });
  page = await context.newPage();

  await page.goto(`${baseUrl}/login`);
  await page.getByText("管理员登录").waitFor();
  await page.getByRole("button", { name: "去注册" }).click();
  await page.getByText("管理员注册").waitFor();
  await page.getByPlaceholder("请输入姓名").fill(authName);
  await page.getByPlaceholder("请输入邮箱").fill(authEmail);
  await page.getByPlaceholder("请输入密码").fill(authPassword);
  await page.getByRole("button", { name: "注册" }).click();
  await page.waitForURL((url: URL) => url.pathname === "/dashboard");
  await page.getByRole("heading", { name: "概览", exact: true }).waitFor();

  await page.getByTestId("sign-out-button").click();
  await page.waitForURL((url: URL) => url.pathname === "/");

  await page.goto(`${baseUrl}/dashboard`);
  await page.waitForURL((url: URL) => url.pathname === "/login");
  await page.getByPlaceholder("请输入邮箱").fill(authEmail);
  await page.getByPlaceholder("请输入密码").fill(authPassword);
  await page.getByRole("button", { name: "登录" }).click();
  await page.waitForURL((url: URL) => url.pathname === "/dashboard");
  await page.getByRole("heading", { name: "概览", exact: true }).waitFor();

  await db.insert(employee).values({
    id: createId("ui_emp"),
    code: employeeCode,
    name: employeeName,
  });
  await db.insert(device).values({
    id: createId("ui_dev"),
    deviceCode,
    name: deviceName,
    apiKeyHash: `ui_smoke_hash_${suffix}`,
    status: "active",
  });

  await clickNav(page, "/employees", "员工管理");
  await clickRefresh(page);
  const employeeRow = page.locator("tr").filter({
    hasText: employeeCode,
  });

  await employeeRow.waitFor();
  await selectRowAction(page, employeeRow, "编辑");
  await page.getByTestId("employee-name-input").fill(employeeUpdatedName);
  await page.getByTestId("employee-submit-button").click();
  await clickRefresh(page);
  await page.locator("tr").filter({ hasText: employeeUpdatedName }).waitFor();

  await clickNav(page, "/devices", "设备管理");
  await clickRefresh(page);
  await page.locator("tr").filter({ hasText: deviceName }).waitFor();

  await clickNav(page, "/dashboard", "概览");
  await page.getByRole("heading", { name: "考勤配置", exact: true }).waitFor();
  await page.getByTestId("attendance-work-start-input").waitFor();
  await page.getByTestId("attendance-work-end-input").waitFor();
  await page.getByTestId("attendance-off-start-input").waitFor();
  await page.getByTestId("attendance-off-end-input").waitFor();

  const createdEmployee =
    (await db.query.employee.findFirst({
      where: eq(employee.code, employeeCode),
    })) ?? null;
  const createdDevice =
    (await db.query.device.findFirst({
      where: eq(device.name, deviceName),
    })) ?? null;

  assert(createdEmployee, "employee seeded for UI smoke should exist in database");
  assert(createdEmployee.name === employeeUpdatedName, "employee edit should persist updated name");
  assert(createdDevice, "device seeded for UI smoke should exist in database");

  const faceProfileId = createId("smoke_ui_fp");

  await db.insert(faceProfile).values({
    id: faceProfileId,
    employeeId: createdEmployee.id,
    deviceId: createdDevice.id,
    status: "pending",
  });

  await clickNav(page, "/face-profiles", "录脸记录");
  await clickRefresh(page);
  const faceProfileRow = page.locator("tr").filter({
    hasText: employeeUpdatedName,
  });

  await faceProfileRow.waitFor();
  await faceProfileRow.getByText("待录入").waitFor();
  await selectRowAction(page, faceProfileRow, "取消任务");
  await faceProfileRow.getByText("已取消").waitFor();

  const recognizedAt = dateAtMinute(510).getTime();
  const localDate = localDateString(new Date(recognizedAt));

  await db.insert(attendanceRecord).values({
    id: attendanceRecordId,
    employeeId: createdEmployee.id,
    deviceId: createdDevice.id,
    recognizedAt: new Date(recognizedAt),
    localDate,
    type: "clock_in",
  });

  await clickNav(page, "/attendance-records", "考勤记录");
  await page.getByTestId("attendance-record-employee-select").waitFor();
  await page.getByTestId("attendance-record-device-select").waitFor();
  await page.getByTestId("attendance-record-type-select").waitFor();
  await clickRefresh(page);
  await page.locator("tr").filter({ hasText: employeeCode }).waitFor();
  await page.locator("tr").filter({ hasText: deviceName }).waitFor();
  await page.locator("tr").filter({ hasText: "上班" }).waitFor();

  await clickNav(page, "/employees", "员工管理");
  await clickRefresh(page);
  const deletableEmployeeRow = page.locator("tr").filter({
    hasText: employeeUpdatedName,
  });

  await deletableEmployeeRow.waitFor();
  await selectRowAction(page, deletableEmployeeRow, "删除");
  await page.getByText(`员工编号: ${employeeCode}`).waitFor();
  await page.getByTestId("delete-confirm-input").fill("WRONG_EMPLOYEE_CODE");
  assert(
    await page.getByTestId("delete-confirm-submit-button").isDisabled(),
    "employee delete should stay disabled for mismatched confirm text",
  );
  await page.getByTestId("delete-confirm-input").fill(employeeCode);
  await page.getByTestId("delete-confirm-submit-button").click();
  await deletableEmployeeRow.waitFor({ state: "detached" });

  await clickNav(page, "/devices", "设备管理");
  await clickRefresh(page);
  const deletableDeviceRow = page.locator("tr").filter({
    hasText: deviceName,
  });

  await deletableDeviceRow.waitFor();
  await selectRowAction(page, deletableDeviceRow, "删除");
  await page.getByText(`设备码: ${deviceCode}`).waitFor();
  await page.getByTestId("delete-confirm-input").fill("WRONG_DEVICE_CODE");
  assert(
    await page.getByTestId("delete-confirm-submit-button").isDisabled(),
    "device delete should stay disabled for mismatched confirm text",
  );
  await page.getByTestId("delete-confirm-input").fill(deviceCode);
  await page.getByTestId("delete-confirm-submit-button").click();
  await deletableDeviceRow.waitFor({ state: "detached" });

  await page.getByTestId("sign-out-button").click();
  await page.waitForURL((url: URL) => url.pathname === "/");
  await page.goto(`${baseUrl}/dashboard`);
  await page.waitForURL((url: URL) => url.pathname === "/login");

  console.log("admin ui smoke test passed");
} catch (error) {
  if (page) {
    await page.screenshot({
      path: resolve(artifactsDir, "admin-ui-smoke-failure.png"),
      fullPage: true,
    });
  }

  throw error;
} finally {
  if (context) {
    await context.close();
  }

  if (browser) {
    await browser.close();
  }

  await cleanupData({
    email: authEmail,
    employeeCode,
    deviceName,
    initialConfig: initialConfig
      ? {
          workStartMinute: initialConfig.workStartMinute,
          workEndMinute: initialConfig.workEndMinute,
          offStartMinute: initialConfig.offStartMinute,
          offEndMinute: initialConfig.offEndMinute,
        }
      : null,
  });

  await stopServer(server);
}
