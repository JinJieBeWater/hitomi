# 管理端 oRPC 接口设计

## 文档目标

本文档定义当前毕设 MVP 考勤系统中管理端 oRPC 接口的输入、输出、校验规则和业务错误。

本文档用于指导以下实现：

- 后台页面调用接口
- 服务端管理端路由实现
- 数据库查询与写入逻辑设计

---

## 适用范围

本文档覆盖以下管理端接口：

- `dashboard.summary`
- `employee.list`
- `employee.create`
- `employee.update`
- `employee.getDeleteImpact`
- `employee.remove`
- `device.list`
- `device.create`
- `device.update`
- `device.getDeleteImpact`
- `device.findByBootstrapSerial`
- `device.remove`
- `attendanceConfig.get`
- `attendanceConfig.save`
- `faceProfile.list`
- `faceProfile.enqueue`
- `faceProfile.cancel`
- `attendanceRecord.list`
- `attendanceRecord.monthlySummary`

当前文档遵循以下 MVP 前提：

- 单管理员
- 单设备演示场景
- 默认使用 `Asia/Shanghai`
- 不处理 `apiKey` 重置
- 当前 MVP 不额外校验员工与设备绑定关系

---

## 通用约定

### 1. 鉴权

- 所有管理端接口都必须登录后访问
- 未登录请求统一返回 `UNAUTHORIZED`
- 管理员身份由 Better Auth session 提供

### 2. 命名方式

- 接口名统一采用 `domain.action` 形式
- 输入输出字段统一使用 `camelCase`

### 3. 时间与日期

- 所有时间字段统一使用 Unix 毫秒时间戳
- 考勤日期字段统一使用 `YYYY-MM-DD`
- 所有考勤相关日期计算都固定按 `Asia/Shanghai`

### 4. 分页约定

所有列表接口统一使用以下分页输入：

```ts
type PageInput = {
  page?: number; // 默认 1
  pageSize?: number; // 默认 20，最大 100
};
```

所有列表接口统一返回以下分页信息：

```ts
type PageInfo = {
  page: number;
  pageSize: number;
  total: number;
  totalPages: number;
};
```

当前 MVP 约定：

- `page` 小于 `1` 视为非法请求
- `pageSize` 取值范围为 `1-100`
- 不提供自定义排序参数，列表排序规则在各接口内固定

### 5. 错误约定

管理端接口通过 oRPC 抛出错误。

当前设计约定错误至少包含两层信息：

- 通用错误类型：例如 `UNAUTHORIZED`、`BAD_REQUEST`、`NOT_FOUND`、`CONFLICT`
- 稳定业务错误码：供前端进行稳定判断

推荐错误结构：

```ts
type AdminBusinessError = {
  code: "BAD_REQUEST" | "NOT_FOUND" | "CONFLICT" | "UNAUTHORIZED";
  businessCode?: string;
  message: string;
};
```

---

## 公共业务错误码

| businessCode                       | 含义                            | 推荐错误类型  |
| ---------------------------------- | ------------------------------- | ------------- |
| `EMPLOYEE_NOT_FOUND`               | 员工不存在                      | `NOT_FOUND`   |
| `EMPLOYEE_CODE_CONFLICT`           | 员工编号重复                    | `CONFLICT`    |
| `EMPLOYEE_DELETE_CONFIRMATION_MISMATCH` | 员工删除确认信息不匹配     | `CONFLICT`    |
| `DEVICE_NOT_FOUND`                 | 设备不存在                      | `NOT_FOUND`   |
| `DEVICE_DISABLED`                  | 设备已禁用                      | `CONFLICT`    |
| `DEVICE_DELETE_CONFIRMATION_MISMATCH` | 设备删除确认信息不匹配      | `CONFLICT`    |
| `ATTENDANCE_CONFIG_INVALID_MINUTE` | 分钟值不在 `0-1439` 范围内      | `BAD_REQUEST` |
| `ATTENDANCE_CONFIG_INVALID_RANGE`  | 时间段开始结束关系不合法        | `BAD_REQUEST` |
| `ATTENDANCE_CONFIG_OVERLAPPED`     | 上下班时间段重叠或相互包含      | `BAD_REQUEST` |
| `FACE_PROFILE_NOT_FOUND`           | 录脸任务不存在                  | `NOT_FOUND`   |
| `FACE_PROFILE_NOT_PENDING`         | 当前录脸任务不是 `pending` 状态 | `CONFLICT`    |

---

## 公共数据结构

### 1. EmployeeSummary

