# 答辩问答 — 项目概览·选题·架构·技术选型

---

## Q1: 请用一句话介绍你的毕设项目。

基于 ESP32-S3 的人脸识别考勤系统。由三部分组成：ESP32-S3 终端（摄像头 + 屏幕 + Wi-Fi）完成本地人脸识别打卡，后端服务（oRPC + libSQL）完成校验与数据持久化，Nuxt 管理后台（Web）完成人员、设备和考勤管理。

完整闭环：管理员在后台建员工/注册设备 → 下发录脸任务到终端 → 终端录入人脸模板（本地 SD 卡保存）→ 员工在终端前刷脸打卡 → 终端本地判定上班/下班 → 上传考勤记录到后端 → 管理员在后台查看汇总和缺卡统计。

---

## Q2: 为什么要做这个系统？有哪些实际应用场景？

**选题动机**：
- 传统考勤方式（刷卡、指纹、密码）存在代打卡问题，人脸识别天然防代打。
- 中小企业/实验室/工作室需要低成本、易部署的考勤方案，不希望采购昂贵的商业考勤机加 SaaS 年费。
- ESP32-S3 开发板价格不到百元，搭配摄像头模块和 LCD 屏幕，硬件成本极低。

**适用场景**：
- 小型办公室考勤（≤50 人）
- 实验室/工作室成员出勤管理
- 学校社团活动签到

---

## Q3: 系统三部分之间的通讯关系是怎样的？

```
[管理后台 (Nuxt)] ──oRPC (HTTP)──▶ [后端服务 (Nitro)]
                                        │
                        HTTP JSON API   │ (设备接口)
                                        ▼
[ESP32-S3 终端] ◀─── POST /api/device/* ────▶ [后端服务]
```

1. **管理后台 ↔ 后端**：通过 oRPC（基于 HTTP 的 TypeScript-first RPC 框架）调用管理端接口（`employee.list`、`device.create`、`attendanceRecord.list` 等），所有接口需登录认证（Better Auth session）。
2. **终端 ↔ 后端**：通过普通 HTTP POST JSON API（`/api/device/sync`、`/api/device/attendance/upload` 等），使用 `deviceCode + apiKey` 鉴权。

**为什么不用同一个协议？**
- 管理端接口用 oRPC，因为前端和后端都是 TypeScript，oRPC 提供端到端类型安全。
- 设备端用纯 HTTP JSON，因为 ESP32 运行 C++，无法消费 TypeScript RPC 客户端——纯 JSON API 对嵌入式设备更友好、更简单。

---

## Q4: 为什么选择 Bun monorepo + Turbo 的工程结构？

- **Bun**：原生支持 TypeScript，不需要编译步骤，`bun run` 直接执行 `.ts` 文件，开发体验快。
- **Turbo**：monorepo 任务编排，`bun run dev` 一键启动所有 workspace（web + api + db + auth），带缓存加速。
- **Monorepo 划分**：
  - `packages/db`：Drizzle ORM 数据库层——所有数据定义的唯一真相源
  - `packages/api`：oRPC 后端 API——7 个 domain router
  - `packages/auth`：Better Auth 认证配置——单文件导出
  - `packages/env`：t3-env 环境变量——4 个子路径导出
  - `apps/web`：Nuxt 4 管理后台——消费以上所有 packages
  - `hardware/`：ESP32-S3 固件——独立 PlatformIO 项目，不在 Bun/Turbo 任务图中

---

## Q5: 项目中共有多少张数据库表？分别是什么？

共 9 张表：

**认证表（Better Auth 自动生成）**：`user`、`session`、`account`、`verification`

**业务表（手动设计）**：
| 表 | 用途 |
|---|---|
| `employee` | 员工信息（编号 code、姓名 name） |
| `device` | 设备注册与鉴权（device_code、api_key_hash、状态） |
| `attendance_config` | 全局考勤时间段（上班/下班的起止分钟数）— 单例表 |
| `face_profile` | 录脸任务与状态（employee_id + device_id，per-employee unique） |
| `attendance_record` | 有效考勤记录（每人每天每类型各一条，composite unique） |

