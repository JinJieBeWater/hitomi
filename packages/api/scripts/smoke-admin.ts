import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { config } from "dotenv";

process.env.TZ = "Asia/Shanghai";

const envPath = fileURLToPath(new URL("../../../apps/web/.env", import.meta.url));
const localDbUrl = pathToFileURL(fileURLToPath(new URL("../../../local.db", import.meta.url))).href;
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

const [{ createRouterClient }, { eq }, dbModule, routerModule] = await Promise.all([
  import("@orpc/server"),
  import("drizzle-orm"),
  import("@hitomi/db"),
  import("../src/routers/index"),
]);

const { attendanceConfig, attendanceRecord, db, device, employee, faceProfile } = dbModule;
const { appRouter } = routerModule;

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

async function expectBusinessError<T>(action: () => Promise<T>, businessCode: string) {
  try {
    await action();
  } catch (error: any) {
    assert(error?.data?.businessCode === businessCode, `expected businessCode ${businessCode}`);
    return error;
  }

  throw new Error(`expected businessCode ${businessCode}`);
}

const client = createRouterClient(appRouter, {
  context: {
    auth: null,
    session: {
      user: {
        id: "smoke_admin",
        email: "smoke-admin@example.com",
        name: "Smoke Admin",
      },
    },
  } as any,
});

const attendanceRecordId = createId("smoke_ar");
const attendanceRecordId2 = createId("smoke_ar_2");

let employeeId1: string | null = null;
let employeeId2: string | null = null;
let employeeId3: string | null = null;
let deviceId1: string | null = null;
let deviceId2: string | null = null;
let deviceId3: string | null = null;
let faceProfileId: string | null = null;
let faceProfileId2: string | null = null;

const initialSummary = await client.dashboard.summary();
const initialConfig = await client.attendanceConfig.get();

