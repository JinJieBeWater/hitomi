import type { AttendanceRecordType, DeviceStatus, FaceProfileStatus } from "@hitomi/db/schema/business";
import { and, count, eq } from "drizzle-orm";

import { attendanceConfig, attendanceRecord, db, device, employee, faceProfile } from "@hitomi/db";

export const SHANGHAI_TIMEZONE = "Asia/Shanghai";
export const DEFAULT_ATTENDANCE_CONFIG_ID = "default";

export type PageInput = {
  page?: number;
  pageSize?: number;
};

export type NormalizedPageInput = {
  page: number;
  pageSize: number;
};

export function normalizePageInput(input: PageInput): NormalizedPageInput {
  const page = input.page ?? 1;
  const pageSize = input.pageSize ?? 20;

  if (!Number.isInteger(page) || page < 1) {
    throw new Error("page must be a positive integer");
  }

  if (!Number.isInteger(pageSize) || pageSize < 1 || pageSize > 100) {
    throw new Error("pageSize must be an integer between 1 and 100");
  }

  return {
    page,
    pageSize,
  };
}

export function createPageInfo(total: number, input: NormalizedPageInput) {
  return {
    page: input.page,
    pageSize: input.pageSize,
    total,
    totalPages: Math.max(1, Math.ceil(total / input.pageSize)),
  };
}

export function createPageResult<T>(items: T[], total: number, input: NormalizedPageInput) {
  return {
    items,
    pageInfo: createPageInfo(total, input),
  };
}

export function paginate<T>(items: T[], input: PageInput) {
  const { page, pageSize } = normalizePageInput(input);
  const total = items.length;
  const totalPages = Math.max(1, Math.ceil(total / pageSize));
  const offset = (page - 1) * pageSize;

  return {
    items: items.slice(offset, offset + pageSize),
    pageInfo: {
      page,
      pageSize,
      total,
      totalPages,
    },
  };
}

function getParts(value: Date | number) {
  const date = typeof value === "number" ? new Date(value) : value;
  const formatter = new Intl.DateTimeFormat("en-CA", {
    timeZone: SHANGHAI_TIMEZONE,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    hourCycle: "h23",
  });

  const parts = formatter.formatToParts(date);
  const map = Object.fromEntries(parts.map((part) => [part.type, part.value]));

  return {
    year: map.year,
    month: map.month,
    day: map.day,
    hour: map.hour,
    minute: map.minute,
  };
}

export function toTimestamp(value: Date | null | undefined) {
  return value ? value.getTime() : null;
}

export function toLocalDate(value: Date | number) {
  const parts = getParts(value);

  return `${parts.year}-${parts.month}-${parts.day}`;
}

export function toMinuteOfDay(value: Date | number) {
  const parts = getParts(value);

  return Number(parts.hour) * 60 + Number(parts.minute);
}

export function isMinuteInRange(minute: number, start: number, end: number) {
  return minute >= start && minute < end;
}

export function assertMinuteValue(minute: number) {
  return Number.isInteger(minute) && minute >= 0 && minute <= 1439;
}

export function isAttendanceConfigShapeValid(input: {
  workStartMinute: number;
  workEndMinute: number;
  offStartMinute: number;
  offEndMinute: number;
}) {
  return input.workStartMinute < input.workEndMinute && input.offStartMinute < input.offEndMinute;
}

export function isAttendanceConfigOverlapped(input: {
  workStartMinute: number;
  workEndMinute: number;
  offStartMinute: number;
  offEndMinute: number;
}) {
  return Math.max(input.workStartMinute, input.offStartMinute) < Math.min(input.workEndMinute, input.offEndMinute);
}

export function createId(prefix: string) {
  return `${prefix}_${crypto.randomUUID().replaceAll("-", "")}`;
}

export function createDeviceCode() {
  return `DEV-${crypto.randomUUID().replaceAll("-", "").slice(0, 12).toUpperCase()}`;
}

export function createApiKey() {
  return crypto.randomUUID().replaceAll("-", "") + crypto.randomUUID().replaceAll("-", "");
}

export async function sha256Hex(value: string) {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(value));

  return Buffer.from(digest).toString("hex");
}

export function serializeAttendanceConfig(config: typeof attendanceConfig.$inferSelect | null) {
  if (!config) return null;

  return {
    id: config.id,
    workStartMinute: config.workStartMinute,
    workEndMinute: config.workEndMinute,
    offStartMinute: config.offStartMinute,
    offEndMinute: config.offEndMinute,
    updatedAt: config.updatedAt.getTime(),
  };
}

