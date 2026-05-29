# 答辩问答 — 后端·数据库·API设计·安全

---

## Q1: 后端整体架构是怎样的？

```
apps/web (Nuxt 4)
  └── nitro handler ──▶ packages/api (oRPC routers)
                              │
                              ├── Better Auth middleware ──▶ packages/auth
                              │
                              ├── Drizzle query ──▶ packages/db ──▶ libSQL/SQLite
                              │
                              └── Device HTTP handler ──▶ POST /api/device/*
```

- **框架**：Nitro（Nuxt 4 内置的 server engine）
- **管理端**：oRPC — TypeScript-first RPC，输入输出有完整类型推导
- **设备端**：纯 HTTP JSON POST — 路径 `/api/device/*`，用 `deviceCode + apiKey` 鉴权
- **认证**：Better Auth — session-based 登录（email/password），仅管理端需要
- **数据库**：libSQL（SQLite 兼容）通过 Drizzle ORM 操作

---

## Q2: 为什么管理端用 oRPC，设备端用纯 HTTP？

| | 管理端（oRPC） | 设备端（HTTP JSON） |
|---|---|---|
| 调用方 | Nuxt web app (TypeScript) | ESP32-S3 (C++) |
| 鉴权 | Better Auth session cookie | deviceCode + apiKey |
| 类型安全 | 端到端（client → server → db） | 无（手工 JSON） |
| 错误处理 | oRPC error code + businessCode | HTTP 200 + `{success, error}` |

**选择理由**：
- oRPC 给 TypeScript 调用链提供编译时类型检查——改了接口字段，编译不通过，不会出运行时 bug。
- 设备端跑 C++，无法消费 TypeScript RPC 客户端，纯 JSON API 是最通用、最易实现的选择。
- 设备端接口使用了稳定的错误码体系（`DEVICE_AUTH_FAILED`、`ATTENDANCE_NOT_IN_WINDOW` 等），设备按 code 做逻辑判断，不依赖 message 字符串。

---

## Q3: 考勤记录的校验逻辑是什么？

设备上传考勤记录（`POST /api/device/attendance/upload`）后，服务端逐条校验：

```
1. 鉴权：deviceCode + apiKey 是否匹配 ✓
2. 设备状态：是否 active（未禁用） ✓
3. 员工存在性：employeeId 是否在库中 ✓
4. 考勤配置：是否已保存全局考勤配置 ✓
5. 时间校验：recognizedAt 是否落在对应 type 的时间窗口 ✓
6. 去重判断：同一 (employeeId, localDate, type) 是否已存在 ✓
   ├── 不存在 → 新插入 (status: saved)
   ├── 已存在且新记录更早 → 更新为更早时间 (status: updated_earlier)
   └── 已存在且新记录更晚/相同 → 忽略 (status: ignored_duplicate)
7. 所有校验不通过 → rejected，返回具体错误码
```

**为什么要校验时间？** 设备端已经判断过了，但：
- 设备时钟可能不准（没有 RTC 电池）
- 设备端 sync 拿到的配置可能过期（断网期间配置被管理员改了）
- 服务端是最终的权威校验者

---

## Q4: 考勤时间段是怎么设计的？为什么用分钟数？

考勤配置表是全局单例（`id = 'default'`），存 4 个分钟数字段：

| 字段 | 含义 | 示例 |
|---|---|---|
| `work_start_minute` | 上班开始 | 540（09:00） |
| `work_end_minute` | 上班结束 | 600（10:00） |
| `off_start_minute` | 下班开始 | 1080（18:00） |
| `off_end_minute` | 下班结束 | 1140（19:00） |

**为什么用分钟数？**
- 比 `HH:mm` 字符串更易比较（直接做整数 `>=` `<` 判断）
- 避免时区字符串的歧义（"09:00" 是哪个时区？）
- 服务端和设备端都用整数比较，逻辑一致
- `Asia/Shanghai` 时区固定，无需存储时区偏移

**约束规则**：
- 分钟数必须在 `0-1439`
- `start < end`
- 两个时间段不得重叠
- 不得跨天
- 判定规则：`[start, end)` 左闭右开