try {
  const savedConfig = await client.attendanceConfig.save({
    workStartMinute: 540,
    workEndMinute: 600,
    offStartMinute: 1080,
    offEndMinute: 1140,
  });

  assert(savedConfig.config?.id === defaultAttendanceConfigId, "attendance config should be saved");
  assert(
    savedConfig.config.workStartMinute === 540,
    "attendance config should persist work start minute",
  );

  await expectBusinessError(
    () =>
      client.attendanceConfig.save({
        workStartMinute: 540,
        workEndMinute: 600,
        offStartMinute: 590,
        offEndMinute: 660,
      }),
    "ATTENDANCE_CONFIG_OVERLAPPED",
  );

  const createdEmployee1 = await client.employee.create({
    code: `ADM${Date.now().toString().slice(-6)}A`,
    name: "Admin Smoke Employee A",
  });

  employeeId1 = createdEmployee1.id;

  assert(createdEmployee1.id, "first employee should be created");
  assert(createdEmployee1.faceProfile === null, "new employee should not have face profile");

  const updatedEmployee1 = await client.employee.update({
    id: employeeId1,
    code: `${createdEmployee1.code}_UPD`,
    name: "Admin Smoke Employee A Updated",
  });

  assert(updatedEmployee1.code.endsWith("_UPD"), "employee update should update code");
  assert(
    updatedEmployee1.name === "Admin Smoke Employee A Updated",
    "employee update should update name",
  );

  await expectBusinessError(
    () =>
      client.employee.create({
        code: updatedEmployee1.code,
        name: "Admin Smoke Employee Duplicate",
      }),
    "EMPLOYEE_CODE_CONFLICT",
  );

  const createdEmployee2 = await client.employee.create({
    code: `ADM${Date.now().toString().slice(-6)}B`,
    name: "Admin Smoke Employee B",
  });

  employeeId2 = createdEmployee2.id;

  const employeeList = await client.employee.list({
    page: 1,
    pageSize: 100,
    keyword: "Smoke Employee A Updated",
    faceProfileState: "none",
  });

  assert(
    employeeList.items.some((item) => item.id === employeeId1),
    "employee list should support keyword and face status filter",
  );

  const createdDevice1 = await client.device.create({
    name: "Admin Smoke Device A",
  });
  const createdDevice2 = await client.device.create({
    name: "Admin Smoke Device B",
  });

  deviceId1 = createdDevice1.device.id;
  deviceId2 = createdDevice2.device.id;

  assert(createdDevice1.initialApiKey.length > 0, "device create should return initial api key");
  assert(createdDevice2.initialApiKey.length > 0, "device create should return initial api key");

  const updatedDevice2Disabled = await client.device.update({
    id: deviceId2,
    status: "disabled",
  });

  assert(updatedDevice2Disabled.status === "disabled", "device update should change status");

  const disabledDevices = await client.device.list({
    page: 1,
    pageSize: 100,
    status: "disabled",
  });

  assert(
    disabledDevices.items.some((item) => item.id === deviceId2),
    "device list should support status filter",
  );
  assert(
    employeeId1 && employeeId2 && deviceId1 && deviceId2,
    "created ids should exist before face profile flow",
  );

  const employeeId1Value = employeeId1;
  const employeeId2Value = employeeId2;
  const deviceId1Value = deviceId1;
  const deviceId2Value = deviceId2;

  const pendingFaceProfile = await client.faceProfile.enqueue({
    employeeId: employeeId1Value,
    deviceId: deviceId1Value,
  });

  faceProfileId = pendingFaceProfile.id;

  assert(
    pendingFaceProfile.status === "pending",
    "face profile should be pending after assignment",
  );
  assert(
    pendingFaceProfile.deviceId === deviceId1Value,
    "face profile should point to assigned device",
  );

  const secondPendingFaceProfile = await client.faceProfile.enqueue({
    employeeId: employeeId2Value,
    deviceId: deviceId1Value,
  });
  faceProfileId2 = secondPendingFaceProfile.id;

  assert(
    secondPendingFaceProfile.status === "pending",
    "second face profile should also become pending",
  );
  assert(
    secondPendingFaceProfile.deviceId === deviceId1Value,
    "second face profile should stay on the same device",
  );

  const pendingOnDevice = await client.faceProfile.list({
    page: 1,
    pageSize: 100,
    status: "pending",
    deviceId: deviceId1Value,
  });

  assert(
    pendingOnDevice.items.length === 2,
    "same device should support multiple pending face profiles",
  );

  const cancelledFaceProfile = await client.faceProfile.cancel({
    id: faceProfileId,
  });

  assert(cancelledFaceProfile.status === "cancelled", "face profile cancel should update status");

  await expectBusinessError(
    () =>
      client.faceProfile.enqueue({
        employeeId: employeeId1Value,
        deviceId: deviceId2Value,
      }),
    "DEVICE_DISABLED",
  );

  await client.device.update({
    id: deviceId2Value,
    status: "active",
    name: "Admin Smoke Device B Active",
  });

  const reassignedFaceProfile = await client.faceProfile.enqueue({
    employeeId: employeeId1Value,
    deviceId: deviceId2Value,
  });

  faceProfileId = reassignedFaceProfile.id;

  assert(
    reassignedFaceProfile.status === "pending",
    "face profile should become pending after reassign",
  );
  assert(
    reassignedFaceProfile.deviceId === deviceId2Value,
    "face profile should switch to new device",
  );

  const pendingFaceProfiles = await client.faceProfile.list({
    page: 1,
    pageSize: 100,
    status: "pending",
    employeeId: employeeId1Value,
  });

  assert(
    pendingFaceProfiles.items.some((item) => item.id === faceProfileId),
    "face profile list should support status and employee filters",
  );

  const recognizedAt = dateAtMinute(545);
  const localDate = localDateString(recognizedAt);

  await db.insert(attendanceRecord).values({
    id: attendanceRecordId,
    employeeId: employeeId1Value,
    deviceId: deviceId2Value,
    recognizedAt,
    localDate,
    type: "clock_in",
  });

  const attendanceRecords = await client.attendanceRecord.list({
    page: 1,
    pageSize: 100,
    localDate,
    employeeId: employeeId1Value,
    deviceId: deviceId2Value,
    type: "clock_in",
  });

  assert(
    attendanceRecords.items.some((item) => item.id === attendanceRecordId),
    "attendance record list should support combined filters",
  );

  const finalSummary = await client.dashboard.summary();

  assert(
    finalSummary.employeeCount === initialSummary.employeeCount + 2,
    "dashboard employee count should increase",
  );
  assert(
    finalSummary.deviceCount === initialSummary.deviceCount + 2,
    "dashboard device count should increase",
  );
  assert(
    finalSummary.todayClockInCount === initialSummary.todayClockInCount + 1,
    "dashboard clock-in count should increase",
  );

  const employeeDeleteImpact = await client.employee.getDeleteImpact({
    id: employeeId1Value,
  });

  assert(
    employeeDeleteImpact.code === updatedEmployee1.code,
    "employee delete impact should return latest employee code",
  );
  assert(
    employeeDeleteImpact.faceProfileCount === 1,
    "employee delete impact should count linked face profile",
  );
  assert(
    employeeDeleteImpact.attendanceRecordCount === 1,
    "employee delete impact should count linked attendance record",
  );

  await expectBusinessError(
    () =>
      client.employee.remove({
        id: employeeId1Value,
        confirmText: "WRONG_EMPLOYEE_CODE",
      }),
    "EMPLOYEE_DELETE_CONFIRMATION_MISMATCH",
  );

  const removedEmployee = await client.employee.remove({
    id: employeeId1Value,
    confirmText: updatedEmployee1.code,
  });

  assert(
    removedEmployee.deletedFaceProfileCount === 1,
    "employee remove should delete linked face profile",
  );
  assert(
    removedEmployee.deletedAttendanceRecordCount === 1,
    "employee remove should delete linked attendance record",
  );

  employeeId1 = null;
  faceProfileId = null;

  const deletedEmployee =
    (await db.query.employee.findFirst({
      where: eq(employee.id, employeeId1Value),
    })) ?? null;
  const employeeFaceProfiles = await db.query.faceProfile.findMany({
    where: eq(faceProfile.employeeId, employeeId1Value),
  });
  const employeeAttendanceRecords = await db.query.attendanceRecord.findMany({
    where: eq(attendanceRecord.employeeId, employeeId1Value),
  });
  const remainingDevice2 =
    (await db.query.device.findFirst({
      where: eq(device.id, deviceId2Value),
    })) ?? null;

  assert(!deletedEmployee, "employee remove should delete employee");
  assert(employeeFaceProfiles.length === 0, "employee remove should cascade face profiles");
  assert(
    employeeAttendanceRecords.length === 0,
    "employee remove should cascade attendance records",
  );
  assert(remainingDevice2, "employee remove should not delete unrelated device");

  await expectBusinessError(
    () =>
      client.employee.remove({
        id: employeeId1Value,
        confirmText: updatedEmployee1.code,
      }),
    "EMPLOYEE_NOT_FOUND",
  );

  const createdEmployee3 = await client.employee.create({
    code: `ADM${Date.now().toString().slice(-6)}C`,
    name: "Admin Smoke Employee C",
  });
  const createdDevice3 = await client.device.create({
    name: "Admin Smoke Device C",
  });
  const deviceId3Value = createdDevice3.device.id;

  employeeId3 = createdEmployee3.id;
  deviceId3 = deviceId3Value;

  const faceProfileForDeviceDelete = await client.faceProfile.enqueue({
    employeeId: employeeId3,
    deviceId: deviceId3Value,
  });

  faceProfileId2 = faceProfileForDeviceDelete.id;

  const recognizedAt2 = dateAtMinute(546);
  const localDate2 = localDateString(recognizedAt2);

  await db.insert(attendanceRecord).values({
    id: attendanceRecordId2,
    employeeId: employeeId3,
    deviceId: deviceId3Value,
    recognizedAt: recognizedAt2,
    localDate: localDate2,
    type: "clock_in",
  });

  const deviceDeleteImpact = await client.device.getDeleteImpact({
    id: deviceId3Value,
  });

  assert(
    deviceDeleteImpact.deviceCode === createdDevice3.device.deviceCode,
    "device delete impact should return device code",
  );
  assert(
    deviceDeleteImpact.faceProfileCount === 1,
    "device delete impact should count linked face profile",
  );
  assert(
    deviceDeleteImpact.attendanceRecordCount === 1,
    "device delete impact should count linked attendance record",
  );

  await expectBusinessError(
    () =>
      client.device.remove({
        id: deviceId3Value,
        confirmText: "WRONG_DEVICE_CODE",
      }),
    "DEVICE_DELETE_CONFIRMATION_MISMATCH",
  );

  const removedDevice = await client.device.remove({
    id: deviceId3Value,
    confirmText: createdDevice3.device.deviceCode,
  });

  assert(
    removedDevice.deletedFaceProfileCount === 1,
    "device remove should delete linked face profile",
  );
  assert(
    removedDevice.deletedAttendanceRecordCount === 1,
    "device remove should delete linked attendance record",
  );

  deviceId3 = null;
  faceProfileId2 = null;

  const deletedDevice =
    (await db.query.device.findFirst({
      where: eq(device.id, createdDevice3.device.id),
    })) ?? null;
  const deviceFaceProfiles = await db.query.faceProfile.findMany({
    where: eq(faceProfile.deviceId, createdDevice3.device.id),
  });
  const deviceAttendanceRecords = await db.query.attendanceRecord.findMany({
    where: eq(attendanceRecord.deviceId, createdDevice3.device.id),
  });
  const remainingEmployee3 =
    (await db.query.employee.findFirst({
      where: eq(employee.id, employeeId3),
    })) ?? null;

  assert(!deletedDevice, "device remove should delete device");
  assert(deviceFaceProfiles.length === 0, "device remove should cascade face profiles");
  assert(deviceAttendanceRecords.length === 0, "device remove should cascade attendance records");
  assert(remainingEmployee3, "device remove should not delete unrelated employee");

  await expectBusinessError(
    () =>
      client.device.remove({
        id: createdDevice3.device.id,
        confirmText: createdDevice3.device.deviceCode,
      }),
    "DEVICE_NOT_FOUND",
  );

  console.log("admin smoke test passed");
} finally {
  await db.delete(attendanceRecord).where(eq(attendanceRecord.id, attendanceRecordId));
  await db.delete(attendanceRecord).where(eq(attendanceRecord.id, attendanceRecordId2));
  if (deviceId1) {
    await db.delete(attendanceRecord).where(eq(attendanceRecord.deviceId, deviceId1));
  }
  if (deviceId2) {
    await db.delete(attendanceRecord).where(eq(attendanceRecord.deviceId, deviceId2));
  }
  if (deviceId3) {
    await db.delete(attendanceRecord).where(eq(attendanceRecord.deviceId, deviceId3));
  }

  if (faceProfileId) {
    await db.delete(faceProfile).where(eq(faceProfile.id, faceProfileId));
  }

  if (faceProfileId2) {
    await db.delete(faceProfile).where(eq(faceProfile.id, faceProfileId2));
  }
  if (deviceId1) {
    await db.delete(faceProfile).where(eq(faceProfile.deviceId, deviceId1));
  }
  if (deviceId2) {
    await db.delete(faceProfile).where(eq(faceProfile.deviceId, deviceId2));
  }
  if (deviceId3) {
    await db.delete(faceProfile).where(eq(faceProfile.deviceId, deviceId3));
  }

  if (deviceId1) {
    await db.delete(device).where(eq(device.id, deviceId1));
  }

  if (deviceId2) {
    await db.delete(device).where(eq(device.id, deviceId2));
  }

  if (deviceId3) {
    await db.delete(device).where(eq(device.id, deviceId3));
  }

  if (employeeId1) {
    await db.delete(employee).where(eq(employee.id, employeeId1));
  }

  if (employeeId2) {
    await db.delete(employee).where(eq(employee.id, employeeId2));
  }

  if (employeeId3) {
    await db.delete(employee).where(eq(employee.id, employeeId3));
  }

  if (initialConfig.config) {
    const currentConfig =
      (await db.query.attendanceConfig.findFirst({
        where: eq(attendanceConfig.id, defaultAttendanceConfigId),
      })) ?? null;

    if (!currentConfig) {
      await db.insert(attendanceConfig).values({
        id: defaultAttendanceConfigId,
        workStartMinute: initialConfig.config.workStartMinute,
        workEndMinute: initialConfig.config.workEndMinute,
        offStartMinute: initialConfig.config.offStartMinute,
        offEndMinute: initialConfig.config.offEndMinute,
      });
    } else {
      await db
        .update(attendanceConfig)
        .set({
          workStartMinute: initialConfig.config.workStartMinute,
          workEndMinute: initialConfig.config.workEndMinute,
          offStartMinute: initialConfig.config.offStartMinute,
          offEndMinute: initialConfig.config.offEndMinute,
        })
        .where(eq(attendanceConfig.id, defaultAttendanceConfigId));
    }
  } else {
    await db.delete(attendanceConfig).where(eq(attendanceConfig.id, defaultAttendanceConfigId));
  }
}
