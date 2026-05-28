import { db } from "@hitomi/db";
import { attendanceRecord, attendanceRecordTypes, employee } from "@hitomi/db/schema/business";
import { and, asc, desc, eq, gte, lt } from "drizzle-orm";
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

const monthlySummaryInput = z.object({
  month: z.string().regex(/^\d{4}-(0[1-9]|1[0-2])$/),
  employeeId: z.string().min(1).optional(),
});

function getNextMonth(month: string) {
  const [yearPart, monthPart] = month.split("-");
  const year = Number(yearPart);
  const monthNumber = Number(monthPart);

  if (monthNumber === 12) return `${year + 1}-01`;

  return `${year}-${String(monthNumber + 1).padStart(2, "0")}`;
}

function getMonthRange(month: string) {
  return {
    start: `${month}-01`,
    end: `${getNextMonth(month)}-01`,
  };
}

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

  monthlySummary: protectedProcedure.input(monthlySummaryInput).handler(async ({ input }) => {
    const range = getMonthRange(input.month);
    const [employees, records, config] = await Promise.all([
      db.query.employee.findMany({
        where: input.employeeId ? eq(employee.id, input.employeeId) : undefined,
        orderBy: asc(employee.code),
      }),
      db.query.attendanceRecord.findMany({
        with: {
          employee: true,
        },
        where: and(
          gte(attendanceRecord.localDate, range.start),
          lt(attendanceRecord.localDate, range.end),
          input.employeeId ? eq(attendanceRecord.employeeId, input.employeeId) : undefined,
        ),
        orderBy: [asc(attendanceRecord.localDate), asc(attendanceRecord.employeeId)],
      }),
      getSingleAttendanceConfig(db),
    ]);

    const itemMap = new Map(
      employees.map((item) => [
        item.id,
        {
          employee: {
            id: item.id,
            code: item.code,
            name: item.name,
          },
          activeDays: 0,
          clockInCount: 0,
          clockOutCount: 0,
          missingClockInCount: 0,
          missingClockOutCount: 0,
          days: [] as Array<{
            localDate: string;
            clockInStatus: "recorded" | "missing" | null;
            clockOutStatus: "recorded" | "missing" | null;
          }>,
        },
      ]),
    );

    const dayMap = new Map<
      string,
      {
        employeeId: string;
        localDate: string;
        clockInStatus: "recorded" | "missing" | null;
        clockOutStatus: "recorded" | "missing" | null;
      }
    >();

    for (const record of records) {
      const key = `${record.employeeId}:${record.localDate}`;
      let item = dayMap.get(key);

      if (!item) {
        item = {
          employeeId: record.employeeId,
          localDate: record.localDate,
          clockInStatus: null,
          clockOutStatus: null,
        };
        dayMap.set(key, item);
      }

      if (record.type === "clock_in") {
        item.clockInStatus = "recorded";
      } else {
        item.clockOutStatus = "recorded";
      }
    }

    const nowMs = Date.now();
    for (const item of dayMap.values()) {
      if (!itemMap.has(item.employeeId)) continue;

      item.clockInStatus = item.clockInStatus
        ? "recorded"
        : config
          ? getEmptySlotStatus(item.localDate, config.workEndMinute, nowMs)
          : null;
      item.clockOutStatus = item.clockOutStatus
        ? "recorded"
        : config
          ? getEmptySlotStatus(item.localDate, config.offEndMinute, nowMs)
          : null;

      const summary = itemMap.get(item.employeeId);
      if (!summary) continue;

      summary.activeDays += 1;
      if (item.clockInStatus === "recorded") summary.clockInCount += 1;
      if (item.clockOutStatus === "recorded") summary.clockOutCount += 1;
      if (item.clockInStatus === "missing") summary.missingClockInCount += 1;
      if (item.clockOutStatus === "missing") summary.missingClockOutCount += 1;
      summary.days.push({
        localDate: item.localDate,
        clockInStatus: item.clockInStatus,
        clockOutStatus: item.clockOutStatus,
      });
    }

    const items = Array.from(itemMap.values()).map((item) => ({
      ...item,
      days: item.days.sort((left, right) => left.localDate.localeCompare(right.localDate)),
    }));

    return {
      month: input.month,
      range,
      items,
      totals: {
        employeeCount: items.length,
        activeDays: items.reduce((sum, item) => sum + item.activeDays, 0),
        clockInCount: items.reduce((sum, item) => sum + item.clockInCount, 0),
        clockOutCount: items.reduce((sum, item) => sum + item.clockOutCount, 0),
        missingClockInCount: items.reduce((sum, item) => sum + item.missingClockInCount, 0),
        missingClockOutCount: items.reduce((sum, item) => sum + item.missingClockOutCount, 0),
      },
    };
  }),
};
