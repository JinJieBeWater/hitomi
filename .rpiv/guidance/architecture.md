# Project Overview
Hitomi — 人脸识别考勤管理系统。一个 **bun monorepo + turbo** 项目，包含 Nuxt 4 管理后台 (`apps/web`)、oRPC 后端 API (`packages/api`)、Drizzle ORM 数据库层 (`packages/db`)、Better Auth 认证 (`packages/auth`)、t3-env 环境配置 (`packages/env`)，以及独立的 ESP32-S3 PlatformIO 固件项目 (`hardware/`)。

# Architecture
```
hitomi/
├── apps/web/              # Nuxt 4 管理后台 (CSR) + Nitro 设备 API 网关
│   └── app/components/    # 23 个 Vue 组件库
├── packages/
│   ├── api/               # oRPC 后端 API — 7 个 domain router
│   ├── db/                # Drizzle ORM (libSQL) — 9 张表 (4 auth + 5 business)
│   ├── auth/              # Better Auth 配置 — 单文件导出
│   ├── env/               # 统一环境变量 (4 个子路径导出)
│   └── config/            # 共享 TypeScript 基础配置
├── hardware/              # ESP32-S3 固件 (PlatformIO · 独立项目)
│   ├── src/app/           # 应用运行时编排层
│   └── src/infra/         # 硬件抽象层 (Ports & Adapters)
└── spec/                  # 产品规格文档 (AGENTS.md 引用的设计真相源)
```

**依赖流**: `apps/web` → `@hitomi/api` → `@hitomi/auth` + `@hitomi/db` → `@hitomi/env`。`hardware/` 完全独立，通过 REST API 与 web 后端通信。

**两轨开发**: `web/api/db` 和 `hardware` 是独立的开发轨道。`hardware/` 不在 `bun`/`turbo` 任务图中。

# Commands
| Command | Purpose |
|---|---|
| `bun run dev` | 启动所有 turbo 工作区 |
| `bun run check-types` | 全项目类型检查 |
| `bun run smoke:admin` | 管理员 API 烟幕测试 |
| `bun run smoke:device` | 设备 API 烟幕测试 |
| `bun run smoke:admin-ui` | 管理员 UI 烟幕测试 |
| `bun run db:push` / `db:generate` / `db:migrate` | Drizzle schema 管理 |
| `cd hardware && platformio run -e szpi_esp32s3` | 构建固件 |
| `cd hardware && platformio test -e native` | 固件本地测试 |

# Business Context
基于人脸识别的考勤系统。管理员创建员工和设备，通过 USB 串口配网设备，设备通过 Wi-Fi 与后端同步（拉取员工列表 + 注册任务，上传考勤记录）。人脸注册在设备端进行（ESP-WHO），识别人脸后自动记录上下班打卡。

<important if="you are starting any non-trivial change">
## 变更前必读
阅读 `spec/` 中的相关文档后再修改代码:
- Admin 页面/UI: `spec/admin-pages.md` → `spec/admin-api.md`
- Admin API/DB: `spec/admin-api.md` → `spec/database-design.md`
- 设备协议: `spec/device-api.md`
- 设备本地: `spec/device-local-design.md`
- 硬件任务: 先读 [SZPI ESP32-S3 板卡 Wiki](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/) 再读 `hardware/README.md`
</important>

<important if="you are making a git commit">
## 提交规范
- 使用英文主题，前缀如 `feat:`, `fix:`, `docs:`, `chore:`, `hardware:`, `feat(hardware):`
- 主题简洁可搜索；额外上下文放在正文中（可用中文）
- `hardware/` 变更使用 `hardware:` 或 `feat(hardware):` 前缀
</important>

<important if="you are adding a new domain entity end-to-end">
## 新增领域实体（全栈）
1. **DB**: 按照 `packages/db/architecture.md` 添加表并生成迁移
2. **API**: 按照 `packages/api/architecture.md` 创建 router
3. **Web 页面**: 按照 `apps/web/architecture.md` 和 `apps/web/app/components/architecture.md` 创建页面 + 编辑器
4. **类型检查**: `bun run check-types`
5. **烟幕测试**: `bun run smoke:admin`
</important>

<important if="you are writing or modifying tests">
## 测试约定
- Web: `bun run smoke:admin` / `smoke:device` / `smoke:admin-ui` 烟幕测试
- 固件: `cd hardware && platformio test -e native` 用 Unity 风格的 `expect()` + try/catch
- 每个测试套件必须清理数据（web）并恢复初始状态
</important>
