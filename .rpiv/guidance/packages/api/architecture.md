# packages/api — oRPC Backend API Layer

## Responsibility
定义完整的后端 API 协议——每个实体（employee, device, faceProfile, attendanceConfig, attendanceRecord, dashboard）一个独立 router，使用 Zod 输入校验和 `ORPCError` 业务错误。本层**不运行独立服务**；导出 `appRouter` 对象供 `apps/web` 的 Nitro server 通过 `RPCHandler` 挂载到 `/rpc`。

## Dependencies
- **`@hitomi/auth`**: session 校验 — `auth.api.getSession({ headers })` 在 context 工厂中调用
- **`@hitomi/db`**: Drizzle ORM 客户端 (`db`) + schema 导出 (`employee`, `device`, ...)
- **`@orpc/server`**: `os`, `ORPCError`, middleware — 提供 RPC 抽象
- **`zod`**: 所有输入校验
- **无 HTTP/Web 框架依赖** — 本层是纯库

## Consumers
- `apps/web/server/routes/rpc/[...].ts` — HTTP 入口 (RPCHandler + OpenAPIHandler)
- `apps/web/app/plugins/orpc.client.ts` — 浏览器端 RPC 客户端
- `apps/web/app/plugins/orpc.server.ts` — SSR 服务端直接调用 (当前 ssr:false 实际未使用)

## Module Structure
```
packages/api/src/
├── index.ts           # [CORE] os.$context<Context>() 实例, publicProcedure, requireAuth 中间件, protectedProcedure
├── context.ts         # [CORE] createContext() — 从 headers 解析 session (Better Auth)
├── lib/
│   ├── errors.ts      # [CROSS-CUTTING] adminError() 工厂, AdminBusinessCode 联合类型 (12 种业务错误)
│   └── utils.ts       # [CROSS-CUTTING] 分页 helpers, 序列化函数, ID 生成, 时区工具
└── routers/
    ├── index.ts       # [COMPOSITION ROOT] appRouter 聚合 — 纯对象，key=camelCase 域名
    ├── dashboard.ts, employee.ts, device.ts, attendance-config.ts, attendance-record.ts, face-profile.ts
```

## oRPC Router Pattern (canonical)

```typescript
import { z } from "zod";
import { protectedProcedure } from "../index";
import { adminError } from "../lib/errors";
import { db, employee, eq } from "@hitomi/db";

// Zod schema 在模块顶层定义，命名: {action}Input
const listInput = z.object({
  page: z.number().int().min(1).optional(),       // 默认 1
  pageSize: z.number().int().min(1).max(100).optional(), // 默认 20
  keyword: z.string().trim().optional(),
});

export const employeeRouter = {
  list: protectedProcedure
    .input(listInput)
    .handler(async ({ input }) => {
      const pageInput = normalizePageInput(input);
      const where = input.keyword
        ? or(like(employee.name, `%${input.keyword}%`), like(employee.code, `%${input.keyword}%`))
        : undefined;
      const [items, totalRow] = await Promise.all([
        db.query.employee.findMany({ where, with: { faceProfile: true },
          orderBy: desc(employee.createdAt),
          limit: pageInput.pageSize, offset: (pageInput.page - 1) * pageInput.pageSize }),
        db.select({ value: count() }).from(employee).where(where).get(),
      ]);
      return createPageResult(items.map(serializeEmployeeSummary), totalRow?.value ?? 0, pageInput);
    }),

  create: protectedProcedure
    .input(z.object({ code: z.string().trim().min(1), name: z.string().trim().min(1) }))
    .handler(async ({ input }) => {
      const exists = await db.query.employee.findFirst({ where: eq(employee.code, input.code) });
      if (exists) throw adminError("CONFLICT", "EMPLOYEE_CODE_CONFLICT", "Employee code already exists");
      const id = createId("emp");
      await db.insert(employee).values({ id, code: input.code, name: input.name });
      // Drizzle insert 返回值可以直接使用
      return serializeEmployeeSummary({ id, code: input.code, name: input.name, createdAt: new Date(), updatedAt: new Date(), faceProfile: null });
    }),

  remove: protectedProcedure
    .input(z.object({ id: z.string().min(1), confirmText: z.string().trim().min(1) }))
    .handler(async ({ input }) => {
      const current = await db.query.employee.findFirst({ where: eq(employee.id, input.id) });
      if (!current) throw adminError("NOT_FOUND", "EMPLOYEE_NOT_FOUND", "Employee not found");
      if (input.confirmText !== current.code) throw adminError("CONFLICT", "EMPLOYEE_DELETE_CONFIRMATION_MISMATCH", "...");
      return await db.transaction(async (tx) => {
        const [fpCount, arCount] = await Promise.all([/* count faceProfiles */, /* count attendanceRecords */]);
        await tx.delete(attendanceRecord).where(eq(attendanceRecord.employeeId, input.id));
        await tx.delete(faceProfile).where(eq(faceProfile.employeeId, input.id));
        await tx.delete(employee).where(eq(employee.id, input.id));
        return { id: input.id, deletedFaceProfileCount: fpCount?.value ?? 0, deletedAttendanceRecordCount: arCount?.value ?? 0 };
      });
    }),
};
```

