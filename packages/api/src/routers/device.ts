import { attendanceRecord, db, device, faceProfile } from "@hitomi/db";
import { deviceActivationStatuses, deviceStatuses } from "@hitomi/db/schema/business";
import { and, count, desc, eq, like, or } from "drizzle-orm";
import { z } from "zod";

import { protectedProcedure } from "../index";
import { adminError } from "../lib/errors";
import {
  createApiKey,
  createBootstrapSecret,
  createBootstrapSerial,
  createDeviceCode,
  createId,
  createPageResult,
  getDevice,
  normalizePageInput,
  serializeDeviceSummary,
  sha256Hex,
} from "../lib/utils";

const listInput = z.object({
  page: z.number().int().min(1).optional(),
  pageSize: z.number().int().min(1).max(100).optional(),
  keyword: z.string().trim().optional(),
  status: z.enum(deviceStatuses).optional(),
});

const deviceIdInput = z.object({
  id: z.string().min(1),
});

const deviceDeleteInput = deviceIdInput.extend({
  confirmText: z.string().trim().min(1),
});

const activationIdInput = z.object({
  deviceId: z.string().min(1),
});

export const deviceRouter = {
  list: protectedProcedure.input(listInput).handler(async ({ input }) => {
    const pageInput = normalizePageInput(input);
    const where = and(
      input.status ? eq(device.status, input.status) : undefined,
      input.keyword
        ? or(like(device.name, `%${input.keyword}%`), like(device.deviceCode, `%${input.keyword}%`))
        : undefined,
    );

    const [items, totalRow] = await Promise.all([
      db.query.device.findMany({
        where,
        orderBy: desc(device.createdAt),
        limit: pageInput.pageSize,
        offset: (pageInput.page - 1) * pageInput.pageSize,
      }),
      db.select({ value: count() }).from(device).where(where).get(),
    ]);

    return createPageResult(items.map(serializeDeviceSummary), totalRow?.value ?? 0, pageInput);
  }),
  create: protectedProcedure
    .input(
      z.object({
        name: z.string().trim().min(1),
      }),
    )
    .handler(async ({ input }) => {
      const id = createId("dev");
      const deviceCode = createDeviceCode();
      const initialApiKey = createApiKey();
      const bootstrapSerial = createBootstrapSerial();
      const bootstrapSecret = createBootstrapSecret();
      const apiKeyHash = await sha256Hex(initialApiKey);
      const bootstrapSecretHash = await sha256Hex(bootstrapSecret);

      await db.insert(device).values({
        id,
        deviceCode,
        name: input.name,
        apiKeyHash,
        bootstrapSerial,
        bootstrapSecretHash,
        activationStatus: "pending",
        pendingApiKey: initialApiKey,
      });

      const created = await getDevice(db, id);

      if (!created) {
        throw adminError("NOT_FOUND", "DEVICE_NOT_FOUND", "Device was not found after creation");
      }

      return {
        device: serializeDeviceSummary(created),
        initialApiKey,
        bootstrapSerial,
        bootstrapSecret,
      };
    }),
  listPendingActivations: protectedProcedure
    .input(
      z.object({
        status: z.enum(deviceActivationStatuses).optional(),
      }),
    )
    .handler(async ({ input }) => {
      const where = input.status
        ? eq(device.activationStatus, input.status)
        : or(eq(device.activationStatus, "pending"), eq(device.activationStatus, "issued"));

      const items = await db.query.device.findMany({
        where,
        orderBy: desc(device.updatedAt),
      });

      return items.map((item) => ({
        deviceId: item.id,
        deviceName: item.name,
        deviceCode: item.deviceCode,
        bootstrapSerial: item.bootstrapSerial,
        status: item.activationStatus,
        lastHelloAt: item.lastHelloAt?.getTime() ?? null,
        activatedAt: item.activatedAt?.getTime() ?? null,
        updatedAt: item.updatedAt.getTime(),
      }));
    }),
  issueActivation: protectedProcedure.input(activationIdInput).handler(async ({ input }) => {
    const current = await db.query.device.findFirst({
      where: eq(device.id, input.deviceId),
    });

    if (!current) {
      throw adminError("NOT_FOUND", "DEVICE_NOT_FOUND", "Device activation was not found");
    }
    if (!current.bootstrapSerial || !current.bootstrapSecretHash) {
      throw adminError(
        "CONFLICT",
        "DEVICE_BOOTSTRAP_NOT_CONFIGURED",
        "Device bootstrap identity is not configured",
      );
    }
    if (current.status === "disabled") {
      throw adminError(
        "CONFLICT",
        "DEVICE_DISABLED",
        "Disabled device cannot be issued activation credentials",
      );
    }

    const nextApiKey = current.pendingApiKey ?? createApiKey();
    const apiKeyHash = await sha256Hex(nextApiKey);

    await db
      .update(device)
      .set({
        apiKeyHash,
        activationStatus: "issued",
        pendingApiKey: nextApiKey,
        // Preserve activatedAt for re-issued devices so history is not lost
        activatedAt: current.activationStatus === "activated" ? current.activatedAt : null,
      })
      .where(eq(device.id, input.deviceId));

    return {
      deviceId: input.deviceId,
      bootstrapSerial: current.bootstrapSerial,
      status: "issued" as const,
    };
  }),
  update: protectedProcedure
    .input(
      z
        .object({
          id: z.string().min(1),
          name: z.string().trim().min(1).optional(),
          status: z.enum(deviceStatuses).optional(),
        })
        .refine((value) => value.name !== undefined || value.status !== undefined, {
          message: "name or status is required",
        }),
    )
    .handler(async ({ input }) => {
      const current = await getDevice(db, input.id);

      if (!current) {
        throw adminError("NOT_FOUND", "DEVICE_NOT_FOUND", "Device not found");
      }

      await db
        .update(device)
        .set({
          name: input.name ?? current.name,
          status: input.status ?? current.status,
        })
        .where(eq(device.id, input.id));

      const updated = await getDevice(db, input.id);

      if (!updated) {
        throw adminError("NOT_FOUND", "DEVICE_NOT_FOUND", "Device not found");
      }

      return serializeDeviceSummary(updated);
    }),
  getDeleteImpact: protectedProcedure.input(deviceIdInput).handler(async ({ input }) => {
    const current = await getDevice(db, input.id);

    if (!current) {
      throw adminError("NOT_FOUND", "DEVICE_NOT_FOUND", "Device not found");
    }

    const [faceProfileCountRow, attendanceRecordCountRow] = await Promise.all([
      db
        .select({ value: count() })
        .from(faceProfile)
        .where(eq(faceProfile.deviceId, input.id))
        .get(),
      db
        .select({ value: count() })
        .from(attendanceRecord)
        .where(eq(attendanceRecord.deviceId, input.id))
        .get(),
    ]);

    return {
      id: current.id,
      deviceCode: current.deviceCode,
      name: current.name,
      faceProfileCount: faceProfileCountRow?.value ?? 0,
      attendanceRecordCount: attendanceRecordCountRow?.value ?? 0,
      confirmText: current.deviceCode,
    };
  }),
  remove: protectedProcedure.input(deviceDeleteInput).handler(async ({ input }) => {
    const current = await getDevice(db, input.id);

    if (!current) {
      throw adminError("NOT_FOUND", "DEVICE_NOT_FOUND", "Device not found");
    }

    if (input.confirmText !== current.deviceCode) {
      throw adminError(
        "CONFLICT",
        "DEVICE_DELETE_CONFIRMATION_MISMATCH",
        "Device delete confirmation does not match",
      );
    }

    return await db.transaction(async (tx) => {
      const [faceProfileCountRow, attendanceRecordCountRow] = await Promise.all([
        tx
          .select({ value: count() })
          .from(faceProfile)
          .where(eq(faceProfile.deviceId, input.id))
          .get(),
        tx
          .select({ value: count() })
          .from(attendanceRecord)
          .where(eq(attendanceRecord.deviceId, input.id))
          .get(),
      ]);

      await tx.delete(attendanceRecord).where(eq(attendanceRecord.deviceId, input.id));
      await tx.delete(faceProfile).where(eq(faceProfile.deviceId, input.id));
      await tx.delete(device).where(eq(device.id, input.id));

      return {
        id: input.id,
        deletedFaceProfileCount: faceProfileCountRow?.value ?? 0,
        deletedAttendanceRecordCount: attendanceRecordCountRow?.value ?? 0,
      };
    });
  }),
};