```json
{
  "id": "emp_001",
  "code": "20230001",
  "name": "张三",
  "createdAt": 1743158400000,
  "updatedAt": 1743158400000,
  "faceProfile": {
    "id": "fp_001",
    "status": "success",
    "deviceId": "dev_001",
    "deviceName": "一号设备",
    "updatedAt": 1743158400000
  }
}
```

说明：

- 若员工当前没有 `face_profile`，则 `faceProfile = null`

### 2. DeviceSummary

```json
{
  "id": "dev_001",
  "deviceCode": "DEV-001",
  "name": "一号设备",
  "status": "active",
  "lastSeenAt": 1743158400000,
  "createdAt": 1743158400000,
  "updatedAt": 1743158400000
}
```

### 3. AttendanceConfig

```json
{
  "id": "default",
  "workStartMinute": 540,
  "workEndMinute": 600,
  "offStartMinute": 1080,
  "offEndMinute": 1140,
  "updatedAt": 1743158400000
}
```

### 4. FaceProfileSummary

```json
{
  "id": "fp_001",
  "employeeId": "emp_001",
  "deviceId": "dev_001",
  "status": "pending",
  "createdAt": 1743158400000,
  "updatedAt": 1743158400000,
  "employee": {
    "id": "emp_001",
    "code": "20230001",
    "name": "张三"
  },
  "device": {
    "id": "dev_001",
    "name": "一号设备",
    "deviceCode": "DEV-001",
    "status": "active"
  }
}
```

### 5. AttendanceRecordSummary

```json
{
  "id": "ar_001",
  "employeeId": "emp_001",
  "deviceId": "dev_001",
  "recognizedAt": 1743158400000,
  "localDate": "2026-03-28",
  "type": "clock_in",
  "createdAt": 1743158400000,
  "updatedAt": 1743158400000,
  "employee": {
    "id": "emp_001",
    "code": "20230001",
    "name": "张三"
  },
  "device": {
    "id": "dev_001",
    "name": "一号设备",
    "deviceCode": "DEV-001"
  }
}
```

---

## 1. `dashboard.summary`

### 作用

为 Dashboard 页面提供首页统计信息。

### 输入

无输入。

### 输出

```json
{
  "todayLocalDate": "2026-03-28",
  "employeeCount": 10,
  "deviceCount": 1,
  "todayClockInCount": 8,
  "todayClockOutCount": 6
}
```

### 处理规则

- `todayLocalDate` 固定按 `Asia/Shanghai` 计算
- `deviceCount` 统计所有设备数量
- `todayClockInCount` 统计当日 `clock_in` 记录数
- `todayClockOutCount` 统计当日 `clock_out` 记录数

---

## 2. `employee.list`

### 作用

获取员工列表，用于员工管理页。

### 输入

```json
{
  "page": 1,
  "pageSize": 20,
  "keyword": "张",
  "faceProfileState": "success"
}
```

### 字段说明

| 字段               | 类型     | 必填 | 说明                                                |
| ------------------ | -------- | ---- | --------------------------------------------------- |
| `page`             | `number` | 否   | 默认 `1`                                            |
| `pageSize`         | `number` | 否   | 默认 `20`，最大 `100`                               |
| `keyword`          | `string` | 否   | 按员工编号或姓名模糊搜索                            |
| `faceProfileState` | `string` | 否   | `pending`、`success`、`failed`、`cancelled`、`none` |

### 输出

```json
{
  "items": [
    {
      "id": "emp_001",
      "code": "20230001",
      "name": "张三",
      "createdAt": 1743158400000,
      "updatedAt": 1743158400000,
      "faceProfile": null
    }
  ],
  "pageInfo": {
    "page": 1,
    "pageSize": 20,
    "total": 1,
    "totalPages": 1
  }
}
```

### 处理规则

- 固定按 `createdAt desc` 排序
- `keyword` 同时匹配 `code` 和 `name`
- `faceProfileState = none` 表示筛选当前没有录脸记录的员工

---

## 3. `employee.create`

### 作用

创建员工。

### 输入

```json
{
  "code": "20230001",
  "name": "张三"
}
```

### 输入规则

- `code` 必填，去除首尾空格后不能为空
- `name` 必填，去除首尾空格后不能为空
- `code` 必须唯一

### 输出

返回创建后的 `EmployeeSummary`，其中 `faceProfile` 固定为 `null`。

### 可能错误

- `EMPLOYEE_CODE_CONFLICT`

---

## 4. `employee.update`

### 作用

更新员工信息。

