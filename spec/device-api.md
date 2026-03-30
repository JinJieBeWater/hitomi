# 设备端 HTTP 接口设计

## 文档目标

本文档定义当前毕设 MVP 考勤系统中设备端 HTTP JSON API 的请求结构、响应结构、错误码和业务处理规则。

本文档用于指导以下实现：

- 服务端设备接口实现
- ESP32 设备端接口调用实现
- 设备端失败原因展示和日志记录

---

## 适用范围

本文档仅覆盖以下 3 个接口：

- `POST /api/device/sync`
- `POST /api/device/enrollment/report`
- `POST /api/device/attendance/upload`

当前文档遵循以下 MVP 前提：

- 单组织
- 单服务
- 单数据库
- 单管理员
- 单设备演示场景
- 默认使用 `Asia/Shanghai`
- 当前 MVP 不额外校验员工与设备绑定关系

---

## 通用约定

### 1. 传输协议

- 所有设备端接口均使用 HTTP `POST`
- 请求和响应均使用 `application/json`
- 字符编码固定为 `UTF-8`

### 2. 鉴权方式

所有设备端接口都在请求体根级携带以下字段：

- `deviceCode`: 设备码
- `apiKey`: 设备密钥明文

服务端处理方式：

- 通过 `deviceCode` 查找设备
- 使用 `apiKey` 与 `api_key_hash` 校验
- 鉴权成功后由服务端确定当前 `deviceId`

当前约束：

- 设备端不需要额外传 `deviceId`
- `apiKey` 仅在创建设备时生成一次
- 设备禁用后，3 个接口都返回 `DEVICE_DISABLED`

### 3. 时间字段

所有时间字段统一使用 Unix 毫秒时间戳，类型为 `integer`。

说明：

- 设备上传的 `recognizedAt` 和 `finishedAt` 都是毫秒时间戳
- 服务端固定按 `Asia/Shanghai` 解释考勤时间段和 `local_date`
- 上下班时间段统一按 `[start, end)` 规则判定

### 4. 响应包结构

#### 成功响应

```json
{
  "success": true,
  "data": {}
}
```

#### 失败响应

```json
{
  "success": false,
  "error": {
    "code": "DEVICE_AUTH_FAILED",
    "message": "deviceCode or apiKey is invalid",
    "retryable": false
  }
}
```

说明：

- 设备端需要优先依赖 `error.code` 做逻辑判断
- `message` 主要用于日志和调试
- `retryable` 用于提示设备是否适合自动重试

### 5. HTTP 状态码约定

- `200`: 请求已被服务端正常处理，业务成功或业务失败都通过响应体表达
- `400`: JSON 结构错误或缺少必填字段，服务端无法继续处理
- `500`: 服务端内部异常

当前 MVP 为简化设备实现：

- 业务拒绝尽量返回 `200`
- 设备应始终解析响应体中的 `success`、`data`、`error`

---

## 公共错误码

以下错误码可在多个设备接口中复用：

| code | 含义 | retryable | 说明 |
| --- | --- | --- | --- |
| `DEVICE_AUTH_FAILED` | 设备鉴权失败 | `false` | `deviceCode` 或 `apiKey` 不正确 |
| `DEVICE_DISABLED` | 设备已禁用 | `false` | 后台已禁用当前设备 |
| `INVALID_REQUEST` | 请求参数非法 | `false` | 字段缺失、类型错误、枚举值非法 |
| `ATTENDANCE_CONFIG_MISSING` | 尚未配置考勤时间段 | `false` | 后台还未保存全局考勤配置 |
| `EMPLOYEE_NOT_FOUND` | 员工不存在 | `false` | 上传的 `employeeId` 无效 |
| `TASK_CANCELLED` | 录脸任务已取消 | `false` | 后台已取消当前任务 |
| `ENROLLMENT_TASK_NOT_FOUND` | 录脸任务不存在 | `false` | 指定 `taskId` 无效 |
| `ENROLLMENT_TASK_MISMATCH` | 录脸任务与员工不匹配 | `false` | `taskId` 与 `employeeId` 不一致 |
| `ATTENDANCE_NOT_IN_WINDOW` | 不在有效打卡时间段 | `false` | `recognizedAt` 不落在对应时间段内 |
| `ATTENDANCE_DUPLICATE_LATER_OR_EQUAL` | 已存在更早或相同时间记录 | `false` | 本次记录不应覆盖原记录 |
| `INTERNAL_ERROR` | 服务端内部错误 | `true` | 设备可稍后重试 |

---

## 公共数据结构

### 1. EmployeeBrief

```json
{
  "id": "emp_001",
  "code": "20230001",
  "name": "张三",
  "updatedAt": 1743158400000
}
```

字段说明：

- `id`: 员工主键
- `code`: 员工编号
- `name`: 员工姓名
- `updatedAt`: 最近更新时间

### 2. AttendanceConfig

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

### 3. EnrollmentTask

