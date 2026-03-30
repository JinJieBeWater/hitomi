import { db } from "@hitomi/db";
import { attendanceConfig } from "@hitomi/db/schema/business";
import { eq } from "drizzle-orm";
import { z } from "zod";

import { protectedProcedure } from "../index";
import { adminError } from "../lib/errors";
import {
  DEFAULT_ATTENDANCE_CONFIG_ID,
  assertMinuteValue,
  getSingleAttendanceConfig,
  isAttendanceConfigOverlapped,
  isAttendanceConfigShapeValid,
  serializeAttendanceConfig,
} from "../lib/utils";

const saveInput = z.object({
  workStartMinute: z.number().int(),
  workEndMinute: z.number().int(),
  offStartMinute: z.number().int(),
  offEndMinute: z.number().int(),
});

export const attendanceConfigRouter = {
  get: protectedProcedure.handler(async () => {
    const config = await getSingleAttendanceConfig(db);

    return {
      config: serializeAttendanceConfig(config),
    };
  }),
  save: protectedProcedure.input(saveInput).handler(async ({ input }) => {
    if (
      !assertMinuteValue(input.workStartMinute) ||
      !assertMinuteValue(input.workEndMinute) ||
      !assertMinuteValue(input.offStartMinute) ||
      !assertMinuteValue(input.offEndMinute)
    ) {
      throw adminError(
        "BAD_REQUEST",
        "ATTENDANCE_CONFIG_INVALID_MINUTE",
        "Attendance config minute must be between 0 and 1439",
      );
    }

    if (!isAttendanceConfigShapeValid(input)) {
      throw adminError(
        "BAD_REQUEST",
        "ATTENDANCE_CONFIG_INVALID_RANGE",
        "Attendance config start minute must be earlier than end minute",
      );
    }

    if (isAttendanceConfigOverlapped(input)) {
      throw adminError(
        "BAD_REQUEST",
        "ATTENDANCE_CONFIG_OVERLAPPED",
        "Attendance config time ranges must not overlap",
      );
    }

    const current = await getSingleAttendanceConfig(db);

    if (!current) {
      await db.insert(attendanceConfig).values({
        id: DEFAULT_ATTENDANCE_CONFIG_ID,
        ...input,
      });
    } else {
      await db
        .update(attendanceConfig)
        .set(input)
        .where(eq(attendanceConfig.id, DEFAULT_ATTENDANCE_CONFIG_ID));
    }

    const updated = await getSingleAttendanceConfig(db);

    return {
      config: serializeAttendanceConfig(updated),
    };
  }),
};