### 输入

```json
{
  "id": "emp_001",
  "code": "20230001",
  "name": "张三"
}
```

### 输入规则

- `id` 必填
- `code` 必填，去除首尾空格后不能为空
- `name` 必填，去除首尾空格后不能为空
- 若修改后的 `code` 与其他员工重复，则报错

### 输出

返回更新后的 `EmployeeSummary`。

### 可能错误

- `EMPLOYEE_NOT_FOUND`
- `EMPLOYEE_CODE_CONFLICT`

---

## 5. `employee.getDeleteImpact`

### 作用

获取删除员工前的影响摘要，用于二次确认弹窗。

### 输入

```json
{
  "id": "emp_001"
}
```

### 输出

```json
{
  "id": "emp_001",
  "code": "20230001",
  "name": "张三",
  "faceProfileCount": 1,
  "attendanceRecordCount": 12,
  "confirmText": "20230001"
}
```

### 处理规则

- 若员工不存在，直接报错
- `confirmText` 固定返回员工编号，前端用于二次确认输入
- 影响统计至少包含关联录脸任务数量与考勤记录数量

### 可能错误

- `EMPLOYEE_NOT_FOUND`

---

## 6. `employee.remove`

### 作用

删除一个员工，并级联清理其关联录脸任务和考勤记录。

### 输入

```json
{
  "id": "emp_001",
  "confirmText": "20230001"
}
```

### 输入规则

- `id` 必填
- `confirmText` 必填
- `confirmText` 必须与当前员工编号完全一致

### 输出

```json
{
  "id": "emp_001",
  "deletedFaceProfileCount": 1,
  "deletedAttendanceRecordCount": 12
}
```

### 处理规则

- 删除前先校验员工存在
- 删除操作在事务中执行
- 先删除该员工关联的考勤记录与录脸任务，再删除员工本体

### 可能错误

- `EMPLOYEE_NOT_FOUND`
- `EMPLOYEE_DELETE_CONFIRMATION_MISMATCH`

---

## 7. `device.list`

### 作用

获取设备列表，用于设备管理页。

### 输入

```json
{
  "page": 1,
  "pageSize": 20,
  "keyword": "一号",
  "status": "active"
}
```

### 字段说明

| 字段       | 类型     | 必填 | 说明                       |
| ---------- | -------- | ---- | -------------------------- |
| `page`     | `number` | 否   | 默认 `1`                   |
| `pageSize` | `number` | 否   | 默认 `20`                  |
| `keyword`  | `string` | 否   | 按设备名称或设备码模糊搜索 |
| `status`   | `string` | 否   | `active` 或 `disabled`     |

### 输出

```json
{
  "items": [
    {
      "id": "dev_001",
      "deviceCode": "DEV-001",
      "name": "一号设备",
      "status": "active",
      "lastSeenAt": null,
      "createdAt": 1743158400000,
      "updatedAt": 1743158400000
    }
  ],
  "pageInfo": {
    "page": 1,
    "pageSize": 20,
    "total": 1,
    "totalPages": 1
  }
}
```

### 处理规则

- 固定按 `createdAt desc` 排序
- 列表接口不返回明文 `apiKey`

---

## 8. `device.create`

### 作用

创建设备，并生成设备首配所需的 bootstrap 凭据。

### 输入

```json
{
  "name": "一号设备"
}
```

### 输入规则

- `name` 必填，去除首尾空格后不能为空
- 服务端自动生成唯一 `deviceCode`
- 服务端自动生成 `bootstrapSerial`
- 服务端自动生成 `bootstrapSecret`
- 数据库预先写入占位 `apiKeyHash`，设备完成 bootstrap 激活后再换发正式运行时凭据

### 输出

```json
{
  "device": {
    "id": "dev_001",
    "deviceCode": "DEV-001",
    "name": "一号设备",
    "status": "active",
    "lastSeenAt": null,
    "createdAt": 1743158400000,
    "updatedAt": 1743158400000
  },
  "bootstrapSerial": "BOOT-001",
  "bootstrapSecret": "bootstrap-secret"
}
```

### 输出规则

- `bootstrapSerial` 与 `bootstrapSecret` 只在本次创建设备成功时返回
- 正式运行时 `apiKey` 只在设备 bootstrap 激活成功时下发给设备，不在管理端展示

---

## 9. `device.update`

### 作用

更新设备名称或启用状态。

### 输入

```json
{
  "id": "dev_001",
  "name": "前台设备",
  "status": "disabled"
}
```

### 输入规则

