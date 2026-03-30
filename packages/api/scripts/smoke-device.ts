import { spawn } from "node:child_process";
import { createServer } from "node:net";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { pathToFileURL } from "node:url";
import { setTimeout as delay } from "node:timers/promises";
import { config } from "dotenv";
import { resolveAppOrigin } from "@hitomi/env/app";

process.env.TZ = "Asia/Shanghai";

const appRoot = fileURLToPath(new URL("../../../apps/web", import.meta.url));
const envPath = fileURLToPath(new URL("../../../apps/web/.env", import.meta.url));
const localDbUrl = pathToFileURL(fileURLToPath(new URL("../../../local.db", import.meta.url))).href;

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

const [{ and, eq }, dbModule] = await Promise.all([import("drizzle-orm"), import("@hitomi/db")]);

const {
  attendanceConfig,
  attendanceRecord,
  db,
  device,
  employee,
  faceProfile,
} = dbModule;

type DeviceFailure = {
  success: false;
  error: {
    code: string;
    message: string;
    retryable: boolean;
  };
};

type DeviceResponse<T> =
  | {
      success: true;
      data: T;
    }
  | DeviceFailure;

type SyncResponseData = {
  serverTime: number;
  timezone: string;
  device: {
    id: string;
    name: string;
    status: string;
  };
  attendanceConfig: {
    id: string;
    workStartMinute: number;
    workEndMinute: number;
    offStartMinute: number;
    offEndMinute: number;
    updatedAt: number;
  } | null;
  employees: Array<{
    id: string;
    code: string;
    name: string;
    updatedAt: number;
  }>;
  enrollmentTask: {
    taskId: string;
    employeeId: string;
    employeeCode: string;
    employeeName: string;
    status: string;
    createdAt: number;
    updatedAt: number;
  } | null;
};

type EnrollmentReportResponseData = {
  taskId: string;
  employeeId: string;
  status: "pending" | "success" | "failed" | "cancelled";
  applied: boolean;
};

type AttendanceUploadResponseData = {
  results: Array<{
    clientRecordId: string;
    status: "saved" | "updated_earlier" | "ignored_duplicate" | "rejected";
    attendanceRecordId: string | null;
    error: {
      code: string;
      message: string;
      retryable: boolean;
    } | null;
  }>;
  summary: {
    saved: number;
    updatedEarlier: number;
    ignoredDuplicate: number;
    rejected: number;
  };
};

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

async function readJson<T>(response: Response) {
  return (await response.json()) as T;
}

function createId(prefix: string) {
  return `${prefix}_${Date.now()}_${Math.random().toString(36).slice(2, 8)}`;
}

async function sha256Hex(value: string) {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(value));

  return Buffer.from(digest).toString("hex");
}

function localDateString(date: Date) {
  const year = date.getFullYear();
  const month = `${date.getMonth() + 1}`.padStart(2, "0");
  const day = `${date.getDate()}`.padStart(2, "0");

  return `${year}-${month}-${day}`;
}

function dateAtMinute(minute: number) {
  const date = new Date();

  date.setHours(0, 0, 0, 0);
  date.setMinutes(minute, 0, 0);

  return date;
}

