# packages/env — 统一环境配置层

## Responsibility
monorepo 环境变量的**唯一来源**——解析、验证、路径发现。三个消费者通过 4 个子路径导出使用：
- `@hitomi/env/app` — 纯解析函数（Nuxt config 构建时）
- `@hitomi/env/server` — 已验证的 Node 运行时 env（`@hitomi/db`, `@hitomi/auth`, `@hitomi/api`）
- `@hitomi/env/web` — Nuxt 构建时校验（副作用导入）
- `@hitomi/env/paths` — 工作区根目录发现 + `.env` 路径解析

## Dependencies
- **`@t3-oss/env-core`** ^0.13.1: 服务端 Zod 环境校验
- **`@t3-oss/env-nuxt`** ^0.13.1: Nuxt 构建时校验
- **`dotenv`**: `.env` 文件加载
- **`zod`**: 校验 schema 定义
- **`node:fs`, `node:path`, `node:url`**: `paths.ts` 中用于文件系统发现

## 两层架构

### 第一层: 纯解析函数 (`app.ts`) — 无依赖，可测试
```typescript
import { resolveAppOrigin, resolveAppPort, resolveAppListenHost } from "@hitomi/env/app";

// 接受 EnvLike = Record<string, unknown> (process.env 或普通对象)
// 优先级链: PORT → APP_PORT → NUXT_PORT → 3001 (默认)
const port = resolveAppPort(process.env);

// 通配符地址暂留: "0.0.0.0" 用于开发服务器 LAN 绑定
const listenHost = resolveAppListenHost(process.env);

// 公共 URL: 通配符 → "localhost"
const host = resolveAppHost(process.env);

// 组合解析: APP_BASE_URL → BETTER_AUTH_URL → CORS_ORIGIN
//           → NUXT_PUBLIC_SERVER_URL → http://{host}:{port} (构造回退)
const origin = resolveAppOrigin(process.env);
```

### 第二层: t3-oss Zod 校验 (`server.ts`) — 运行时保证
```typescript
import { env } from "@hitomi/env/server";

// dotenv 加载 (如 apps/web/.env 存在) → createEnv Zod 校验 → app.ts 回退
// 必填项: DATABASE_URL (非空), BETTER_AUTH_SECRET (≥32 字符)
// 可选项: APP_HOST, APP_PORT, APP_BASE_URL, BETTER_AUTH_URL, CORS_ORIGIN
// 默认值: NODE_ENV = "development"
// 缺失可选项通过 ?? 回退到 app.ts 解析器 → 保证每个导出字段都有值
```

## 工作区根目录发现 (`paths.ts`)
上行搜索，直到 4 个 marker 文件**全部存在**: `package.json`, `bun.lock`, `turbo.json`, `apps/web/package.json`。
4 个种子起始目录（按优先级）: `process.cwd()` → `INIT_CWD` → `PWD` → 本模块路径。

## 消费模式
| 消费者 | 导入 | 用法 |
|---|---|---|
| `apps/web/nuxt.config.ts` | `@hitomi/env/web` + `@hitomi/env/app` | 副作用导入触发构建时校验 + 纯解析函数设置 devServer/runtimeConfig |
| `@hitomi/auth` | `@hitomi/env/server` | `env.BETTER_AUTH_SECRET`, `env.BETTER_AUTH_URL`, `env.CORS_ORIGIN` |
| `@hitomi/db` | `@hitomi/env/server` | `env.DATABASE_URL`, `serverEnvPath`（解析相对路径） |
| `@hitomi/db` drizzle.config.ts | `@hitomi/env/paths` | 仅路径解析，手动调用 `dotenv.config()` |

## 关键设计
- **单个规范 `.env` 位置**: `apps/web/.env` — 整个 monorepo 只有一个
- **fail-fast**: `createEnv` 在模块导入时运行 — 必填项缺失立即崩溃
- **`emptyStringAsUndefined: true`**: 空字符串视同缺失
- **`as const`**: 保留字面量类型 (如 `NODE_ENV: "development" | "production" | "test"`)

<important if="you are adding a new environment variable">
## 新增环境变量
1. `app.ts`: 如果需要纯解析，添加 `readValue(env, "NEW_VAR")` + normalize/resolve 函数
2. `server.ts`: 在 Zod schema 中添加 `NEW_VAR: z.string().min(1).optional()`；如需回退，在导出的 `env` 对象中加上 `NEW_VAR: rawEnv.NEW_VAR ?? resolveNewVar(process.env)`
3. `web.ts`: 仅当变量需要暴露给浏览器时才添加（通常不需要）
4. `nuxt.config.ts`: 如需，从 `@hitomi/env/app` 导入并在 `runtimeConfig` 中使用
5. 测试: 在 `app.test.js` 中用对象字面量测试纯解析函数
</important>