- `id` 必填
- `name` 可选，传入时去除首尾空格后不能为空
- `status` 可选，取值为 `active` 或 `disabled`
- `name` 和 `status` 至少要提供一个
- 不允许通过该接口修改 `deviceCode`

### 输出

返回更新后的 `DeviceSummary`。

### 可能错误

- `DEVICE_NOT_FOUND`

---

## 10. `device.getDeleteImpact`

### 作用

获取删除设备前的影响摘要，用于二次确认弹窗。

### 输入

```json
{
  "id": "dev_001"
}
```

### 输出

```json
{
  "id": "dev_001",
  "deviceCode": "DEV-001",
  "name": "一号设备",
  "faceProfileCount": 2,
  "attendanceRecordCount": 36,
  "confirmText": "DEV-001"
}
```

### 处理规则

- 若设备不存在，直接报错
- `confirmText` 固定返回设备码，前端用于二次确认输入
- 影响统计至少包含关联录脸任务数量与考勤记录数量

### 可能错误

- `DEVICE_NOT_FOUND`

---

## 11. `device.findByBootstrapSerial`

### 作用

在串口配置页中，按设备当前上报的 `bootstrapSerial` 反查后台记录。

### 输入

```json
{
  "serial": "BOOT-001"
}
```

### 输出

```json
{
  "id": "dev_001",
  "deviceCode": "DEV-001",
  "name": "一号设备",
  "status": "active",
  "lastSeenAt": 1743158400000,
  "createdAt": 1743158400000,
  "updatedAt": 1743158400000
}
```

补充说明：

- 若后台没有匹配记录，则返回 `null`
- 该接口主要服务于 `/devices/serial` 页面，不返回明文 `apiKey`

---

## 12. `device.remove`

### 作用

删除一个设备，并级联清理其关联录脸任务和考勤记录。

### 输入

```json
{
  "id": "dev_001",
  "confirmText": "DEV-001"
}
```

### 输入规则

- `id` 必填
- `confirmText` 必填
- `confirmText` 必须与当前设备码完全一致

### 输出

```json
{
  "id": "dev_001",
  "deletedFaceProfileCount": 2,
  "deletedAttendanceRecordCount": 36
}
```

### 处理规则

- 删除前先校验设备存在
- 删除操作在事务中执行
- 先删除该设备关联的考勤记录与录脸任务，再删除设备本体

### 可能错误

- `DEVICE_NOT_FOUND`
- `DEVICE_DELETE_CONFIRMATION_MISMATCH`

---

## 13. `attendanceConfig.get`

### 作用

获取全局考勤配置。

### 输入

无输入。

### 输出

```json
{
  "config": {
    "id": "default",
    "workStartMinute": 540,
    "workEndMinute": 600,
    "offStartMinute": 1080,
    "offEndMinute": 1140,
    "updatedAt": 1743158400000
  }
}
```

说明：

- 若当前尚未创建配置，则返回 `{ "config": null }`

---

## 14. `attendanceConfig.save`

### 作用

创建或更新全局考勤配置。

### 输入

```json
{
  "workStartMinute": 540,
  "workEndMinute": 600,
  "offStartMinute": 1080,
  "offEndMinute": 1140
}
```

### 输入规则

- 所有字段必填
- 所有分钟值都必须在 `0-1439` 范围内
- 必须满足：
  - `workStartMinute < workEndMinute`
  - `offStartMinute < offEndMinute`
  - 两个时间段不得重叠
  - 两个时间段不得相互包含
  - 不允许跨天
- 服务端使用固定主键，例如 `id = "default"`，执行单例 upsert

### 输出

```json
{
  "config": {
    "id": "default",
    "workStartMinute": 540,
    "workEndMinute": 600,
    "offStartMinute": 1080,
    "offEndMinute": 1140,
    "updatedAt": 1743158400000
  }
}
```

### 可能错误

- `ATTENDANCE_CONFIG_INVALID_MINUTE`
- `ATTENDANCE_CONFIG_INVALID_RANGE`
- `ATTENDANCE_CONFIG_OVERLAPPED`

---

## 15. `faceProfile.list`

### 作用

获取录脸记录列表，包括待处理任务和已完成结果。

### 输入

```json
{
  "page": 1,
  "pageSize": 20,
  "status": "pending",
  "employeeId": "emp_001",
  "deviceId": "dev_001"
}
```

### 字段说明

