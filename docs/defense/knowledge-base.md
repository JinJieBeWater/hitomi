# 答辩知识库 🇪🇸 人脸识别考勤系统

此文档按概念体系组织，不讲问答格式。供你回忆起整体项目后，再按问答形式准备。

---

## 一、硬件芯片：ESP32-S3

### 芯片规格
- **架构**：Xtensa® dual-core 32-bit LX7 @240MHz
- **AI 加速**：Vector Extension（SIMD 向量指令），单周期可处理 128-bit 数据（= 16 个 int8 乘加）
- **内存**：512KB SRAM + 8MB Octal PSRAM（本项目 SZPI 板）
- **Flash**：16MB 外挂 Flash
- **无线**：Wi-Fi 4 (2.4GHz 802.11 b/g/n) + BLE 5.0
- **外设**：DVP 摄像头接口、SPI、I2C、SD/MMC、USB OTG（CDC 串口）

### 为什么选 ESP32-S3
1. Vector 指令直接加速 int8 量化模型推理——检测 5-6x 加速，识别 6.25x 加速（相对初代 ESP32）
2. 外设正好覆盖考勤终端需求：摄像头 + 屏幕 + 触摸 + SD 卡 + Wi-Fi
3. 成本 ~50 元，功耗 <1W，启动 <1 秒

---

## 二、人脸识别技术栈（逐层往下拆）

### 框架层级

```
ESP-WHO（应用框架：人脸检测/识别/行人检测/QROCode 的应用级封装）
  └── ESP-DL（深度学习推理引擎：模型加载、int8 推理、图像预处理、数学运算）
       └── ESP-NN（底层算子：卷积、激活、池化等 SIMD 优化实现）
            └── ESP32-S3 Vector Extension（硬件指令）
```

**这个项目用的是 ESP-WHO 的 Arduino 封装类**（来自 `arduino-esp32` BSP），不是直接用 ESP-IDF C API：
- `HumanFaceDetect` → 封装 ESP-DL 的人脸检测模型
- `HumanFaceFeat` → 封装 ESP-DL 的特征提取模型
- `HumanFaceRecognizer` → 封装 ESP-DL 的识别器（特征提取 + 数据库管理）

### 模型列表

#### 人脸检测模型（Human Face Detection）

| 模型名 | 类型 | 输入尺寸 | ESP32-S3 延迟 | 精度 (mAP) |
|---|---|---|---|---|
| **MSR_S8_V1 + MNP_S8_V1** | 两阶段 | 120×160 + 48×48 | 32.4ms + 5.6ms = ~38ms | 0.367 |
| ESPDET_PICO_224 | 单阶段 | 224×224×3 | 122.5ms | 0.495 |
| ESPDET_PICO_416 | 单阶段 | 416×416×3 | 436.4ms | 0.598 |

**本项目使用的**：`CONFIG_DEFAULT_HUMAN_FACE_DETECT_MODEL` → MSR_S8_V1 + MNP_S8_V1（两阶段，最快）。

**MSR + MNP 原理**：
- **MSR（Multi-Scale Region）**：第一阶段在 120×160 图像上快速扫描，生成"候选人脸区域"候选框
- **MNP（Multi-task Neural Processing）**：第二阶段对每个候选区域（48×48）做精细判定 + 关键点回归——最终输出人脸框 + 5 个关键点（两眼、鼻尖、两嘴角）

#### 人脸识别模型（Feature Extraction）

| 模型名 | 全称 | 参数量 | GFLOPs | 输入 | ESP32-S3 延迟 | 精度 (TAR@FAR=1E-4 on IJB-C) |
|---|---|---|---|---|---|---|
| **MFN_S8_V1** | MobileFaceNet S8 V1 | 1.2M | 0.46 | 112×112×3 | 248.8ms | 90.03% |
| MBF_S8_V1 | MobileFace (larger) | 3.4M | 0.90 | 112×112×3 | 1072.4ms | 93.94% |

**本项目使用的**：`CONFIG_DEFAULT_HUMAN_FACE_FEAT_MODEL` → MFN_S8_V1（轻量，速度快）。

**MFN_S8_V1 架构**（MobileFaceNet）：
- 基于 MobileNetV2 的 inverted residual block
- 使用 PReLU 激活函数（非 ReLU）
- 最后输出一个 512 维的特征向量（embedding）
- 输入：裁剪后的 112×112 人脸区域（RGB）

### 量化

所有模型均为 **S8 = Signed Int8** 量化：
- 模型权重从 float32 量化到 int8
- 推理过程中使用 int8 乘加，中间结果可累加到 int32
- **精度损失极小**（<0.5%），但推理速度提升 2.5-6x
- CPU 加载耗时：MFN_S8_V1 ~248ms（单次推理）
- 检测+识别总流水线延迟（一帧）：~38ms（检测）+ ~249ms（识别）= ~287ms
- **实际帧率**：项目内部有冷却间隔（`kFaceRecognitionCooldownMs`），识别不是每帧都跑