export function serializeEmployeeSummary(record: {
  id: string;
  code: string;
  name: string;
  createdAt: Date;
  updatedAt: Date;
  faceProfile:
    | null
    | {
        id: string;
        status: FaceProfileStatus;
        deviceId: string;
        updatedAt: Date;
        device: { name: string } | null;
      };
}) {
  return {
    id: record.id,
    code: record.code,
    name: record.name,
    createdAt: record.createdAt.getTime(),
    updatedAt: record.updatedAt.getTime(),
    faceProfile: record.faceProfile
      ? {
          id: record.faceProfile.id,
          status: record.faceProfile.status,
          deviceId: record.faceProfile.deviceId,
          deviceName: record.faceProfile.device?.name ?? "",
          updatedAt: record.faceProfile.updatedAt.getTime(),
        }
      : null,
  };
}

export function serializeDeviceSummary(record: {
  id: string;
  deviceCode: string;
  name: string;
  status: DeviceStatus;
  lastSeenAt: Date | null;
  createdAt: Date;
  updatedAt: Date;
}) {
  return {
    id: record.id,
    deviceCode: record.deviceCode,
    name: record.name,
    status: record.status,
    lastSeenAt: toTimestamp(record.lastSeenAt),
    createdAt: record.createdAt.getTime(),
    updatedAt: record.updatedAt.getTime(),
  };
}

export function serializeFaceProfileSummary(record: {
  id: string;
  employeeId: string;
  deviceId: string;
  status: FaceProfileStatus;
  createdAt: Date;
  updatedAt: Date;
  employee: {
    id: string;
    code: string;
    name: string;
  } | null;
  device: {
    id: string;
    name: string;
    deviceCode: string;
    status: DeviceStatus;
  } | null;
}) {
  return {
    id: record.id,
    employeeId: record.employeeId,
    deviceId: record.deviceId,
    status: record.status,
    createdAt: record.createdAt.getTime(),
    updatedAt: record.updatedAt.getTime(),
    employee: record.employee,
    device: record.device,
  };
}

export function serializeAttendanceRecordSummary(record: {
  id: string;
  employeeId: string;
  deviceId: string;
  recognizedAt: Date;
  localDate: string;
  type: AttendanceRecordType;
  createdAt: Date;
  updatedAt: Date;
  employee: {
    id: string;
    code: string;
    name: string;
  } | null;
  device: {
    id: string;
    name: string;
    deviceCode: string;
  } | null;
}) {
  return {
    id: record.id,
    employeeId: record.employeeId,
    deviceId: record.deviceId,
    recognizedAt: record.recognizedAt.getTime(),
    localDate: record.localDate,
    type: record.type,
    createdAt: record.createdAt.getTime(),
    updatedAt: record.updatedAt.getTime(),
    employee: record.employee,
    device: record.device,
  };
}

export async function getDashboardSummary(database: typeof db) {
  const todayLocalDate = toLocalDate(Date.now());
  const employeeCountRow = await database.select({ value: count() }).from(employee).get();
  const deviceCountRow = await database.select({ value: count() }).from(device).get();
  const todayClockInCountRow = await database
    .select({ value: count() })
    .from(attendanceRecord)
    .where(and(eq(attendanceRecord.localDate, todayLocalDate), eq(attendanceRecord.type, "clock_in")))
    .get();
  const todayClockOutCountRow = await database
    .select({ value: count() })
    .from(attendanceRecord)
    .where(and(eq(attendanceRecord.localDate, todayLocalDate), eq(attendanceRecord.type, "clock_out")))
    .get();

  return {
    todayLocalDate,
    employeeCount: employeeCountRow?.value ?? 0,
    deviceCount: deviceCountRow?.value ?? 0,
    todayClockInCount: todayClockInCountRow?.value ?? 0,
    todayClockOutCount: todayClockOutCountRow?.value ?? 0,
  };
}

export async function getSingleAttendanceConfig(database: typeof db) {
  return (
    (await database.query.attendanceConfig.findFirst({
      where: eq(attendanceConfig.id, DEFAULT_ATTENDANCE_CONFIG_ID),
    })) ?? null
  );
}

export async function getEmployeeWithFaceProfile(database: typeof db, id: string) {
  return database.query.employee.findFirst({
    where: eq(employee.id, id),
    with: {
      faceProfile: {
        with: {
          device: true,
        },
      },
    },
  });
}

export async function getDevice(database: typeof db, id: string) {
  return database.query.device.findFirst({
    where: eq(device.id, id),
  });
}

export async function getFaceProfile(database: typeof db, id: string) {
  return database.query.faceProfile.findFirst({
    where: eq(faceProfile.id, id),
    with: {
      employee: true,
      device: true,
    },
  });
}