| 字段         | 类型     | 必填 | 说明                                        |
| ------------ | -------- | ---- | ------------------------------------------- |
| `page`       | `number` | 否   | 默认 `1`                                    |
| `pageSize`   | `number` | 否   | 默认 `20`                                   |
| `status`     | `string` | 否   | `pending`、`success`、`failed`、`cancelled` |
| `employeeId` | `string` | 否   | 按员工筛选                                  |
| `deviceId`   | `string` | 否   | 按设备筛选                                  |

### 输出

```json
{
  "items": [
    {
      "id": "fp_001",
      "employeeId": "emp_001",
      "deviceId": "dev_001",
      "status": "pending",
      "createdAt": 1743158400000,
      "updatedAt": 1743158400000,
      "employee": {
        "id": "emp_001",
        "code": "20230001",
        "name": "张三"
      },
      "device": {
        "id": "dev_001",
        "name": "一号设备",
        "deviceCode": "DEV-001",
        "status": "active"
      }
    }
  ],
  "pageInfo": {
    "page": 1,
    "pageSize": 20,
    "total": 1,
    "totalPages": 1
  }
}
```

### 处理规则

- 固定按 `updatedAt desc` 排序
- 当前每个员工最多只有一条 `face_profile` 记录

---

## 16. `faceProfile.enqueue`

### 作用

为员工创建录脸任务，或把员工重新分配到指定设备并重置为 `pending`。

### 输入

```json
{
  "employeeId": "emp_001",
  "deviceId": "dev_001"
}
```

### 处理规则

1. 校验员工存在
2. 校验设备存在
3. 校验设备状态为 `active`
4. 校验目标设备当前不存在其他员工的 `pending` 任务
5. 若该员工已有 `face_profile`，则覆盖其 `deviceId`、`status` 和 `updatedAt`
6. 若该员工没有 `face_profile`，则创建一条新记录
7. 无论原状态是 `success`、`failed` 还是 `cancelled`，本次操作后都重置为 `pending`

### 输出

返回更新后的 `FaceProfileSummary`。

### 可能错误

- `EMPLOYEE_NOT_FOUND`
- `DEVICE_NOT_FOUND`
- `DEVICE_DISABLED`
- `DEVICE_PENDING_TASK_EXISTS`

### 补充说明

- 当前“重新发起失败任务”复用本接口
- 当前“重新分配设备录脸”也复用本接口

---

## 17. `faceProfile.cancel`

### 作用

取消一个待处理的录脸任务。

### 输入

```json
{
  "id": "fp_001"
}
```

### 处理规则

- 只有 `pending` 状态的任务允许取消
- 取消后状态更新为 `cancelled`
- 若设备之后继续上报该任务结果，设备端接口会收到 `TASK_CANCELLED`

### 输出

返回取消后的 `FaceProfileSummary`。

### 可能错误

- `FACE_PROFILE_NOT_FOUND`
- `FACE_PROFILE_NOT_PENDING`

---

## 18. `attendanceRecord.list`

### 作用

获取考勤记录列表，用于考勤记录页。

### 输入

```json
{
  "page": 1,
  "pageSize": 20,
  "localDate": "2026-03-28",
  "employeeId": "emp_001",
  "deviceId": "dev_001",
  "type": "clock_in",
  "slotStatus": "missing"
}
```

### 字段说明

| 字段         | 类型     | 必填 | 说明                          |
| ------------ | -------- | ---- | ----------------------------- |
| `page`       | `number` | 否   | 默认 `1`                      |
| `pageSize`   | `number` | 否   | 默认 `20`                     |
| `localDate`  | `string` | 否   | 按单日筛选，格式 `YYYY-MM-DD` |
| `employeeId` | `string` | 否   | 按员工筛选                    |
| `deviceId`   | `string` | 否   | 按设备筛选                    |
| `type`       | `string` | 否   | 关注的时段，`clock_in` 或 `clock_out` |
| `slotStatus` | `string` | 否   | `recorded` 或 `missing` |

### 输出

```json
{
  "items": [
    {
      "id": "emp_001:2026-03-28",
      "employeeId": "emp_001",
      "localDate": "2026-03-28",
      "latestRecognizedAt": 1743190800000,
      "employee": {
        "id": "emp_001",
        "code": "20230001",
        "name": "张三"
      },
      "clockIn": {
        "id": "ar_001",
        "deviceId": "dev_001",
        "recognizedAt": 1743158400000,
        "type": "clock_in",
        "device": {
          "id": "dev_001",
          "name": "一号设备",
          "deviceCode": "DEV-001"
        }
      },
      "clockInStatus": "recorded",
      "clockOut": {
        "id": "ar_002",
        "deviceId": "dev_001",
        "recognizedAt": 1743190800000,
        "type": "clock_out",
        "device": {
          "id": "dev_001",
          "name": "一号设备",
          "deviceCode": "DEV-001"
        }
      },
      "clockOutStatus": "recorded"
    }
  ],
  "pageInfo": {
    "page": 1,
    "pageSize": 20,
    "total": 1,
    "totalPages": 1
  }
}
```

