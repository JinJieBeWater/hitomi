import { relations, sql } from "drizzle-orm";
import { index, integer, sqliteTable, text, uniqueIndex } from "drizzle-orm/sqlite-core";

export const deviceStatuses = ["active", "disabled"] as const;
export type DeviceStatus = (typeof deviceStatuses)[number];

export const faceProfileStatuses = ["pending", "success", "failed", "cancelled"] as const;
export type FaceProfileStatus = (typeof faceProfileStatuses)[number];

export const attendanceRecordTypes = ["clock_in", "clock_out"] as const;
export type AttendanceRecordType = (typeof attendanceRecordTypes)[number];

const timestampNow = sql`(cast(unixepoch('subsecond') * 1000 as integer))`;

export const employee = sqliteTable(
  "employee",
  {
    id: text("id").primaryKey(),
    code: text("code").notNull(),
    name: text("name").notNull(),
    createdAt: integer("created_at", { mode: "timestamp_ms" }).default(timestampNow).notNull(),
    updatedAt: integer("updated_at", { mode: "timestamp_ms" })
      .default(timestampNow)
      .$onUpdate(() => /* @__PURE__ */ new Date())
      .notNull(),
  },
  (table) => [uniqueIndex("employee_code_unique_idx").on(table.code)],
);

export const device = sqliteTable(
  "device",
  {
    id: text("id").primaryKey(),
    deviceCode: text("device_code").notNull(),
    name: text("name").notNull(),
    apiKeyHash: text("api_key_hash").notNull(),
    status: text("status").$type<DeviceStatus>().default("active").notNull(),
    lastSeenAt: integer("last_seen_at", { mode: "timestamp_ms" }),
    createdAt: integer("created_at", { mode: "timestamp_ms" }).default(timestampNow).notNull(),
    updatedAt: integer("updated_at", { mode: "timestamp_ms" })
      .default(timestampNow)
      .$onUpdate(() => /* @__PURE__ */ new Date())
      .notNull(),
  },
  (table) => [uniqueIndex("device_code_unique_idx").on(table.deviceCode)],
);

export const attendanceConfig = sqliteTable("attendance_config", {
  id: text("id").primaryKey(),
  workStartMinute: integer("work_start_minute").notNull(),
  workEndMinute: integer("work_end_minute").notNull(),
  offStartMinute: integer("off_start_minute").notNull(),
  offEndMinute: integer("off_end_minute").notNull(),
  updatedAt: integer("updated_at", { mode: "timestamp_ms" })
    .default(timestampNow)
    .$onUpdate(() => /* @__PURE__ */ new Date())
    .notNull(),
});

export const faceProfile = sqliteTable(
  "face_profile",
  {
    id: text("id").primaryKey(),
    employeeId: text("employee_id")
      .notNull()
      .references(() => employee.id, { onDelete: "restrict" }),
    deviceId: text("device_id")
      .notNull()
      .references(() => device.id, { onDelete: "restrict" }),
    status: text("status").$type<FaceProfileStatus>().default("pending").notNull(),
    createdAt: integer("created_at", { mode: "timestamp_ms" }).default(timestampNow).notNull(),
    updatedAt: integer("updated_at", { mode: "timestamp_ms" })
      .default(timestampNow)
      .$onUpdate(() => /* @__PURE__ */ new Date())
      .notNull(),
  },
  (table) => [
    uniqueIndex("face_profile_employee_unique_idx").on(table.employeeId),
    index("face_profile_device_idx").on(table.deviceId),
    uniqueIndex("face_profile_pending_device_unique_idx")
      .on(table.deviceId)
      .where(sql`${table.status} = 'pending'`),
  ],
);

export const attendanceRecord = sqliteTable(
  "attendance_record",
  {
    id: text("id").primaryKey(),
    employeeId: text("employee_id")
      .notNull()
      .references(() => employee.id, { onDelete: "restrict" }),
    deviceId: text("device_id")
      .notNull()
      .references(() => device.id, { onDelete: "restrict" }),
    recognizedAt: integer("recognized_at", { mode: "timestamp_ms" }).notNull(),
    localDate: text("local_date").notNull(),
    type: text("type").$type<AttendanceRecordType>().notNull(),
    createdAt: integer("created_at", { mode: "timestamp_ms" }).default(timestampNow).notNull(),
    updatedAt: integer("updated_at", { mode: "timestamp_ms" })
      .default(timestampNow)
      .$onUpdate(() => /* @__PURE__ */ new Date())
      .notNull(),
  },
  (table) => [
    uniqueIndex("attendance_record_employee_date_type_unique_idx").on(
      table.employeeId,
      table.localDate,
      table.type,
    ),
    index("attendance_record_local_date_type_idx").on(table.localDate, table.type),
    index("attendance_record_device_local_date_idx").on(table.deviceId, table.localDate),
  ],
);

export const employeeRelations = relations(employee, ({ one, many }) => ({
  faceProfile: one(faceProfile, {
    fields: [employee.id],
    references: [faceProfile.employeeId],
  }),
  attendanceRecords: many(attendanceRecord),
}));

export const deviceRelations = relations(device, ({ many }) => ({
  faceProfiles: many(faceProfile),
  attendanceRecords: many(attendanceRecord),
}));

export const faceProfileRelations = relations(faceProfile, ({ one }) => ({
  employee: one(employee, {
    fields: [faceProfile.employeeId],
    references: [employee.id],
  }),
  device: one(device, {
    fields: [faceProfile.deviceId],
    references: [device.id],
  }),
}));

export const attendanceRecordRelations = relations(attendanceRecord, ({ one }) => ({
  employee: one(employee, {
    fields: [attendanceRecord.employeeId],
    references: [employee.id],
  }),
  device: one(device, {
    fields: [attendanceRecord.deviceId],
    references: [device.id],
  }),
}));
