import { and, attendanceRecord, db, eq, gt } from "@hitomi/db";
import { defineEventHandler } from "h3";

import {
  attendanceUploadBodySchema,
  authenticateDevice,
  createId,
  deviceFailure,
  deviceInternalError,
  deviceSuccess,
  getAttendanceConfig,
  getAttendanceRecord,
  getEmployee,
  isAttendanceValid,
  parseDeviceBody,
  toLocalDate,
} from "../../../utils/device";

export default defineEventHandler(async (event) => {
  try {
    const parsed = await parseDeviceBody(event, attendanceUploadBodySchema);

    if (!parsed.ok) {
      return parsed.response;
    }

    const clientRecordIds = new Set<string>();

    for (const record of parsed.data.records) {
      if (clientRecordIds.has(record.clientRecordId)) {
        return deviceFailure("INVALID_REQUEST", "clientRecordId must be unique in current request");
      }

      clientRecordIds.add(record.clientRecordId);
    }

    const auth = await authenticateDevice(parsed.data.deviceCode, parsed.data.apiKey);

    if (!auth.ok) {
      return auth.response;
    }

    const config = await getAttendanceConfig();
    const employeeCache = new Map<string, Awaited<ReturnType<typeof getEmployee>>>();
    const results: Array<{
      clientRecordId: string;
      status: "saved" | "updated_earlier" | "ignored_duplicate" | "rejected";
      attendanceRecordId: string | null;
      error: null | {
        code:
          | "EMPLOYEE_NOT_FOUND"
          | "ATTENDANCE_CONFIG_MISSING"
          | "ATTENDANCE_NOT_IN_WINDOW"
          | "ATTENDANCE_DUPLICATE_LATER_OR_EQUAL"
          | "INVALID_REQUEST";
        message: string;
        retryable: boolean;
      };
    }> = [];

    for (const record of parsed.data.records) {
      let currentEmployee = employeeCache.get(record.employeeId);

      if (!currentEmployee) {
        currentEmployee = await getEmployee(record.employeeId);
        employeeCache.set(record.employeeId, currentEmployee);
      }

      if (!currentEmployee) {
        results.push({
          clientRecordId: record.clientRecordId,
          status: "rejected",
          attendanceRecordId: null,
          error: {
            code: "EMPLOYEE_NOT_FOUND",
            message: "employee not found",
            retryable: false,
          },
        });
        continue;
      }

      if (!config) {
        results.push({
          clientRecordId: record.clientRecordId,
          status: "rejected",
          attendanceRecordId: null,
          error: {
            code: "ATTENDANCE_CONFIG_MISSING",
            message: "attendance config is missing",
            retryable: false,
          },
        });
        continue;
      }

      if (!isAttendanceValid(record.recognizedAt, record.type, config)) {
        results.push({
          clientRecordId: record.clientRecordId,
          status: "rejected",
          attendanceRecordId: null,
          error: {
            code: "ATTENDANCE_NOT_IN_WINDOW",
            message: "recognizedAt is not in valid attendance window",
            retryable: false,
          },
        });
        continue;
      }

      const localDate = toLocalDate(record.recognizedAt);
      const id = createId("ar");
      const recognizedAt = new Date(record.recognizedAt);
      const insertResult = await db
        .insert(attendanceRecord)
        .values({
          id,
          employeeId: record.employeeId,
          deviceId: auth.device.id,
          recognizedAt,
          localDate,
          type: record.type,
        })
        .onConflictDoNothing({
          target: [attendanceRecord.employeeId, attendanceRecord.localDate, attendanceRecord.type],
        });

      if (insertResult.rowsAffected > 0) {
        results.push({
          clientRecordId: record.clientRecordId,
          status: "saved",
          attendanceRecordId: id,
          error: null,
        });
        continue;
      }

      const existing = await getAttendanceRecord(record.employeeId, localDate, record.type);

      if (!existing) {
        throw new Error("attendance record missing after conflict");
      }

      if (record.recognizedAt < existing.recognizedAt.getTime()) {
        const updateResult = await db
          .update(attendanceRecord)
          .set({
            deviceId: auth.device.id,
            recognizedAt,
          })
          .where(
            and(
              eq(attendanceRecord.id, existing.id),
              gt(attendanceRecord.recognizedAt, recognizedAt),
            ),
          );

        if (updateResult.rowsAffected > 0) {
          results.push({
            clientRecordId: record.clientRecordId,
            status: "updated_earlier",
            attendanceRecordId: existing.id,
            error: null,
          });
          continue;
        }
      }

      const latest = await getAttendanceRecord(record.employeeId, localDate, record.type);

      if (!latest) {
        throw new Error("attendance record missing after update");
      }

      results.push({
        clientRecordId: record.clientRecordId,
        status: "ignored_duplicate",
        attendanceRecordId: latest.id,
        error: {
          code: "ATTENDANCE_DUPLICATE_LATER_OR_EQUAL",
          message: "an earlier or equal attendance record already exists",
          retryable: false,
        },
      });
    }

    return deviceSuccess({
      results,
      summary: {
        saved: results.filter((item) => item.status === "saved").length,
        updatedEarlier: results.filter((item) => item.status === "updated_earlier").length,
        ignoredDuplicate: results.filter((item) => item.status === "ignored_duplicate").length,
        rejected: results.filter((item) => item.status === "rejected").length,
      },
    });
  } catch (error) {
    return deviceInternalError(event, error);
  }
});
