import { db } from "@hitomi/db";
import { attendanceRecord, attendanceRecordTypes } from "@hitomi/db/schema/business";
import { and, desc, eq } from "drizzle-orm";
import { z } from "zod";

import { protectedProcedure } from "../index";
import {
  createPageResult,
  getSingleAttendanceConfig,
  normalizePageInput,
  serializeAttendanceRecordSummary,
  toLocalDate,
  toMinuteOfDay,
} from "../lib/utils";

const listInput = z.object({
  page: z.number().int().min(1).optional(),
  pageSize: z.number().int().min(1).max(100).optional(),
  localDate: z
    .string()
    .regex(/^\d{4}-\d{2}-\d{2}$/)
    .optional(),
  employeeId: z.string().min(1).optional(),
  deviceId: z.string().min(1).optional(),
  type: z.enum(attendanceRecordTypes).optional(),
  slotStatus: z.enum(["recorded", "missing"]).optional(),
});

function serializeAttendancePoint(record: Parameters<typeof serializeAttendanceRecordSummary>[0]) {
  const {
    employee: _employee,
    employeeId: _employeeId,
    localDate: _localDate,
    ...item
  } = serializeAttendanceRecordSummary(record);

  return item;
}

function getEmptySlotStatus(localDate: string, endMinute: number, nowMs: number) {
  const today = toLocalDate(nowMs);
  if (localDate > today) return null;
  if (localDate < today) return "missing";

  return toMinuteOfDay(nowMs) >= endMinute ? "missing" : null;
}

export const attendanceRecordRouter = {
  list: protectedProcedure.input(listInput).handler(async ({ input }) => {
    const pageInput = normalizePageInput(input);
    const where = and(
      input.localDate ? eq(attendanceRecord.localDate, input.localDate) : undefined,
      input.employeeId ? eq(attendanceRecord.employeeId, input.employeeId) : undefined,
      input.deviceId ? eq(attendanceRecord.deviceId, input.deviceId) : undefined,
    );

    const [records, config] = await Promise.all([
      db.query.attendanceRecord.findMany({
        with: {
          employee: true,
          device: true,
        },
        where,
        orderBy: desc(attendanceRecord.recognizedAt),
      }),
      getSingleAttendanceConfig(db),
    ]);

    const grouped = new Map<
      string,
      {
        id: string;
        employeeId: string;
        localDate: string;
        latestRecognizedAt: number;
        employee: (typeof records)[number]["employee"];
        clockIn: ReturnType<typeof serializeAttendancePoint> | null;
        clockOut: ReturnType<typeof serializeAttendancePoint> | null;
        clockInStatus: "recorded" | "missing" | null;
        clockOutStatus: "recorded" | "missing" | null;
      }
    >();

    for (const record of records) {
      const key = `${record.employeeId}:${record.localDate}`;
      const item = serializeAttendancePoint(record);
      let group = grouped.get(key);

      if (!group) {
        group = {
          id: key,
          employeeId: record.employeeId,
          localDate: record.localDate,
          latestRecognizedAt: item.recognizedAt,
          employee: record.employee,
          clockIn: null,
          clockOut: null,
          clockInStatus: null,
          clockOutStatus: null,
        };
        grouped.set(key, group);
      }

      group.latestRecognizedAt = Math.max(group.latestRecognizedAt, item.recognizedAt);
      if (record.type === "clock_in") {
        group.clockIn = item;
      } else {
        group.clockOut = item;
      }
    }

    const nowMs = Date.now();
    for (const item of grouped.values()) {
      item.clockInStatus = item.clockIn
        ? "recorded"
        : config
          ? getEmptySlotStatus(item.localDate, config.workEndMinute, nowMs)
          : null;
      item.clockOutStatus = item.clockOut
        ? "recorded"
        : config
          ? getEmptySlotStatus(item.localDate, config.offEndMinute, nowMs)
          : null;
    }

    const dailyItems = Array.from(grouped.values())
      .filter((item) => {
        if (!input.slotStatus) return true;
        if (input.type === "clock_in") return item.clockInStatus === input.slotStatus;
        if (input.type === "clock_out") return item.clockOutStatus === input.slotStatus;

        return item.clockInStatus === input.slotStatus || item.clockOutStatus === input.slotStatus;
      })
      .sort((left, right) => {
        const dateOrder = right.localDate.localeCompare(left.localDate);
        if (dateOrder !== 0) return dateOrder;
        return right.latestRecognizedAt - left.latestRecognizedAt;
      });

    const offset = (pageInput.page - 1) * pageInput.pageSize;

    return createPageResult(
      dailyItems.slice(offset, offset + pageInput.pageSize),
      dailyItems.length,
      pageInput,
    );
  }),
};
