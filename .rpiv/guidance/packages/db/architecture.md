# packages/db — Drizzle ORM Database Layer (libSQL)

## Responsibility
整个 Hitomi 系统的唯一持久化层。定义完整的关系型 schema（9 张表: 4 auth + 5 business），实例化单例 Drizzle 客户端，并选择性重导出 Drizzle 运算符。所有消费者（`@hitomi/api`、`@hitomi/auth`、`apps/web`）将 `@hitomi/db` 作为数据库交互的唯一入口。

## Dependencies
- **`@libsql/client`**: Turso/LibSQL 客户端驱动（本地文件或远程 HTTP）
- **`drizzle-orm`** ^0.45.1: TypeScript ORM — schema 定义、查询构建器、关系 API
- **`@hitomi/env`**: 提供 `env.DATABASE_URL` 和 `serverEnvPath`
- **`better-auth`**: **不直接依赖** — `@hitomi/auth` 通过 `drizzleAdapter(db, { schema })` 消费本层

## Module Structure
```
packages/db/
├── src/
│   ├── index.ts              # [SINGLETON] createClient + drizzle() → export const db
│   │                         # 重导出: eq, and, gt + 所有 schema
│   ├── schema/
│   │   ├── index.ts          # [BARREL] 重导出 auth + business
│   │   ├── auth.ts           # [AUTH] user, session, account, verification (better-auth v1 协议)
│   │   └── business.ts       # [BUSINESS] employee, device, attendanceConfig, faceProfile, attendanceRecord
│   └── migrations/           # Drizzle Kit 生成的迁移 (0000_mushy_fixer.sql + meta/)
├── drizzle.config.ts         # Drizzle Kit CLI 配置 (dialect: "turso")
└── package.json              # 子路径导出: "." 和 "./schema/auth"
```

## sqliteTable 定义模式 (canonical)

```typescript
import { sql } from "drizzle-orm";
import { sqliteTable, text, integer, uniqueIndex, index } from "drizzle-orm/sqlite-core";

const timestampNow = sql`(cast(unixepoch('subsecond') * 1000 as integer))`;

export const employee = sqliteTable("employee", {
  id: text("id").primaryKey(),                                        // 文本 PK (UUID), 不自增
  code: text("code").notNull(),
  name: text("name").notNull(),
  createdAt: integer("created_at", { mode: "timestamp_ms" })           // 整数 → JS Date
    .default(timestampNow).notNull(),
  updatedAt: integer("updated_at", { mode: "timestamp_ms" })
    .default(timestampNow).$onUpdate(() => new Date()).notNull(),
}, (table) => [
  uniqueIndex("employee_code_unique_idx").on(table.code),              // 唯一索引
]);

// 枚举: as const 元组 + $type<>()
export const deviceStatuses = ["active", "disabled"] as const;
export type DeviceStatus = (typeof deviceStatuses)[number];

export const device = sqliteTable("device", {
  status: text("status").$type<DeviceStatus>().default("active").notNull(),
  // ...
}, (table) => [
  uniqueIndex("device_code_unique_idx").on(table.deviceCode),
  uniqueIndex("device_bootstrap_serial_unique_idx").on(table.bootstrapSerial),
  index("device_activation_status_idx").on(table.activationStatus),
]);

// 外键
export const faceProfile = sqliteTable("face_profile", {
  employeeId: text("employee_id").notNull()
    .references(() => employee.id, { onDelete: "restrict" }),         // RESTRICT: 禁止静默删除业务数据
  deviceId: text("device_id").notNull()
    .references(() => device.id, { onDelete: "restrict" }),
});
// auth 表使用 CASCADE: session.userId → onDelete: "cascade" (auth 基础设施可以级联删除)
```

## 关系定义
```typescript
export const employeeRelations = relations(employee, ({ one, many }) => ({
  faceProfile: one(faceProfile, { fields: [employee.id], references: [faceProfile.employeeId] }),
  attendanceRecords: many(attendanceRecord),
}));
export const faceProfileRelations = relations(faceProfile, ({ one }) => ({
  employee: one(employee, { fields: [faceProfile.employeeId], references: [employee.id] }),
  device: one(device, { fields: [faceProfile.deviceId], references: [device.id] }),
}));
// 命名: {tableName}Relations。关系仅用于 ORM 查询构建器，不影响 SQL schema
```

## 关键约束
- **face_profile: 每人一条** — `uniqueIndex("face_profile_employee_unique_idx").on(table.employeeId)`
- **attendance_record: 每人每天每种类型一条** — 复合唯一索引 `(employeeId, localDate, type)`
- **所有时间戳**: `mode: "timestamp_ms"` — 以整数毫秒存储，作为 JS Date 对象读取
- **$type<>()**: TypeScript 类型窄化，**不创建** SQL CHECK 约束 (SQLite 无原生枚举)

## better-auth 集成
`@hitomi/auth` 通过 `import * as schema from "@hitomi/db/schema/auth"` 仅消费 auth 表，然后调用 `drizzleAdapter(db, { provider: "sqlite", schema })` 。Auth schema 匹配 better-auth v1 协议（列名如 `emailVerified`, `expiresAt`, `ipAddress`, `userAgent` 等）。

## 迁移工作流
1. 修改 `src/schema/*.ts`
2. `bun run db:generate` → Drizzle Kit 对比 snapshot 生成新迁移 SQL
3. `bun run db:migrate` → 应用到数据库
4. 提交迁移 SQL + 更新后的 `meta/` 文件

<important if="you are adding a new table to the business schema">
## 新增业务表
1. 在 `src/schema/business.ts` 中（或新建 domain 文件 + 在 `schema/index.ts` 中加 barrel export）
2. 遵循上方 sqliteTable 模式: text PK, 时间戳列, 可选索引
3. 如有 FK，使用 `onDelete: "restrict"`（业务数据）或 `onDelete: "cascade"`（auth 基础设施）
4. 添加 `relations()` 同时在父表和子表两侧
5. 运行 `bun run db:generate` → `bun run db:migrate`
</important>