### 特征比对

识别输出是一个 512 维 float 向量，比对用**余弦相似度**（点积）：

```cpp
// runtime_face_engine_ops.cpp 中的实际代码
for (size_t i = 0; i < stored.size(); i++) {
    similarity += current[i] * stored[i];  // 点积 = 余弦相似度（当向量已归一化）
}
```

- 阈值：`board::kFaceRecognitionThreshold`
- 遍历所有已加载的模板，找最大相似度
- 超过阈值 → 匹配成功，返回员工 ID

### 模型存放位置

| 项目 | 位置 |
|---|---|
| 检测模型（`human_face_detect`） | SPIFFS Flash 分区（或 SD 卡） |
| 识别模型（`human_face_feat`） | SPIFFS Flash 分区（或 SD 卡） |

配置方式：`CONFIG_HUMAN_FACE_DETECT_MODEL_LOCATION` → `FLASH_PARTITION`

---

## 三、人脸录脸流程（Enrollment）

### 完整流程

```
摄像头帧（320×240 RGB565）
  → HumanFaceDetect.run()         // 检测人脸框 + 关键点
  → 检查：只有一张人脸？          // 多张人脸拒绝
  → 连续采集 N 帧（board::kEnrollmentRequiredSamples）
  → HumanFaceRecognizer.enroll()  // 批量提取特征 → 生成模板 .bin
  → 模板写入 SD 卡               // SdMmcTemplateStore::upsertTemplate()
  → 上报结果给后端               // POST /api/device/enrollment/report
```

### 关键约束
- **每次只采一张人脸**：检测到多张脸 → 拒绝，提示"检测到多张人脸"
- **需要采集 N 帧**：不是单帧出模板，而是采集 N 帧确保质量
- **模板存为二进制文件**：`/templates/{employeeId}.bin`
- **每个员工只有一个模板**：re-enrollment 覆盖旧模板
- **模板格式**：特征向量（float 数组），不是图片

### 超时保护
- 整个录脸会话有超时（`kEnrollmentSessionTimeoutMs`）
- 超时 → 自动终止并报告失败

---

## 四、数据存储（三张表 + 三类介质）

### 总览

| 数据 | 位置 | 介质 | 大小 | 断网/掉电安全性 |
|---|---|---|---|---|
| 设备身份（deviceCode, apiKey） | NVS | Flash 键值 | 百字节级 | ✅ OTA 不丢 |
| Wi-Fi profiles | NVS | Flash 键值 | KB 级 | ✅ |
| 后台地址 | NVS | Flash 键值 | 百字节级 | ✅ |
| 考勤配置快照 | LittleFS | Flash 文件 | KB 级 | ✅ |
| 员工信息摘要 | LittleFS | Flash 文件 | KB 级 | ✅ |
| 录脸任务列表 | LittleFS | Flash 文件 | KB 级 | ✅ |
| 待上传考勤队列 | LittleFS | Flash 文件 | KB 级 | ✅ |
| 本地考勤标记 | LittleFS | Flash 文件 | KB 级 | ✅ |
| 接口失败日志 | LittleFS | Flash 文件 | KB 级 (最多 200 条) | ✅ |
| 模板库版本摘要 | LittleFS | Flash 文件 | 百字节级 | ✅ |
| SD 卡状态摘要 | LittleFS | Flash 文件 | 百字节级 | ✅ |
| 人脸模板 | SD Card | FAT 文件系统 | MB 级（每人一份 .bin） | ✅ |
| 模型文件（检测+识别） | SPIFFS/Flash | Flash 分区 | MB 级 | ✅ 只读 |

### 三层存储的降级策略

```
启动时：
  NVS          → 失败 → 设备无法工作（缺少身份凭证）     【阻断】
  LittleFS     → 失败 → 允许降级启动（本地状态不持久化） 【降级】
  SD Card      → 失败 → 允许启动（禁用识别功能）        【降级】
```

### NVS vs LittleFS vs SD Card 的职责划分

- **NVS**：只存最核心的，不能依赖任何文件系统挂载——设备身份、Wi-Fi 凭据、bootstrap 身份。这是设备的"根信任"。
- **LittleFS**：存设备"最小运行态"——每次 sync 下来的配置快照、待上传队列。Flash 文件系统，擦写次数有限（~10 万次），但容量够用。
- **SD Card**：存容量型大数据——人脸模板。GB 级容量，可插拔，独立于 Flash。

### 服务端存储