---

## Q5: 设备 apiKey 的管理机制是怎样的？

**创建阶段**：
1. 管理员创建设备 → 服务端生成随机 `apiKey`，哈希后存 `api_key_hash`（数据库不存明文）
2. 同时生成 `bootstrapSerial` + `bootstrapSecret` 作为首次激活凭据
3. 创建成功 → 前端一次性展示 bootstrap 凭据，明确提示：运行时 apiKey 由设备激活时领取，管理端不展示

**激活阶段**（Bootstrap Flow）：
1. 终端通过 Web Serial 拿到 `bootstrapSerial + bootstrapSecret`
2. 终端调用 `POST /api/device/bootstrap/hello` → 服务端验证凭据
3. 终端调用 `POST /api/device/bootstrap/claim-status` → 服务端下发正式 `apiKey` 明文
4. 终端收到 `apiKey` 后写入本地 NVS，后续所有业务接口使用 `deviceCode + apiKey` 鉴权
5. 服务端激活完成后清除暂存的 bootstrap 凭据

**运行时**：
- 终端每次请求携带 `deviceCode` + 明文 `apiKey`
- 服务端通过 `deviceCode` 查设备，用 `apiKey` 与 `api_key_hash` 校验
- **MVP 阶段不支持 apiKey 重置/轮换**——如果泄露需要重新创建设备

---

## Q6: 录脸任务的状态机是怎样的？

```
        ┌─ admin enqueue ──▶ [pending]
        │                       │
        │         ┌─────────────┼──────────────┐
        │         ▼             ▼              ▼
        │     device report  device report   admin cancel
        │      result=success  result=failed     │
        │         │             │                │
        │         ▼             ▼                ▼
        │     [success]     [failed]        [cancelled]
        │         │             │                │
        └─────────┴─────────────┴────────────────┘
              (re-enqueue 重新发起，回到 pending)
```

- 每个员工只有**一条**当前录脸记录（`uniqueIndex` on `employee_id`）
- `enqueue` 时可重新分配设备和重置状态（复用同一接口）
- `cancel` 只能取消 `pending` 状态的任务
- 如果设备对已取消的任务继续上报结果 → 服务端返回 `TASK_CANCELLED` 并忽略

---

## Q7: 考勤记录页面的逐日汇总逻辑是什么？

考勤记录列表不直接展示原始记录，而是以 `employeeId + localDate` 为粒度做**日汇总行**：

```
┌──────────┬────────┬──────────┬──────────────────┬──────────────────┐
│ 日期     │ 员工   │ 上班     │ 下班             │ 状态             │
├──────────┼────────┼──────────┼──────────────────┼──────────────────┤
│ 03-28    │ 张三   │ 09:05    │ 18:10            │ 正常             │
│ 03-28    │ 李四   │ 09:02    │ 缺卡             │ 下班缺卡         │
│ 03-27    │ 张三   │ 缺卡     │ 18:05            │ 上班缺卡         │
└──────────┴────────┴──────────┴──────────────────┴──────────────────┘
```

**实现方式**：
- 接口 `attendanceRecord.list` 返回聚合后的行，每行包含 `clockIn` 和 `clockOut` 两个子对象
- 记录为空时，根据 `clockInStatus` / `clockOutStatus` 判断：
  - 当天未结束的时段 → `null`（不判定缺卡）
  - 已过时间段或历史日期 → `missing`（判定缺卡）

**筛选功能**：
- `slotStatus=missing` → 只看有缺卡日的记录
- `type=clock_out` + `slotStatus=missing` → 只看下班缺卡日

---

## Q8: 月度考勤统计是怎么做的？

`attendanceRecord.monthlySummary` 接口：

**输入**：月份 `2026-03`，可选 `employeeId`

**输出**：
```json
{
  "items": [
    {
      "employee": {"code": "20230001", "name": "张三"},
      "activeDays": 22,          // 本月有记录的活跃天数
      "clockInCount": 22,         // 上班打卡次数
      "clockOutCount": 21,        // 下班打卡次数
      "missingClockInCount": 0,   // 上班缺卡天数
      "missingClockOutCount": 1,  // 下班缺卡天数
      "days": [...]               // 每日明细
    }
  ]
}
```