```json
{
  "taskId": "fp_001",
  "employeeId": "emp_001",
  "employeeCode": "20230001",
  "employeeName": "张三",
  "status": "pending",
  "createdAt": 1743158400000,
  "updatedAt": 1743158400000
}
```

说明：

- 当前 MVP 中每台设备最多只会收到 1 个 `pending` 任务
- 若没有待处理任务，则返回 `null`

---

## 1. `POST /api/device/sync`

### 作用

设备启动后或定时调用，用于拉取最新工作数据。

### 请求体

```json
{
  "deviceCode": "DEV-001",
  "apiKey": "plain-api-key"
}
```

### 请求规则

- `deviceCode` 必填
- `apiKey` 必填
- 当前 MVP 不要求设备传版本号或增量同步游标
- 当前 MVP 统一返回全量配置、全量员工信息和当前待处理录脸任务

### 成功响应

```json
{
  "success": true,
  "data": {
    "serverTime": 1743158400000,
    "timezone": "Asia/Shanghai",
    "device": {
      "id": "dev_001",
      "name": "一号设备",
      "status": "active"
    },
    "attendanceConfig": {
      "id": "default",
      "workStartMinute": 540,
      "workEndMinute": 600,
      "offStartMinute": 1080,
      "offEndMinute": 1140,
      "updatedAt": 1743158400000
    },
    "employees": [
      {
        "id": "emp_001",
        "code": "20230001",
        "name": "张三",
        "updatedAt": 1743158400000
      }
    ],
    "enrollmentTask": {
      "taskId": "fp_001",
      "employeeId": "emp_001",
      "employeeCode": "20230001",
      "employeeName": "张三",
      "status": "pending",
      "createdAt": 1743158400000,
      "updatedAt": 1743158400000
    }
  }
}
```

### 字段说明

- `serverTime`: 服务端当前时间，设备可用于日志或本地校时参考
- `timezone`: 固定返回 `Asia/Shanghai`
- `device.status`: 当前仅有 `active` 或 `disabled`
- `attendanceConfig`: 若后台尚未保存考勤配置，则返回 `null`
- `employees`: 当前设备本地识别所需的员工全量列表
- `enrollmentTask`: 当前待处理录脸任务，没有则返回 `null`

### 失败响应

可能返回以下错误码：

- `DEVICE_AUTH_FAILED`
- `DEVICE_DISABLED`
- `INTERNAL_ERROR`

### 设备端处理建议

- 调用成功后，设备应覆盖本地配置、员工信息和当前录脸任务
- 若 `attendanceConfig` 为 `null`，设备不应上传考勤记录
- 若 `enrollmentTask` 为 `null`，设备应清空当前待录脸任务状态

---

## 2. `POST /api/device/enrollment/report`

### 作用

设备在完成一次录脸任务后上报结果。

### 请求体

```json
{
  "deviceCode": "DEV-001",
  "apiKey": "plain-api-key",
  "taskId": "fp_001",
  "employeeId": "emp_001",
  "result": "success",
  "finishedAt": 1743158400000,
  "failureReason": null
}
```

### 请求字段说明

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `deviceCode` | `string` | 是 | 设备码 |
| `apiKey` | `string` | 是 | 设备密钥 |
| `taskId` | `string` | 是 | 录脸任务 ID，对应 `face_profile.id` |
| `employeeId` | `string` | 是 | 当前完成录脸的员工 ID |
| `result` | `string` | 是 | `success` 或 `failed` |
| `finishedAt` | `integer` | 是 | 录脸完成时间 |
| `failureReason` | `string \| null` | 否 | 当 `result = failed` 时建议填写失败原因 |

### 请求规则

- `result = success` 时，`failureReason` 必须为空或不传
- `result = failed` 时，`failureReason` 建议传字符串
- `taskId` 与 `employeeId` 必须匹配

### 成功响应

```json
{
  "success": true,
  "data": {
    "taskId": "fp_001",
    "employeeId": "emp_001",
    "status": "success",
    "applied": true
  }
}
```

### 响应字段说明

- `status`: 服务端最终保存的状态，值为 `success` 或 `failed`
- `applied`: 是否实际更新了数据库

`applied` 处理约定：

- 首次成功处理该结果时返回 `true`
- 若设备重复上报同一终态，允许返回 `true` 或 `false`
- 当前 MVP 推荐实现为幂等处理，重复上报时返回当前终态并设置 `applied = false`

### 失败响应

可能返回以下错误码：

- `DEVICE_AUTH_FAILED`
- `DEVICE_DISABLED`
- `INVALID_REQUEST`
- `ENROLLMENT_TASK_NOT_FOUND`
- `ENROLLMENT_TASK_MISMATCH`
- `TASK_CANCELLED`
- `INTERNAL_ERROR`

### 特殊处理规则

- 若任务当前状态为 `cancelled`，服务端返回 `TASK_CANCELLED`
- 若任务不存在，服务端返回 `ENROLLMENT_TASK_NOT_FOUND`
- 若任务存在但不属于当前 `employeeId`，服务端返回 `ENROLLMENT_TASK_MISMATCH`

