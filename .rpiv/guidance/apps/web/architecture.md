# apps/web — Nuxt 4 Admin Web App (CSR)

## Responsibility
Hitomi 考勤管理后台的浏览器前端 + Nitro 设备 API 网关。管理员在此管理员工、设备、人脸录入、考勤记录和考勤时段配置。IoT 设备（ESP32）通过 Nitro server 端点与后台同步。

## Dependencies
- **`@nuxt/ui` v4**: 组件库（UCard, UTable, UButton, UModal, USlideover 等），驱动所有 UI 原语
- **`@orpc/client` + `@orpc/tanstack-query`**: 端到端类型安全 RPC；服务端 procedure 类型自动传递到浏览器 `$orpc.xxx.queryOptions()`
- **`@tanstack/vue-query`**: 唯一状态管理层；无 Pinia。服务器状态缓存在 queryClient，本地 UI 状态用 `ref`/`computed`
- **`better-auth`**: 邮箱/密码认证，session cookie，通过 `$authClient` 注入
- **`@xterm/xterm`**: Web Serial 终端模拟器（设备串口配置页面）

## Module Structure
```
apps/web/
├── app/                              # Vue 前端 (CSR, ssr: false)
│   ├── pages/                        # 文件路由: index, login, dashboard, employees, face-profiles, attendance-records, devices/(index|serial)
│   ├── components/                   # 23 个 Vue SFC — 见子目录 architecture.md
│   ├── composables/                  # useDeviceSerial (Web Serial 单例), usePagedListState (分页+过滤重置)
│   ├── layouts/                      # default (公开页面), dashboard (认证页面+侧边栏)
│   ├── middleware/                   # auth.ts — 客户端路由守卫，检查 session 后重定向到 /login
│   ├── plugins/                      # orpc.client.ts, auth-client.ts, vue-query.ts — 均通过 defineNuxtPlugin → provide 注入
│   ├── utils/                        # format.ts (日期/状态标签), pagination.ts (fetchAllPages), deviceSerialSupport.ts
│   └── assets/css/                   # main.css — Tailwind 指令 + workspace-* 设计 token
└── server/                           # Nitro 服务端 (同进程)
    ├── api/auth/[...all].ts          # Better Auth 全部端点 → auth.handler()
    ├── api/device/                   # 设备 REST 端点: bootstrap/hello, sync, attendance/upload, enrollment/report
    │   └── (全部 POST, Zod 校验, deviceSuccess/deviceFailure 信封)
    ├── routes/rpc/[...].ts           # oRPC + OpenAPI 处理器 — 所有管理端 RPC 的入口
    └── utils/device.ts               # 设备端点共享: Zod schemas, 认证, 时间计算, DB 查询 (单体工具文件)
```

## Page Composition Pattern (canonical)
每个数据列表页面按此结构组装：

```vue
<script setup lang="ts">
definePageMeta({ layout: "dashboard", middleware: ["auth"] });
const { $orpc } = useNuxtApp();
const queryClient = useQueryClient();

const page = ref(1); const keyword = ref("");
const listQuery = useQuery(computed(() =>
  $orpc.entity.list.queryOptions({ input: { page: page.value, pageSize: 20, keyword: keyword.value || undefined } })
));
const rows = computed(() => listQuery.data.value?.items ?? []);
</script>

<template>
  <UDashboardPanel>
    <template #header>
      <PageHeader title="实体管理" :badges="headerBadges">
        <template #actions>
          <UButton icon="i-lucide-refresh-cw" @click="listQuery.refetch()">刷新</UButton>
          <UButton icon="i-lucide-plus" @click="openCreate()">新增</UButton>
        </template>
      </PageHeader>
    </template>
    <template #body>
      <div class="workspace-page-stack">
        <FilterBar> <!-- 搜索/过滤 --> </FilterBar>
        <DataSurface title="列表">
          <QueryGuard :status="listQuery.status.value" :empty="rows.length === 0">
            <UTable :data="rows" :columns="columns">
              <template #actions-cell="{ row }">
                <RowActions :items="getRowActions(row.original)" />
              </template>
            </UTable>
          </QueryGuard>
          <template #footer>
            <ListPagination :page="page" :page-size="20" :total="total" />
          </template>
        </DataSurface>
      </div>
      <!-- Slideover 编辑器 + DeleteConfirmModal -->
    </template>
  </UDashboardPanel>
</template>
```

## Architectural Boundaries
- **SSR 已关闭** (`ssr: false`): 所有数据获取在浏览器中通过 `@tanstack/vue-query` + oRPC 客户端进行
- **设备 API 与管理员 RPC 完全分离**: 设备端点使用原始 Nitro `defineEventHandler` 和手动 Zod 校验；不走 oRPC
- **Web Serial 直接连接浏览器↔设备**: 绕过服务端。`useDeviceSerial` composable 维护模块级单例共享端口
- **全局考勤配置只有一行**: `server/utils/device.ts` 中的 `getSyncPayload` 调用 `findFirst()` — 系统只维护一条考勤配置，无按团队/按设备区分

## CSS 设计 Token
所有自定义 CSS 类使用 `workspace-` 前缀:
`workspace-page-stack`, `workspace-panel-icon`, `workspace-metric-grid`, `workspace-filter-bar`, `workspace-empty-state`, `workspace-danger-panel`, `workspace-code-value` 等

## Commands
| Command | Purpose |
|---|---|
| `bun run dev:web` | 启动 Nuxt 开发服务器 |
| `bun run check-types` | 全项目类型检查 |

<important if="you are adding a new management page">
## 新增管理页面
1. 在 `@hitomi/api` 中创建 router + procedure (参考 `packages/api/architecture.md`)
2. 创建 `pages/my-resource.vue` 并加上 `definePageMeta({ layout: "dashboard", middleware: ["auth"] })`
3. 按上方 Page Composition Pattern 组装 PageHeader → FilterBar → DataSurface → QueryGuard → UTable → ListPagination
4. 如需编辑表单，创建 `components/MyResourceSlideoverEditor.vue` (参考 `apps/web/app/components/architecture.md`)
5. 在 `layouts/dashboard.vue` 的 `mainLinks` 数组中添加导航条目
</important>

<important if="you are adding a new device API endpoint">
## 新增设备 API 端点
1. 在 `server/utils/device.ts` 中定义 Zod 校验 schema
2. 创建 `server/api/device/xxx.post.ts`，遵循此模式:
   `parseDeviceBody(event, schema)` → `authenticateDevice(...)` → 业务逻辑 → `deviceSuccess(data)` / `deviceFailure(code, msg)`
3. 用 try/catch + `deviceInternalError(event, error)` 包裹
</important>