async function isServerReachable(baseUrl: string) {
  try {
    const response = await fetch(baseUrl);
    return response.ok || response.status === 200 || response.status === 404;
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
          rejectPromise(new Error("Unable to determine smoke-device port"));
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

async function startServerIfNeeded(preferredBaseUrl: string) {
  if (await isServerReachable(preferredBaseUrl)) {
    return {
      baseUrl: preferredBaseUrl,
      server: null,
    };
  }

  const host = "127.0.0.1";
  const port = await findAvailablePort(host);
  const baseUrl = resolveAppOrigin({
    ...process.env,
    HOST: host,
    PORT: `${port}`,
  });
  const child = spawn("bun", ["run", "dev", "--", "--port", `${port}`, "--host", host], {
    cwd: appRoot,
    env: {
      ...process.env,
      HOST: host,
      PORT: `${port}`,
    },
    stdio: "ignore",
  });

  await waitForServer(baseUrl);

  return {
    baseUrl,
    server: child,
  };
}

async function cleanup(ids: {
  employeeId: string;
  deviceId: string;
  secondDeviceId: string;
  faceProfileId: string;
  createdTempConfig: boolean;
}) {
  await db.delete(attendanceRecord).where(eq(attendanceRecord.employeeId, ids.employeeId));
  await db.delete(faceProfile).where(eq(faceProfile.id, ids.faceProfileId));
  await db.delete(device).where(eq(device.id, ids.secondDeviceId));
  await db.delete(device).where(eq(device.id, ids.deviceId));
  await db.delete(employee).where(eq(employee.id, ids.employeeId));

  if (ids.createdTempConfig) {
    await db.delete(attendanceConfig).where(eq(attendanceConfig.id, "default"));
  }
}

const preferredBaseUrl = resolveAppOrigin(process.env);
const { baseUrl, server } = await startServerIfNeeded(preferredBaseUrl);

const employeeId = createId("smoke_emp");
const deviceId = createId("smoke_dev");
const secondDeviceId = createId("smoke_dev");
const faceProfileId = createId("smoke_fp");
const smokeSuffix = Date.now().toString(36);
const deviceCode = `DEV-SMOKE-A-${smokeSuffix}`;
const secondDeviceCode = `DEV-SMOKE-B-${smokeSuffix}`;
const apiKey = `smoke_api_key_a_${smokeSuffix}`;
const secondApiKey = `smoke_api_key_b_${smokeSuffix}`;

let createdTempConfig = false;

try {
  let configRow =
    (await db.query.attendanceConfig.findFirst({
      where: eq(attendanceConfig.id, "default"),
    })) ?? null;

  if (!configRow) {
    createdTempConfig = true;

    await db.insert(attendanceConfig).values({
      id: "default",
      workStartMinute: 540,
      workEndMinute: 600,
      offStartMinute: 1080,
      offEndMinute: 1140,
    });

    configRow =
      (await db.query.attendanceConfig.findFirst({
        where: eq(attendanceConfig.id, "default"),
      })) ?? null;
  }

  assert(configRow, "attendance config should exist before smoke test");

  await db.insert(employee).values({
    id: employeeId,
    code: `SMOKE${Date.now().toString().slice(-6)}`,
    name: "Smoke Employee",
  });

  await db.insert(device).values({
    id: deviceId,
    deviceCode,
    name: "Smoke Device",
    apiKeyHash: await sha256Hex(apiKey),
    status: "active",
  });

  await db.insert(device).values({
    id: secondDeviceId,
    deviceCode: secondDeviceCode,
    name: "Smoke Device Reassigned",
    apiKeyHash: await sha256Hex(secondApiKey),
    status: "active",
  });

  await db.insert(faceProfile).values({
    id: faceProfileId,
    employeeId,
    deviceId,
    status: "pending",
  });

  const syncResponse = await fetch(`${baseUrl}/api/device/sync`, {
    method: "POST",
    headers: {
      "content-type": "application/json",
    },
    body: JSON.stringify({
      deviceCode,
      apiKey,
    }),
  });
  const syncJson = await readJson<DeviceResponse<SyncResponseData>>(syncResponse);

  assert(syncJson.success === true, "device sync should succeed");
  assert(syncJson.data.device.id === deviceId, "device sync should return current device");
  assert(
    syncJson.data.employees.some((item: { id: string }) => item.id === employeeId),
    "device sync should include smoke employee",
  );
  assert(syncJson.data.enrollmentTask?.taskId === faceProfileId, "device sync should include pending task");

  await db
    .update(faceProfile)
    .set({
      deviceId: secondDeviceId,
      status: "pending",
    })
    .where(eq(faceProfile.id, faceProfileId));

  const staleEnrollmentResponse = await fetch(`${baseUrl}/api/device/enrollment/report`, {
    method: "POST",
    headers: {
      "content-type": "application/json",
    },
    body: JSON.stringify({
      deviceCode,
      apiKey,
      taskId: faceProfileId,
      employeeId,
      result: "success",
      finishedAt: Date.now(),
      failureReason: null,
    }),
  });
  const staleEnrollmentJson = await readJson<DeviceResponse<EnrollmentReportResponseData>>(staleEnrollmentResponse);

  assert(staleEnrollmentJson.success === false, "stale device should not be able to finish reassigned task");
  assert(
    staleEnrollmentJson.error.code === "ENROLLMENT_TASK_MISMATCH",
    "stale device should receive enrollment task mismatch",
  );

  const pendingFaceProfile =
    (await db.query.faceProfile.findFirst({
      where: eq(faceProfile.id, faceProfileId),
    })) ?? null;

  assert(pendingFaceProfile?.status === "pending", "reassigned face profile should remain pending");
  assert(pendingFaceProfile.deviceId === secondDeviceId, "reassigned face profile should stay on the new device");

  const secondSyncResponse = await fetch(`${baseUrl}/api/device/sync`, {
    method: "POST",
    headers: {
      "content-type": "application/json",
    },
    body: JSON.stringify({
      deviceCode: secondDeviceCode,
      apiKey: secondApiKey,
    }),
  });
  const secondSyncJson = await readJson<DeviceResponse<SyncResponseData>>(secondSyncResponse);

  assert(secondSyncJson.success === true, "second sync should succeed");
  assert(secondSyncJson.data.enrollmentTask?.taskId === faceProfileId, "reassigned device should receive pending task");

  const enrollmentResponse = await fetch(`${baseUrl}/api/device/enrollment/report`, {
    method: "POST",
    headers: {
      "content-type": "application/json",
    },
    body: JSON.stringify({
      deviceCode: secondDeviceCode,
      apiKey: secondApiKey,
      taskId: faceProfileId,
      employeeId,
      result: "success",
      finishedAt: Date.now(),
      failureReason: null,
    }),
  });
  const enrollmentJson = await readJson<DeviceResponse<EnrollmentReportResponseData>>(enrollmentResponse);

  assert(enrollmentJson.success === true, "enrollment report should succeed on current device");
  assert(enrollmentJson.data.status === "success", "enrollment status should become success");

  const updatedFaceProfile =
    (await db.query.faceProfile.findFirst({
      where: eq(faceProfile.id, faceProfileId),
    })) ?? null;

  assert(updatedFaceProfile?.status === "success", "face profile should be updated to success");

  const thirdSyncResponse = await fetch(`${baseUrl}/api/device/sync`, {
    method: "POST",
    headers: {
      "content-type": "application/json",
    },
    body: JSON.stringify({
      deviceCode: secondDeviceCode,
      apiKey: secondApiKey,
    }),
  });
  const thirdSyncJson = await readJson<DeviceResponse<SyncResponseData>>(thirdSyncResponse);

  assert(thirdSyncJson.success === true, "third sync should succeed");
  assert(thirdSyncJson.data.enrollmentTask === null, "completed task should no longer be returned");

  const windowSize = configRow.workEndMinute - configRow.workStartMinute;
  const baseMinute = windowSize > 2 ? configRow.workStartMinute + 2 : configRow.workStartMinute;
  const baseRecognizedAt = dateAtMinute(baseMinute).getTime();
  const localDate = localDateString(dateAtMinute(baseMinute));

  const firstUploadResponse = await fetch(`${baseUrl}/api/device/attendance/upload`, {
    method: "POST",
    headers: {
      "content-type": "application/json",
    },
    body: JSON.stringify({
      deviceCode,
      apiKey,
      records: [
        {
          clientRecordId: "smoke-record-1",
          employeeId,
          recognizedAt: baseRecognizedAt,
          type: "clock_in",
        },
      ],
    }),
  });
  const firstUploadJson = await readJson<DeviceResponse<AttendanceUploadResponseData>>(firstUploadResponse);

  assert(firstUploadJson.success === true, "first attendance upload should succeed");
  const firstUploadResult = firstUploadJson.data.results[0];

  assert(firstUploadResult, "first attendance upload should return one result");
  assert(firstUploadResult.status === "saved", "first attendance upload should save record");

  const savedRecord = await db.query.attendanceRecord.findFirst({
    where: and(
      eq(attendanceRecord.employeeId, employeeId),
      eq(attendanceRecord.localDate, localDate),
      eq(attendanceRecord.type, "clock_in"),
    ),
  });

  assert(savedRecord, "saved attendance record should exist in database");

  const duplicateUploadResponse = await fetch(`${baseUrl}/api/device/attendance/upload`, {
    method: "POST",
    headers: {
      "content-type": "application/json",
    },
    body: JSON.stringify({
      deviceCode,
      apiKey,
      records: [
        {
          clientRecordId: "smoke-record-2",
          employeeId,
          recognizedAt: baseRecognizedAt,
          type: "clock_in",
        },
      ],
    }),
  });
  const duplicateUploadJson = await readJson<DeviceResponse<AttendanceUploadResponseData>>(duplicateUploadResponse);

  assert(duplicateUploadJson.success === true, "duplicate attendance upload should still be handled");
  const duplicateUploadResult = duplicateUploadJson.data.results[0];

  assert(duplicateUploadResult, "duplicate attendance upload should return one result");
  assert(
    duplicateUploadResult.status === "ignored_duplicate",
    "duplicate attendance upload should be ignored",
  );

  if (baseMinute > configRow.workStartMinute) {
    const earlierRecognizedAt = dateAtMinute(baseMinute - 1).getTime();
    const earlierUploadResponse = await fetch(`${baseUrl}/api/device/attendance/upload`, {
      method: "POST",
      headers: {
        "content-type": "application/json",
      },
      body: JSON.stringify({
        deviceCode,
        apiKey,
        records: [
          {
            clientRecordId: "smoke-record-3",
            employeeId,
            recognizedAt: earlierRecognizedAt,
            type: "clock_in",
          },
        ],
      }),
    });
    const earlierUploadJson = await readJson<DeviceResponse<AttendanceUploadResponseData>>(earlierUploadResponse);

    assert(earlierUploadJson.success === true, "earlier attendance upload should be handled");
    const earlierUploadResult = earlierUploadJson.data.results[0];

    assert(earlierUploadResult, "earlier attendance upload should return one result");
    assert(
      earlierUploadResult.status === "updated_earlier",
      "earlier attendance upload should update existing record",
    );

    const updatedRecord = await db.query.attendanceRecord.findFirst({
      where: eq(attendanceRecord.id, savedRecord.id),
    });

    assert(
      updatedRecord?.recognizedAt.getTime() === earlierRecognizedAt,
      "attendance record should update to earlier recognizedAt",
    );
  }

  const offWindowSize = configRow.offEndMinute - configRow.offStartMinute;
  const offBaseMinute = offWindowSize > 2 ? configRow.offStartMinute + 2 : configRow.offStartMinute;
  const concurrentRecognizedAt = dateAtMinute(offBaseMinute).getTime();
  const concurrentResponses = await Promise.all([
    fetch(`${baseUrl}/api/device/attendance/upload`, {
      method: "POST",
      headers: {
        "content-type": "application/json",
      },
      body: JSON.stringify({
        deviceCode: secondDeviceCode,
        apiKey: secondApiKey,
        records: [
          {
            clientRecordId: "smoke-record-4a",
            employeeId,
            recognizedAt: concurrentRecognizedAt,
            type: "clock_out",
          },
        ],
      }),
    }),
    fetch(`${baseUrl}/api/device/attendance/upload`, {
      method: "POST",
      headers: {
        "content-type": "application/json",
      },
      body: JSON.stringify({
        deviceCode: secondDeviceCode,
        apiKey: secondApiKey,
        records: [
          {
            clientRecordId: "smoke-record-4b",
            employeeId,
            recognizedAt: concurrentRecognizedAt,
            type: "clock_out",
          },
        ],
      }),
    }),
  ]);
  const concurrentJsons = await Promise.all(
    concurrentResponses.map((response) => readJson<DeviceResponse<AttendanceUploadResponseData>>(response)),
  );
  const successfulConcurrentJsons: Array<{ success: true; data: AttendanceUploadResponseData }> = [];

  for (const item of concurrentJsons) {
    assert(item.success === true, "concurrent attendance uploads should still succeed");
    successfulConcurrentJsons.push(item);
  }

  const concurrentStatuses = successfulConcurrentJsons
    .map((item) => item.data.results[0]?.status)
    .sort();

  assert(
    concurrentStatuses[0] === "ignored_duplicate" && concurrentStatuses[1] === "saved",
    "concurrent duplicate uploads should resolve to saved plus ignored_duplicate",
  );

  console.log("device smoke test passed");
} finally {
  await cleanup({
    employeeId,
    deviceId,
    secondDeviceId,
    faceProfileId,
    createdTempConfig,
  });

  if (server) {
    server.kill("SIGTERM");
    await delay(500);
  }
}