---

## 3. `POST /api/device/attendance/upload`

### 作用

设备联网后批量上传本地缓存的考勤记录。

### 请求体

```json
{
  "deviceCode": "DEV-001",
  "apiKey": "plain-api-key",
  "records": [
    {
      "clientRecordId": "local-001",
      "employeeId": "emp_001",
      "recognizedAt": 1743158400000,
      "type": "clock_in"
    }
  ]
}
```

### 请求字段说明

#### 根级字段

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `deviceCode` | `string` | 是 | 设备码 |
| `apiKey` | `string` | 是 | 设备密钥 |
| `records` | `array` | 是 | 待上传记录数组，不能为空 |

#### `records[]` 字段

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `clientRecordId` | `string` | 是 | 设备本地记录 ID，用于匹配响应结果 |
| `employeeId` | `string` | 是 | 员工 ID |
| `recognizedAt` | `integer` | 是 | 识别时间 |
| `type` | `string` | 是 | `clock_in` 或 `clock_out` |

### 请求规则

- `records` 至少包含 1 条记录
- 当前 MVP 建议单次最多上传 100 条记录
- 同一次请求中的 `clientRecordId` 不得重复
- 服务端通过鉴权结果确定 `deviceId`，请求体不传 `deviceId`
- 设备应按本地缓存顺序上传，服务端按请求顺序返回结果

### 成功响应

```json
{
  "success": true,
  "data": {
    "results": [
      {
        "clientRecordId": "local-001",
        "status": "saved",
        "attendanceRecordId": "ar_001",
        "error": null
      }
    ],
    "summary": {
      "saved": 1,
      "updatedEarlier": 0,
      "ignoredDuplicate": 0,
      "rejected": 0
    }
  }
}
```

### `results[].status` 枚举

| status | 含义 |
| --- | --- |
| `saved` | 新增成功 |
| `updated_earlier` | 已存在同日同类记录，但本次时间更早，已覆盖原记录 |
| `ignored_duplicate` | 已存在更早或相同时间记录，本次不入库 |
| `rejected` | 校验未通过，本次被拒绝 |

### `results[]` 字段说明

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `clientRecordId` | `string` | 原样回传 |
| `status` | `string` | 处理结果 |
| `attendanceRecordId` | `string \| null` | 新增或更新后对应的记录 ID |
| `error` | `object \| null` | 当 `status = rejected` 或 `ignored_duplicate` 时返回原因 |

### `results[].error.code` 可能值

- `EMPLOYEE_NOT_FOUND`
- `ATTENDANCE_CONFIG_MISSING`
- `ATTENDANCE_NOT_IN_WINDOW`
- `ATTENDANCE_DUPLICATE_LATER_OR_EQUAL`
- `INVALID_REQUEST`

### 处理规则

服务端逐条处理 `records`：

1. 校验员工是否存在
2. 校验是否已存在全局考勤配置
3. 基于 `recognizedAt` 和 `type` 校验时间是否合法
4. 计算 `local_date`
5. 按 `employee_id + local_date + type` 查重
6. 若不存在，则插入新记录
7. 若已存在且当前记录更早，则更新为更早时间
8. 若已存在且当前记录更晚或相同，则忽略本次记录

说明：

- 当前 MVP 不额外校验员工与设备绑定关系
- `ignored_duplicate` 不算接口失败，但设备应将本地记录视为已处理
- `rejected` 需要设备记录失败原因，是否重试由设备自行决定

### 失败响应

以下情况返回顶层失败：

- `DEVICE_AUTH_FAILED`
- `DEVICE_DISABLED`
- `INVALID_REQUEST`
- `INTERNAL_ERROR`

说明：

- 顶层失败表示本次请求中的所有记录都未进入逐条处理
- 顶层成功但 `results[]` 中存在 `rejected`，表示接口已处理完成，但部分记录未入库

---

## 设备端最小实现建议

### 1. 启动流程

1. 调用 `/api/device/sync`
2. 覆盖本地配置
3. 覆盖本地员工信息
4. 更新当前录脸任务
5. 进入识别或录脸状态

### 2. 录脸上报流程

1. 设备完成录脸后调用 `/api/device/enrollment/report`
2. 若返回 `TASK_CANCELLED`，设备清空当前录脸任务并等待下一次同步
3. 若返回成功，设备清空当前任务

### 3. 考勤上传流程

1. 设备从本地缓存读取待上传记录
2. 调用 `/api/device/attendance/upload`
3. 根据 `results[]` 逐条处理本地记录
4. `saved`、`updated_earlier`、`ignored_duplicate` 都可以从本地队列移除
5. `rejected` 是否保留由设备决定，当前 MVP 建议记录原因后移除

---

## 后续可扩展但当前不做

- 增量同步游标
- 设备端签名或时间戳防重放
- 多设备协同
- 设备与员工绑定校验
- 上传批次号
- 原始事件审计表
- 更细粒度的错误码分层
