import { db } from "@hitomi/db";
import { attendanceRecord, attendanceRecordTypes } from "@hitomi/db/schema/business";
import { and, count, desc, eq } from "drizzle-orm";
import { z } from "zod";

import { protectedProcedure } from "../index";
import { createPageResult, normalizePageInput, serializeAttendanceRecordSummary } from "../lib/utils";

const listInput = z.object({
  page: z.number().int().min(1).optional(),
  pageSize: z.number().int().min(1).max(100).optional(),
  localDate: z.string().regex(/^\d{4}-\d{2}-\d{2}$/).optional(),
  employeeId: z.string().min(1).optional(),
  deviceId: z.string().min(1).optional(),
  type: z.enum(attendanceRecordTypes).optional(),
});

export const attendanceRecordRouter = {
  list: protectedProcedure.input(listInput).handler(async ({ input }) => {
    const pageInput = normalizePageInput(input);
    const where = and(
      input.localDate ? eq(attendanceRecord.localDate, input.localDate) : undefined,
      input.employeeId ? eq(attendanceRecord.employeeId, input.employeeId) : undefined,
      input.deviceId ? eq(attendanceRecord.deviceId, input.deviceId) : undefined,
      input.type ? eq(attendanceRecord.type, input.type) : undefined,
    );

    const [items, totalRow] = await Promise.all([
      db.query.attendanceRecord.findMany({
        with: {
          employee: true,
          device: true,
        },
        where,
        orderBy: desc(attendanceRecord.recognizedAt),
        limit: pageInput.pageSize,
        offset: (pageInput.page - 1) * pageInput.pageSize,
      }),
      db.select({ value: count() }).from(attendanceRecord).where(where).get(),
    ]);

    return createPageResult(items.map(serializeAttendanceRecordSummary), totalRow?.value ?? 0, pageInput);
  }),
};