## 程序类型谱系
```
o.$context<Context>()
 ├─ publicProcedure (= o)                 → 无需认证
 │   └─ .use(requireAuth)                → protectedProcedure (需要 session.user)
 │       ├─ .input(zodSchema)            → 添加 Zod 校验
 │       │   └─ .handler(async fn)       → 认证 + 校验后的处理函数
 │       └─ .handler(async fn)           → 认证但无输入校验的处理函数
 └─ o.middleware(async fn)               → 创建中间件 (如 requireAuth)
```

## 错误处理

```typescript
// 两层错误模型:
//   1. HTTP 级别 (ErrorType): BAD_REQUEST | NOT_FOUND | CONFLICT | UNAUTHORIZED
//   2. 业务区分码 (AdminBusinessCode): "EMPLOYEE_NOT_FOUND" | "DEVICE_NOT_FOUND" | ...
throw adminError("NOT_FOUND", "EMPLOYEE_NOT_FOUND", "Employee not found");
// → ORPCError { code: "NOT_FOUND", data: { businessCode: "EMPLOYEE_NOT_FOUND" }, message: "..." }
// 前端通过 error.data.businessCode 做本地化/toast 显示
```

## 分页约定
所有 list procedure 返回 `{ items: T[], pageInfo: { page, pageSize, total, totalPages } }`。
使用 `normalizePageInput()` (默认 page=1, pageSize=20, 上限 100) 和 `createPageResult()` 构建。

## Architectural Boundaries
- **无 service 层**: handler 直接调用 `db` (Drizzle)。当前规模下有意如此设计
- **级联删除是显式的**: `remove` handler 在 `db.transaction()` 中手动删除子记录 (faceProfiles, attendanceRecords)，然后删除父记录。不使用 DB 级联删除
- **确认删除**: 两个实体 (employee, device) 的删除都要求 `confirmText` 匹配唯一字段 (code 或 deviceCode)。独立的 `getDeleteImpact` procedure 返回删除影响范围供 UI 展示
- **人脸档案生命周期**: pending → (设备激活) → success/failed | cancelled。admin 只能 enqueue (设为 pending) 和 cancel (从 pending 设为 cancelled)
- **信任 Drizzle 返回值**: insert/update 返回的数据是可信的，无需 re-fetch
- **考勤槽位状态**: `attendanceRecord.list` 将原始打卡记录按 (employee, date) 分组，计算 `clockInStatus`/`clockOutStatus` 为 `"recorded"` | `"missing"` | `null`(未来日期)

<important if="you are adding a new oRPC router">
## 新增 oRPC Router
1. 创建 `src/routers/my-domain.ts`
2. 在模块顶层定义 Zod schema (命名: `{action}Input`)
3. 导出 `const myDomainRouter = { ... }` 对象
4. 在 `src/routers/index.ts` 中导入并加入 `appRouter` 对象
5. `AppRouter` 和 `AppRouterClient` 类型会自动更新
6. 如需新的业务错误码，加入 `AdminBusinessCode` 联合类型，然后调用 `throw adminError(type, code, message)`
</important>

<important if="you are adding a new business error code">
## 新增业务错误码
1. 在 `src/lib/errors.ts` 的 `AdminBusinessCode` 联合类型中添加新的字面量
2. 在 handler 中: `throw adminError("CONFLICT" | "NOT_FOUND" | "BAD_REQUEST", "YOUR_NEW_CODE", "human-readable message")`
3. 前端代码通过 `error.data.businessCode` 捕获
</important>
