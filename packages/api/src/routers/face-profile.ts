import { db } from "@hitomi/db";
import { employee, faceProfile, faceProfileStatuses } from "@hitomi/db/schema/business";
import { and, count, desc, eq } from "drizzle-orm";
import { z } from "zod";

import { protectedProcedure } from "../index";
import { adminError } from "../lib/errors";
import {
  createId,
  createPageResult,
  getDevice,
  getFaceProfile,
  normalizePageInput,
  serializeFaceProfileSummary,
} from "../lib/utils";

const listInput = z.object({
  page: z.number().int().min(1).optional(),
  pageSize: z.number().int().min(1).max(100).optional(),
  status: z.enum(faceProfileStatuses).optional(),
  employeeId: z.string().min(1).optional(),
  deviceId: z.string().min(1).optional(),
});

export const faceProfileRouter = {
  list: protectedProcedure.input(listInput).handler(async ({ input }) => {
    const pageInput = normalizePageInput(input);
    const where = and(
      input.status ? eq(faceProfile.status, input.status) : undefined,
      input.employeeId ? eq(faceProfile.employeeId, input.employeeId) : undefined,
      input.deviceId ? eq(faceProfile.deviceId, input.deviceId) : undefined,
    );

    const [items, totalRow] = await Promise.all([
      db.query.faceProfile.findMany({
        with: {
          employee: true,
          device: true,
        },
        where,
        orderBy: desc(faceProfile.updatedAt),
        limit: pageInput.pageSize,
        offset: (pageInput.page - 1) * pageInput.pageSize,
      }),
      db.select({ value: count() }).from(faceProfile).where(where).get(),
    ]);

    return createPageResult(items.map(serializeFaceProfileSummary), totalRow?.value ?? 0, pageInput);
  }),
  enqueue: protectedProcedure
    .input(
      z.object({
        employeeId: z.string().min(1),
        deviceId: z.string().min(1),
      }),
    )
    .handler(async ({ input }) => {
      const targetEmployee = await db.query.employee.findFirst({
        where: eq(employee.id, input.employeeId),
      });
      const targetDevice = await getDevice(db, input.deviceId);

      if (!targetEmployee) {
        throw adminError("NOT_FOUND", "EMPLOYEE_NOT_FOUND", "Employee not found");
      }

      if (!targetDevice) {
        throw adminError("NOT_FOUND", "DEVICE_NOT_FOUND", "Device not found");
      }

      if (targetDevice.status === "disabled") {
        throw adminError("CONFLICT", "DEVICE_DISABLED", "Device is disabled");
      }

      const pendingOnDevice = await db.query.faceProfile.findFirst({
        where: and(eq(faceProfile.deviceId, input.deviceId), eq(faceProfile.status, "pending")),
      });

      if (pendingOnDevice && pendingOnDevice.employeeId !== input.employeeId) {
        throw adminError(
          "CONFLICT",
          "DEVICE_PENDING_TASK_EXISTS",
          "Device already has another pending face profile task",
        );
      }

      const current = await db.query.faceProfile.findFirst({
        where: eq(faceProfile.employeeId, input.employeeId),
      });

      if (!current) {
        await db.insert(faceProfile).values({
          id: createId("fp"),
          employeeId: input.employeeId,
          deviceId: input.deviceId,
          status: "pending",
        });
      } else {
        await db
          .update(faceProfile)
          .set({
            deviceId: input.deviceId,
            status: "pending",
          })
          .where(eq(faceProfile.id, current.id));
      }

      const updated = await db.query.faceProfile.findFirst({
        where: eq(faceProfile.employeeId, input.employeeId),
        with: {
          employee: true,
          device: true,
        },
      });

      if (!updated) {
        throw adminError("NOT_FOUND", "FACE_PROFILE_NOT_FOUND", "Face profile not found");
      }

      return serializeFaceProfileSummary(updated);
    }),
  cancel: protectedProcedure
    .input(
      z.object({
        id: z.string().min(1),
      }),
    )
    .handler(async ({ input }) => {
      const current = await getFaceProfile(db, input.id);

      if (!current) {
        throw adminError("NOT_FOUND", "FACE_PROFILE_NOT_FOUND", "Face profile not found");
      }

      if (current.status !== "pending") {
        throw adminError("CONFLICT", "FACE_PROFILE_NOT_PENDING", "Face profile is not pending");
      }

      await db
        .update(faceProfile)
        .set({
          status: "cancelled",
        })
        .where(eq(faceProfile.id, input.id));

      const updated = await getFaceProfile(db, input.id);

      if (!updated) {
        throw adminError("NOT_FOUND", "FACE_PROFILE_NOT_FOUND", "Face profile not found");
      }

      return serializeFaceProfileSummary(updated);
    }),
};