**关键规则**：
- 不生成完整自然月日历——只统计已有记录的日期
- 缺卡判断：某员工某日已有上班或下班任一记录，才对另一时段判断缺卡
- 当天未结束的时段不计缺卡（不能上午 10 点就说人下班缺卡）
- **MVP 不处理工作日/休息日**——月度统计是基于已有记录的汇总，非排班考勤报表

---

## Q9: 数据库为什么选 libSQL / SQLite？为什么不选 PostgreSQL？

- **单组织·单服务·MVP**：不需要分布式、不需要多写节点。
- **SQLite 零运维**：一个文件即数据库，备份只需 `cp`，毕设答辩现场也可轻松演示。
- **libSQL** 是 SQLite 的超集（Turso 维护），兼容 SQLite 协议，但提供 HTTP 远程访问能力——如果将来需要把数据库放到云端，只需改连接字符串。
- **PostgreSQL 太重**：对于 5 张业务表、单人使用的考勤系统，PostgreSQL 的安装和运维成本远高于收益。

**Drizzle ORM 的角色**：
- 用 TypeScript 定义 schema → 自动生成 migration SQL → 类型安全的查询 API
- 和 SQLite raw SQL 的关系：Drizzle 是类型安全层，底层仍是 SQLite

---

## Q10: 系统有哪些安全措施？

| 层面 | 措施 |
|---|---|
| **管理端认证** | Better Auth session-based login（email + password），密码哈希存储（bcrypt） |
| **设备鉴权** | `deviceCode + apiKey` 双向校验；apiKey 哈希存储，数据库不存明文 |
| **设备激活** | Bootstrap 双因子（bootstrapSerial + bootstrapSecret），激活成功后清除暂存凭据 |
| **设备禁用** | 管理员可禁用设备，禁用后所有设备端接口返回 `DEVICE_DISABLED` |
| **传输安全** | HTTP（MVP 阶段内网使用），生产环境应升级为 HTTPS |
| **SQL 注入防护** | Drizzle ORM 参数化查询，不存在拼接 SQL |
| **API 鉴权** | 管理端接口必须登录（session 校验）；设备端接口必须有合法 deviceCode + apiKey |
| **数据删除** | 员工/设备删除需二次确认（输入编号），事务中级联删除关联数据 |
| **隐私保护** | 人脸模板仅存设备本地 SD 卡，服务端不保存人脸图片和特征向量 |

**MVP 不做但应考虑的**：`apiKey` 过期轮换、HTTPS、请求签名防重放、速率限制。

---

## Q11: `apiKey` 验证为什么用哈希而不是明文存储？

和密码存储一样的原则——**即使数据库泄露，攻击者也无法直接拿到可用的 apiKey**。

```typescript
// 设备创建时
const apiKey = generateRandomKey();           // 生成明文
const apiKeyHash = await hash(apiKey);        // 哈希
db.insert({ api_key_hash: apiKeyHash });      // 只存哈希
// 明文 apiKey 只在创建时展示一次，通过 bootstrap 激活下发给设备

// 设备请求时
const device = db.findByDeviceCode(deviceCode);
const isValid = await verify(apiKey, device.api_key_hash);  // 验证
```

---

## Q12: 删除员工或设备时为什么要求输入编号确认？

防止误操作。删除操作是不可逆的（级联删除关联的录脸任务和考勤记录）：
- 删除前调用 `getDeleteImpact` → 展示影响范围（几条录脸记录、几条考勤记录）
- 用户在确认弹窗中输入员工编号/设备码 → 必须完全匹配
- 所有删除在数据库事务中执行（先删关联数据，再删主体）

**为什么不搞软删除（deleted_at）？**
MVP 阶段数据量小，业务逻辑简单。软删除会增加所有查询的 `WHERE deleted_at IS NULL` 过滤条件，收益不大。