| 数据 | 存 | 不存 |
|---|---|---|
| 员工信息 | ✅ (employee 表) | — |
| 设备信息 | ✅ (device 表, apiKey 只存 hash) | 明文 apiKey |
| 考勤配置 | ✅ (attendance_config 单例表) | — |
| 录脸任务状态 | ✅ (face_profile 表) | 人脸照片、特征向量 |
| 考勤记录 | ✅ (attendance_record 表) | 原始识别图像 |
| 人脸模板 | ❌ | ✅ |
| 人脸照片 | ❌ | ✅ |

---

## 五、通信协议

### 管理端 ↔ 后端：oRPC

```
[浏览器 Nuxt App]
  → oRPC client (HTTP POST)
  → Nitro server
  → oRPC router (employee.list / device.create / attendanceRecord.list ...)
  → Better Auth middleware (session 校验)
  → Drizzle ORM → libSQL/SQLite
  → 返回 JSON
```

**特点**：
- TypeScript 端到端类型安全（client → server → db 字段都有类型推导）
- 19 个管理端接口（`employee.list`, `employee.create`, `device.create`, `faceProfile.enqueue`, `attendanceRecord.list`, `attendanceRecord.monthlySummary` 等）
- 鉴权：Better Auth session cookie

### 终端 ↔ 后端：HTTP JSON

```
[ESP32-S3]
  → WiFiClient HTTP POST
  → JSON body: { deviceCode, apiKey, ... }
  → Nitro handler: POST /api/device/*
  → 鉴权：查 device → 比对 apiKey ↔ api_key_hash
  → 业务处理
  → 返回 JSON: { success: true/false, data/error }
```

**5 个设备端接口**：

| 接口 | 路径 | 作用 |
|---|---|---|
| Bootstrap Hello | `POST /api/device/bootstrap/hello` | 设备用 bootstrap 凭据验证身份 |
| Bootstrap Claim | `POST /api/device/bootstrap/claim-status` | 领取正式 apiKey 明文 |
| Sync | `POST /api/device/sync` | 拉配置 + 员工 + 录脸任务（全量返回） |
| Enrollment Report | `POST /api/device/enrollment/report` | 上报录脸结果 |
| Attendance Upload | `POST /api/device/attendance/upload` | 批量上传考勤记录 |

**响应格式**：
```json
// 成功
{ "success": true, "data": { ... } }

// 失败
{ "success": false, "error": { "code": "DEVICE_AUTH_FAILED", "message": "...", "retryable": false } }
```

**设备按 `error.code` 做逻辑判断，不依赖 message 字符串。**

### 配网阶段：Web Serial（USB CDC）

```
[浏览器 /devices/serial 页面]
  → navigator.serial.requestPort()    // 弹出串口选择器
  → port.open({ baudRate: 115200 })
  → writer.write(JSON_CMD + "\n")     // 通过 USB 发 JSON
  → reader.read()                     // 读 HITOMI_USB_RESPONSE {...}
```

**协议**：换行分隔 JSON，ESP32 响应前缀 `HITOMI_USB_RESPONSE `（空格结尾）。

**命令集**：
| 命令 | 作用 |
|---|---|
| `{"type":"get_config"}` | 获取当前配置 |
| `{"type":"set_wifi_profiles","profiles":[...]}` | 设置 Wi-Fi 列表 |
| `{"type":"set_backend_origin","origin":"http://..."}` | 设置后台地址 |
| `{"type":"set_bootstrap_identity","deviceSerial":"...","bootstrapSecret":"..."}` | 下发激活凭据 |
| `{"type":"clear_runtime_credentials"}` | 清除运行时凭证 |
| `{"type":"clear_wifi_profiles"}` | 清除 Wi-Fi 列表 |
| `{"type":"reset_device_config"}` | 恢复出厂（清 NVS + SD 模板） |

---

## 六、考勤判定逻辑

### 时间段判定（设备端 + 服务端同一逻辑）

考勤配置示例：
```
上班 [09:00, 10:00) = 分钟 [540, 600)
下班 [18:00, 19:00) = 分钟 [1080, 1140)
```

判定函数（`core::classifyAttendanceType`）：
```
currentMinute = hour * 60 + minute
if 540 ≤ currentMinute < 600 → clock_in
if 1080 ≤ currentMinute < 1140 → clock_out
else → 不在考勤时段
```

### 去重策略

**设备端（本地考勤标记）**：
- key：`employeeId + localDate + type`
- 同一天上午被识别多次 → 只有第一次写入待上传队列，后续显示"今日上班已打卡"

**服务端（数据库约束）**：
- 唯一索引：`(employee_id, local_date, type)`
- 后上传的更早时间 → 更新为更早
- 后上传的更晚或相同 → 忽略（`ignored_duplicate`）

### 缺卡判定

在考勤记录页面上做日汇总时：
- 某员工某日有 `clock_in` 但无 `clock_out` → 下班缺卡（前提：时间段已结束/为历史日期）
- 某员工某日有 `clock_out` 但无 `clock_in` → 上班缺卡
- 当天未结束的时段 → 不计缺卡

