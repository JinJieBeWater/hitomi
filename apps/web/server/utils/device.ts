import { attendanceConfig, attendanceRecord, db, device, employee, faceProfile } from "@hitomi/db";
import { attendanceRecordTypes } from "@hitomi/db/schema/business";
import { and, eq } from "drizzle-orm";
import type { H3Event } from "h3";
import { z } from "zod";

const timezone = "Asia/Shanghai";

export const syncBodySchema = z.object({
  deviceCode: z.string().min(1),
  apiKey: z.string().min(1),
});

export const enrollmentReportBodySchema = syncBodySchema.extend({
  taskId: z.string().min(1),
  employeeId: z.string().min(1),
  result: z.enum(["success", "failed"]),
  finishedAt: z.number().int().nonnegative(),
  failureReason: z.string().nullable().optional(),
});

export const attendanceUploadBodySchema = syncBodySchema.extend({
  records: z
    .array(
      z.object({
        clientRecordId: z.string().min(1),
        employeeId: z.string().min(1),
        recognizedAt: z.number().int().nonnegative(),
        type: z.enum(attendanceRecordTypes),
      }),
    )
    .min(1),
});

export type DeviceErrorCode =
  | "DEVICE_AUTH_FAILED"
  | "DEVICE_DISABLED"
  | "INVALID_REQUEST"
  | "ATTENDANCE_CONFIG_MISSING"
  | "EMPLOYEE_NOT_FOUND"
  | "TASK_CANCELLED"
  | "ENROLLMENT_TASK_NOT_FOUND"
  | "ENROLLMENT_TASK_MISMATCH"
  | "ATTENDANCE_NOT_IN_WINDOW"
  | "ATTENDANCE_DUPLICATE_LATER_OR_EQUAL"
  | "INTERNAL_ERROR";

export function deviceSuccess<T>(data: T) {
  return {
    success: true as const,
    data,
  };
}

export function deviceFailure(code: DeviceErrorCode, message: string, retryable = false) {
  return {
    success: false as const,
    error: {
      code,
      message,
      retryable,
    },
  };
}

export async function parseDeviceBody<T extends z.ZodTypeAny>(event: H3Event, schema: T) {
  try {
    const body = await readBody(event);
    const parsed = schema.safeParse(body);

    if (!parsed.success) {
      setResponseStatus(event, 400);

      return {
        ok: false as const,
        response: deviceFailure("INVALID_REQUEST", "Request body is invalid"),
      };
    }

    return {
      ok: true as const,
      data: parsed.data,
    };
  } catch {
    setResponseStatus(event, 400);

    return {
      ok: false as const,
      response: deviceFailure("INVALID_REQUEST", "Request body is invalid"),
    };
  }
}

async function sha256Hex(value: string) {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(value));

  return Buffer.from(digest).toString("hex");
}

export async function authenticateDevice(deviceCode: string, apiKey: string) {
  const current = await db.query.device.findFirst({
    where: eq(device.deviceCode, deviceCode),
  });

  if (!current) {
    return {
      ok: false as const,
      response: deviceFailure("DEVICE_AUTH_FAILED", "deviceCode or apiKey is invalid"),
    };
  }

  const apiKeyHash = await sha256Hex(apiKey);

  if (apiKeyHash !== current.apiKeyHash) {
    return {
      ok: false as const,
      response: deviceFailure("DEVICE_AUTH_FAILED", "deviceCode or apiKey is invalid"),
    };
  }

  const now = new Date();

  await db
    .update(device)
    .set({
      lastSeenAt: now,
    })
    .where(eq(device.id, current.id));

  if (current.status === "disabled") {
    return {
      ok: false as const,
      response: deviceFailure("DEVICE_DISABLED", "device is disabled"),
    };
  }

  return {
    ok: true as const,
    device: {
      ...current,
      lastSeenAt: now,
    },
  };
}

function getDateParts(value: number | Date) {
  const date = typeof value === "number" ? new Date(value) : value;
  const formatter = new Intl.DateTimeFormat("en-CA", {
    timeZone: timezone,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    hourCycle: "h23",
  });
  const parts = Object.fromEntries(formatter.formatToParts(date).map((part) => [part.type, part.value]));

  return {
    year: parts.year,
    month: parts.month,
    day: parts.day,
    hour: Number(parts.hour),
    minute: Number(parts.minute),
  };
}

export function toLocalDate(value: number | Date) {
  const parts = getDateParts(value);

  return `${parts.year}-${parts.month}-${parts.day}`;
}

export function toMinuteOfDay(value: number | Date) {
  const parts = getDateParts(value);

  return parts.hour * 60 + parts.minute;
}

export function isMinuteWithinRange(minute: number, start: number, end: number) {
  return minute >= start && minute < end;
}

export function isAttendanceValid(
  recognizedAt: number,
  type: "clock_in" | "clock_out",
  config: typeof attendanceConfig.$inferSelect,
) {
  const minute = toMinuteOfDay(recognizedAt);

  if (type === "clock_in") {
    return isMinuteWithinRange(minute, config.workStartMinute, config.workEndMinute);
  }

  return isMinuteWithinRange(minute, config.offStartMinute, config.offEndMinute);
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

export async function getSyncPayload(deviceId: string) {
  const [config, employees, task] = await Promise.all([
    db.query.attendanceConfig.findFirst(),
    db.query.employee.findMany(),
    db.query.faceProfile.findFirst({
      where: and(eq(faceProfile.deviceId, deviceId), eq(faceProfile.status, "pending")),
      with: {
        employee: true,
      },
    }),
  ]);

  return {
    attendanceConfig: serializeAttendanceConfig(config ?? null),
    employees: employees.map((item) => ({
      id: item.id,
      code: item.code,
      name: item.name,
      updatedAt: item.updatedAt.getTime(),
    })),
    enrollmentTask: task
      ? {
          taskId: task.id,
          employeeId: task.employeeId,
          employeeCode: task.employee?.code ?? "",
          employeeName: task.employee?.name ?? "",
          status: task.status,
          createdAt: task.createdAt.getTime(),
          updatedAt: task.updatedAt.getTime(),
        }
      : null,
  };
}

export function createId(prefix: string) {
  return `${prefix}_${crypto.randomUUID().replaceAll("-", "")}`;
}

export async function getAttendanceConfig() {
  return (await db.query.attendanceConfig.findFirst()) ?? null;
}

export async function getEmployee(id: string) {
  return (await db.query.employee.findFirst({
    where: eq(employee.id, id),
  })) ?? null;
}

export async function getFaceProfile(id: string) {
  return (await db.query.faceProfile.findFirst({
    where: eq(faceProfile.id, id),
  })) ?? null;
}

export async function getAttendanceRecord(employeeId: string, localDate: string, type: "clock_in" | "clock_out") {
  return (
    (await db.query.attendanceRecord.findFirst({
      where: and(
        eq(attendanceRecord.employeeId, employeeId),
        eq(attendanceRecord.localDate, localDate),
        eq(attendanceRecord.type, type),
      ),
    })) ?? null
  );
}

export function deviceInternalError(event: H3Event, error: unknown) {
  console.error(error);
  setResponseStatus(event, 500);

  return deviceFailure("INTERNAL_ERROR", "internal server error", true);
}