### 处理规则

- 返回按 `employeeId + localDate` 聚合后的日考勤行，一行同时包含 `clockIn` 与 `clockOut`
- `clockIn` 或 `clockOut` 不存在时返回 `null`
- `clockInStatus` 和 `clockOutStatus` 取值为 `recorded`、`missing` 或 `null`
- 当对应记录为空时，若已过时间段或历史日期则为 `missing`
- 当对应记录为空且不应判定缺卡时返回 `null`
- 固定按 `localDate desc`、当天最新 `recognizedAt desc` 排序
- `localDate` 为单日筛选，不提供区间筛选；月度汇总使用 `attendanceRecord.monthlySummary`
- `slotStatus` 筛选表示只返回包含该状态的日考勤行
- 同时传入 `type` 与 `slotStatus` 时，只匹配指定时段的状态，例如筛选“下班缺卡”
- 只传入 `slotStatus` 时，只要上班或下班任一时段命中该状态即返回该日考勤行

---

## 19. `attendanceRecord.monthlySummary`

### 作用

按月份汇总员工考勤，用于考勤记录页的月度统计区域。

### 输入

```json
{
  "month": "2026-03",
  "employeeId": "emp_001"
}
```

### 字段说明

| 字段         | 类型     | 必填 | 说明                          |
| ------------ | -------- | ---- | ----------------------------- |
| `month`      | `string` | 是   | 统计月份，格式 `YYYY-MM`      |
| `employeeId` | `string` | 否   | 按员工筛选                    |

### 输出

```json
{
  "month": "2026-03",
  "range": {
    "start": "2026-03-01",
    "end": "2026-04-01"
  },
  "items": [
    {
      "employee": {
        "id": "emp_001",
        "code": "20230001",
        "name": "张三"
      },
      "activeDays": 2,
      "clockInCount": 2,
      "clockOutCount": 1,
      "missingClockInCount": 0,
      "missingClockOutCount": 1,
      "days": [
        {
          "localDate": "2026-03-28",
          "clockInStatus": "recorded",
          "clockOutStatus": "missing"
        }
      ]
    }
  ],
  "totals": {
    "employeeCount": 1,
    "activeDays": 2,
    "clockInCount": 2,
    "clockOutCount": 1,
    "missingClockInCount": 0,
    "missingClockOutCount": 1
  }
}
```

### 处理规则

- 按 `month` 查询 `localDate >= YYYY-MM-01` 且 `< nextMonth-01` 的考勤记录
- 返回每名员工的月度统计，员工按编号升序排列
- `activeDays` 只统计本月已有至少一条考勤记录的员工日期
- 不生成完整自然月日历，不推断无任何记录日期的缺卡
- 缺卡仅在某员工某日已有上班或下班任一记录时，对另一时段按全局考勤配置判断
- 当天未结束的时段不计缺卡
- 当前 MVP 不处理工作日/休息日规则，因此月度统计是“基于已有记录的月度汇总”，不是完整排班考勤报表

---

## 页面与接口对应关系

| 页面       | 主要接口                                                                                                  |
| ---------- | --------------------------------------------------------------------------------------------------------- |
| Dashboard  | `dashboard.summary`                                                                                       |
| 员工管理页 | `employee.list`、`employee.create`、`employee.update`、`employee.getDeleteImpact`、`employee.remove`     |
| 设备管理页 | `device.list`、`device.create`、`device.update`、`device.getDeleteImpact`、`device.findByBootstrapSerial`、`device.remove` |
| 考勤配置页 | `attendanceConfig.get`、`attendanceConfig.save`                                                           |
| 录脸记录页 | `faceProfile.list`、`faceProfile.enqueue`、`faceProfile.cancel`                                           |
| 考勤记录页 | `attendanceRecord.list`、`attendanceRecord.monthlySummary`（同一套筛选与结果区域，通过查询方式切换）       |

---

## 当前不做

- `apiKey` 重置接口
- 批量导入员工接口
- 批量操作录脸任务接口
- 考勤记录导出接口
- 多管理员权限区分