---

## 七、设备激活（Bootstrap）流程

```
1. 管理员在后台创建「设备」
   → 服务端生成：deviceCode, apiKey (存 hash), bootstrapSerial, bootstrapSecret
   → 前端一次性展示 bootstrapSerial + bootstrapSecret（明文）

2. 管理员通过 /devices/serial 页面的 Web Serial 配网
   → 下发 Wi-Fi 凭据
   → 下发 bootstrapSerial + bootstrapSecret

3. 设备连上 Wi-Fi → POST /api/device/bootstrap/hello
   → 验证 bootstrap 凭据有效

4. 设备调 POST /api/device/bootstrap/claim-status
   → 服务端下发正式 apiKey 明文
   → 服务端清除暂存的 bootstrap 凭据

5. 设备收到 apiKey → 写入本地 NVS
   → 后续所有业务接口用 deviceCode + apiKey
```

---

## 八、系统技术栈总览

| 层面 | 技术 | 作用 |
|---|---|---|
| 终端芯片 | ESP32-S3 | 人脸识别考勤终端 |
| 终端 OS | FreeRTOS (ESP-IDF 框架) | 多任务分核心调度（见下方任务分配） |
| 终端 AI | ESP-DL (int8 推理) + ESP-WHO | 人脸检测 MSR+MNP、人脸识别 MFN(MobileFaceNet) |
| 终端 UI | LVGL 9.5.0 | 触摸屏界面（ST7789 SPI LCD + FT6336 触摸） |
| 终端存储 | NVS + LittleFS + SD Card (FAT) | 三层持久化 |
| 终端模板库 | ESP-WHO HumanFaceRecognizer | 特征提取 + SD 卡模板管理 |
| 后端运行时 | Bun + Nitro (Nuxt 4) | HTTP server |
| 后端 API | oRPC + 原生 HTTP POST | 管理端 RPC + 设备端 REST |
| 数据库 | libSQL (SQLite) + Drizzle ORM | 数据持久化 |
| 前端框架 | Nuxt 4 + Vue 3 | SPA 管理后台 |
| 前端 CSS | TailwindCSS | 样式 |
| 认证 | Better Auth | session-based 登录 |
| 串口配网 | Web Serial API (Chrome) | 浏览器 USB 串口管理 |

---

## 九、核心工程模式

### 终端架构：Ports & Adapters（六边形架构）

```
app/      → 应用编排层（AppRuntime tick 主循环）
core/     → 纯领域逻辑（无硬件依赖，native-testable）
infra/    → 硬件适配层（display, camera, storage, face, network 的具体实现）
face/     → 人脸服务端口（抽象接口）
ui/       → 展示层（StatusScreenPresenter）
board/    → 板级常量（引脚、阈值）
```

**好处**：`core/` 可在主机上跑 `platformio test -e native` 单元测试，不依赖 ESP32 硬件。

### 后端架构：Monorepo 分层

```
apps/web/         → Nuxt 4 (CSR) — 23 个 Vue 组件
packages/api/     → 7 个 oRPC domain router + 设备 HTTP handler
packages/db/      → Drizzle ORM — 9 张表 (4 auth + 5 business)
packages/auth/    → Better Auth 单文件配置
packages/env/     → t3-env — 4 个子路径导出
```

### 异步识别：FreeRTOS 任务分配（从代码确认）

```
core 0 ── 重活异步任务（不阻塞界面）
  ├── face_recognition 任务   → 人脸特征提取 + 1:N 比对（~249ms 一次）
  ├── face_enrollment 任务    → 录脸采集帧处理
  ├── HTTP executor 任务      → 发 HTTP 请求/等响应（网络 IO）
  └── WiFi 驱动事件           → 连接/断开/扫描回调

core 1 ── 主循环（hitomi_runtime 任务，优先级 1）
  ├── LVGL 界面渲染           → 触摸屏刷新
  ├── 人脸检测（inline）      → MSR+MNP ~38ms，在主 tick 里直接跑
  ├── 摄像头帧轮询
  ├── USB 串口配网命令处理
  └── 业务逻辑编排
```

**检测在主循环（core 1），识别在异步任务（core 0）。**

流程：摄像头帧 → core 1 检测到人脸 → 把帧丢进互斥锁保护的槽位 → core 0 的 face_recognition 任务取走跑特征提取+比对 → 结果回到 core 1 消费，决定是否生成打卡记录。

**为什么这样分**：
- 检测快（~38ms），在 core 1 主 tick 中跑不卡 UI
- 识别慢（~249ms），如果也在 core 1 跑，屏幕会冻结 ~249ms——用户能感知
- 分到 core 0 后，core 1 继续每秒刷屏，core 0 默默算人脸特征，互不干扰
