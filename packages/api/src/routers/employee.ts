import { attendanceRecord, db, employee, faceProfile } from "@hitomi/db";
import { faceProfileStatuses } from "@hitomi/db/schema/business";
import { and, count, desc, eq, inArray, isNull, like, or } from "drizzle-orm";
import { z } from "zod";

import { protectedProcedure } from "../index";
import { adminError } from "../lib/errors";
import {
  createId,
  createPageResult,
  getEmployeeWithFaceProfile,
  normalizePageInput,
  serializeEmployeeSummary,
} from "../lib/utils";

const listInput = z.object({
  page: z.number().int().min(1).optional(),
  pageSize: z.number().int().min(1).max(100).optional(),
  keyword: z.string().trim().optional(),
  faceProfileState: z.enum([...faceProfileStatuses, "none"]).optional(),
});

const employeeInput = z.object({
  code: z.string().trim().min(1),
  name: z.string().trim().min(1),
});

const employeeIdInput = z.object({
  id: z.string().min(1),
});

const employeeDeleteInput = employeeIdInput.extend({
  confirmText: z.string().trim().min(1),
});

export const employeeRouter = {
  list: protectedProcedure.input(listInput).handler(async ({ input }) => {
    const pageInput = normalizePageInput(input);
    const where = and(
      input.keyword ? or(like(employee.code, `%${input.keyword}%`), like(employee.name, `%${input.keyword}%`)) : undefined,
      input.faceProfileState === "none"
        ? isNull(faceProfile.id)
        : input.faceProfileState
          ? eq(faceProfile.status, input.faceProfileState)
          : undefined,
    );

    const [items, totalRow] = await Promise.all([
      db
        .select({ id: employee.id })
        .from(employee)
        .leftJoin(faceProfile, eq(faceProfile.employeeId, employee.id))
        .where(where)
        .orderBy(desc(employee.createdAt))
        .limit(pageInput.pageSize)
        .offset((pageInput.page - 1) * pageInput.pageSize),
      db
        .select({ value: count() })
        .from(employee)
        .leftJoin(faceProfile, eq(faceProfile.employeeId, employee.id))
        .where(where)
        .get(),
    ]);

    const employeeIds = items.map((item) => item.id);

    if (employeeIds.length === 0) {
      return createPageResult([], totalRow?.value ?? 0, pageInput);
    }

    const records = await db.query.employee.findMany({
      where: inArray(employee.id, employeeIds),
      with: {
        faceProfile: {
          with: {
            device: true,
          },
        },
      },
    });
    const order = new Map(employeeIds.map((id, index) => [id, index]));

    records.sort((left, right) => (order.get(left.id) ?? 0) - (order.get(right.id) ?? 0));

    return createPageResult(records.map(serializeEmployeeSummary), totalRow?.value ?? 0, pageInput);
  }),
  create: protectedProcedure.input(employeeInput).handler(async ({ input }) => {
    const exists = await db.query.employee.findFirst({
      where: eq(employee.code, input.code),
    });

    if (exists) {
      throw adminError("CONFLICT", "EMPLOYEE_CODE_CONFLICT", "Employee code already exists");
    }

    const id = createId("emp");

    await db.insert(employee).values({
      id,
      code: input.code,
      name: input.name,
    });

    const created = await getEmployeeWithFaceProfile(db, id);

    if (!created) {
      throw adminError("NOT_FOUND", "EMPLOYEE_NOT_FOUND", "Employee was not found after creation");
    }

    return serializeEmployeeSummary(created);
  }),
  update: protectedProcedure
    .input(
      employeeInput.extend({
        id: z.string().min(1),
      }),
    )
    .handler(async ({ input }) => {
      const current = await db.query.employee.findFirst({
        where: eq(employee.id, input.id),
      });

      if (!current) {
        throw adminError("NOT_FOUND", "EMPLOYEE_NOT_FOUND", "Employee not found");
      }

      const conflict = await db.query.employee.findFirst({
        where: eq(employee.code, input.code),
      });

      if (conflict && conflict.id !== input.id) {
        throw adminError("CONFLICT", "EMPLOYEE_CODE_CONFLICT", "Employee code already exists");
      }

      await db
        .update(employee)
        .set({
          code: input.code,
          name: input.name,
        })
        .where(eq(employee.id, input.id));

      const updated = await getEmployeeWithFaceProfile(db, input.id);

      if (!updated) {
        throw adminError("NOT_FOUND", "EMPLOYEE_NOT_FOUND", "Employee not found");
      }

      return serializeEmployeeSummary(updated);
    }),
  getDeleteImpact: protectedProcedure.input(employeeIdInput).handler(async ({ input }) => {
    const current = await db.query.employee.findFirst({
      where: eq(employee.id, input.id),
    });

    if (!current) {
      throw adminError("NOT_FOUND", "EMPLOYEE_NOT_FOUND", "Employee not found");
    }

    const [faceProfileCountRow, attendanceRecordCountRow] = await Promise.all([
      db
        .select({ value: count() })
        .from(faceProfile)
        .where(eq(faceProfile.employeeId, input.id))
        .get(),
      db
        .select({ value: count() })
        .from(attendanceRecord)
        .where(eq(attendanceRecord.employeeId, input.id))
        .get(),
    ]);

    return {
      id: current.id,
      code: current.code,
      name: current.name,
      faceProfileCount: faceProfileCountRow?.value ?? 0,
      attendanceRecordCount: attendanceRecordCountRow?.value ?? 0,
      confirmText: current.code,
    };
  }),
  remove: protectedProcedure.input(employeeDeleteInput).handler(async ({ input }) => {
    const current = await db.query.employee.findFirst({
      where: eq(employee.id, input.id),
    });

    if (!current) {
      throw adminError("NOT_FOUND", "EMPLOYEE_NOT_FOUND", "Employee not found");
    }

    if (input.confirmText !== current.code) {
      throw adminError(
        "CONFLICT",
        "EMPLOYEE_DELETE_CONFIRMATION_MISMATCH",
        "Employee delete confirmation does not match",
      );
    }

    return await db.transaction(async (tx) => {
      const [faceProfileCountRow, attendanceRecordCountRow] = await Promise.all([
        tx
          .select({ value: count() })
          .from(faceProfile)
          .where(eq(faceProfile.employeeId, input.id))
          .get(),
        tx
          .select({ value: count() })
          .from(attendanceRecord)
          .where(eq(attendanceRecord.employeeId, input.id))
          .get(),
      ]);

      await tx.delete(attendanceRecord).where(eq(attendanceRecord.employeeId, input.id));
      await tx.delete(faceProfile).where(eq(faceProfile.employeeId, input.id));
      await tx.delete(employee).where(eq(employee.id, input.id));

      return {
        id: input.id,
        deletedFaceProfileCount: faceProfileCountRow?.value ?? 0,
        deletedAttendanceRecordCount: attendanceRecordCountRow?.value ?? 0,
      };
    });
  }),
};