---

## Q6: 系统的最小可用闭环是什么？

1. 管理员登录后台 → 创建员工
2. 注册设备 → 通过 Web Serial（浏览器串口）给终端配网 + 下发激活凭据
3. 终端联网激活 → 收到正式 `deviceCode + apiKey`
4. 配置考勤时间段（如上班 09:00-10:00，下班 18:00-19:00）
5. 为员工下发录脸任务 → 终端完成人脸录入（模板存 SD 卡本地）
6. 员工在终端前刷脸 → 终端识别身份 → 判定上班/下班 → 缓存记录
7. 联网时批量上传考勤记录 → 后台校验入库
8. 管理员在后台查看到员工单日汇总，缺卡自动标注

---

## Q7: 为什么做的是"单组织·单管理员·单设备"的 MVP？

毕设的定位是**验证完整闭环的可行性**，而非做商业化产品。

- **单组织**：不需要多租户隔离，SQLite 足够。
- **单管理员**：不需要 RBAC 权限模型，Better Auth 的 session 认证即可。
- **单设备**：不需要设备间数据一致性协议（如向量时钟/CRDT）。

如果扩展到多设备，需要考虑：
- 同一员工在不同设备上的录脸模板一致性
- 同一员工同一天的重复打卡去重（跨设备）

这些是**产品化问题**而非**技术可行性问题**，因此 MVP 阶段不做。

---

## Q8: 你在项目中主要承担了哪些工作？

全栈开发：
1. **硬件侧**：ESP32-S3 固件——摄像头驱动、LVGL 触摸界面、Wi-Fi 连接策略、HTTP 客户端、SD 卡模板存储、ESP-WHO 人脸识别集成、USB 串口配网协议。
2. **后端侧**：数据库 Schema 设计（Drizzle ORM）、19 个管理端 oRPC 接口实现、3 个设备端 HTTP 接口实现、鉴权逻辑、考勤校验逻辑。
3. **前端侧**：Nuxt 4 管理后台——7 个页面、23 个 Vue 组件、Web Serial API 串口配置页面、响应式布局。

（注：根据实际分工调整）

---

## Q9: 项目中最让你自豪的技术点是什么？

（根据自己的亮点回答，以下是可能的选项）

**选项 A — 硬件分层架构**：ESP32-S3 固件采用 Ports & Adapters（六边形架构），将纯领域逻辑（`core/`）、硬件适配（`infra/`）、应用编排（`app/`）严格分离。`core/` 层的代码不依赖任何 Arduino/ESP-IDF 头文件，可以在 `platformio test -e native` 下直接跑单元测试。

**选项 B — Web Serial 配网**：利用浏览器 Web Serial API 直接通过 USB 串口给 ESP32 配置 Wi-Fi 和后台地址，无需屏幕键盘输入 SSID/密码，用户体验流畅。

**选项 C — 离线容错**：设备在断网时继续本地识别打卡，待上传队列持久化到 LittleFS。网络恢复后自动批量上传，上传结果逐条匹配 `clientRecordId` 清理本地队列。

**选项 D — 端到端类型安全**：管理后台（Nuxt + oRPC 客户端）→ 后端 API（oRPC router）→ 数据库（Drizzle ORM schema），整个 TS 调用链有完整类型推导，改一个字段编译器立刻报错。

---

## Q10: 有没有参考过现有的开源项目或产品？

- **ESP-WHO**：Espressif 官方人脸检测/识别框架，提供 `face_detection` 和 `face_recognition` 模型，项目直接集成其 C API。
- **LVGL**：开源嵌入式 GUI 库，驱动 ST7789 SPI 显示屏和 FT6336 触摸控制器。
- **Better Auth**：TypeScript 认证库，提供 session、email/password 登录，通过 Drizzle adapter 对接项目数据库。
- **Drizzle ORM**：TypeScript-first SQL ORM，支持 libSQL/SQLite，用 `sqliteTable()` + `relations()` 定义 schema，生成类型安全的查询。

没有直接参考某个完整的考勤开源项目。架构设计遵循 Clean Architecture 和 Ports & Adapters 思想。
