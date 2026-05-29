# 答辩问答 — 硬件终端·人脸识别·本地存储

---

## Q1: 为什么选择 ESP32-S3 而不是树莓派或其他方案？

| 对比维度 | ESP32-S3 | 树莓派 |
|---|---|---|
| 成本 | 开发板 ~50 元 | ~300-500 元 |
| 功耗 | <1W | 3-5W |
| 启动速度 | <1 秒 | 10-30 秒 |
| 开发复杂度 | C++ / FreeRTOS | Linux + Python |
| AI 加速 | 向量指令（SIMD） | 软件推理 |

ESP32-S3 的独特优势：
- **向量扩展（Vector Extension）**：ESP-WHO 的人脸识别模型利用 ESP32-S3 的 SIMD 指令加速 `1:N` 特征比对。
- **外设丰富**：DVP 摄像头接口、SPI LCD、I2C 触摸、SD/MMC 卡槽——刚好覆盖考勤终端所需。
- **Wi-Fi + BLE 双模**：Wi-Fi 用于与后端通信，BLE 留作后续手机配网的可能性。

**不使用树莓派的原因**：对于单一考勤场景，树莓派的 Linux 系统过于重量级，启动慢、功耗高、成本高，且无法体现嵌入式开发的工程能力。

---

## Q2: 终端固件的架构是怎样的？

采用 **Ports & Adapters（六边形架构）**，严格分层：

```
app/         ── 应用编排层：AppRuntime setup/tick 主循环
core/        ── 纯领域逻辑：使用场景、数据结构、无硬件依赖 → native-testable
infra/       ── 硬件适配层：display, storage, camera, face, network 的 ESP32 实现
ui/          ── 展示层：StatusScreenPresenter → AppViewModel
face/        ── 人脸服务端口：CameraPort, EnrollmentServicePort 抽象接口
board/       ── 板级常量：引脚定义、配置默认值
```

**依赖方向**（严格自上而下）：`app/` → `core/`, `infra/`, `face/`, `ui/`；`core/` 无任何硬件依赖。

**关键好处**：
- `core/` 层代码可在 `platformio test -e native` 下纯 C++ 测试
- 换硬件平台只需替换 `infra/` 适配器，应用层不动

---

## Q3: 终端如何保存数据？三层存储分别存什么？

| 存储层 | 介质 | 存什么 | 特点 |
|---|---|---|---|
| **NVS / Preferences** | Flash 键值存储 | `deviceCode`、`apiKey`、Wi-Fi profiles、后台地址 | 最核心身份数据，不依赖文件系统挂载 |
| **LittleFS** | Flash 文件系统 | 考勤配置快照、员工摘要、录脸任务列表、待上传考勤队列、失败日志 | 设备最小运行态 |
| **SD Card** | 可插拔存储卡 | 员工人脸模板库（主存储） | 大容量、可增长、可替换 |

**分层原因**：
- NVS 是最底层的保障——即使 LittleFS 挂载失败，设备身份和 Wi-Fi 凭据还在，能联网。
- LittleFS 存快照和队列——Flash 磨损小，容量够用。
- SD 卡存人脸模板——模板量大（每人多张照片提取的特征），匹配 SD 卡的大容量和可替换性。

**降级策略**：
- SD 卡缺失 → 设备允许启动，但禁用识别功能。
- LittleFS 挂载失败 → 允许降级启动，但本地状态不能持久化。

---

## Q4: ESP-WHO 人脸识别是怎么工作的？

ESP-WHO 是 Espressif 官方开源的人脸检测/识别框架。

**检测阶段**（`face_detection`）：
- 从摄像头帧中定位人脸区域，返回人脸框坐标和关键点（landmarks）。
- 使用 MobileNet 类轻量级 CNN 模型，在 ESP32-S3 上可达 10+ FPS。

**识别阶段**（`face_recognition`）：
1. **注册（Enrollment）**：对检测到的人脸框进行特征提取，生成固定维度的特征向量（embedding），保存为模板文件到 SD 卡。
2. **识别（Recognition）**：实时帧中检测人脸 → 提取特征向量 → 与 SD 卡中所有保存的模板做 `1:N` 余弦相似度比对 → 找最高分 → 超过阈值即匹配成功。

**为什么模型存在 SPIFFS 分区？**
ESP-WHO 的模型文件（.bin）较大（MB 级别），不适合放在固件中。项目使用独立 SPIFFS 分区存储模型文件，固件启动时从分区加载。

**为什么模板存在 SD 卡？**
模板数量随员工增加而增长。Flash 空间有限（16MB，固件和文件系统共享），SD 卡容量大（GB 级），且可插拔替换。

---

## Q5: 人脸模板里存的是什么？是照片吗？

**不是照片。** 存储的是**特征向量（embedding）**——一个固定长度的浮点数组（如 512 维），是从人脸图片中通过神经网络提取的数学特征表示。

- 不能从特征向量反推出原始人脸照片。
- 特征向量之间的余弦距离反映两张人脸的相似度。
- 这样做保护了员工隐私——即使 SD 卡被拿走，也无法还原人脸图像。

每个人的模板只保存一份（取多张注册照片中质量最好的那份），规模控制在 50 人以内。

---

## Q6: 终端如何判断是上班打卡还是下班打卡？

设备本地保存了一份从后端同步的考勤配置，例如：
- 上班：`09:00-10:00`（分钟数：540-600）
- 下班：`18:00-19:00`（分钟数：1080-1140）

识别成功后：
1. 获取当前本地时间（`Asia/Shanghai` 时区）
2. 算出当前分钟数（`hour * 60 + minute`）
3. 判断是否落在 `[work_start, work_end)`（左闭右开）→ 如果是，判定为 `clock_in`
4. 判断是否落在 `[off_start, off_end)` → 如果是，判定为 `clock_out`
5. 都不在 → 不生成打卡记录，仅显示"不在打卡时段"

**为什么用左闭右开 `[start, end)`？**
- `09:00:00` 可以打卡，`10:00:00` 不能——精确到分钟粒度
- 服务端和设备端使用同一判定规则，确保一致性

---

## Q7: 同一个人同一天被识别多次怎么办？

**设备端去重（本地考勤标记）**：
- 维护一个按 `employeeId + localDate + type` 组合键的标记表
- 早上第一次识别 → 记录 `clock_in` 标记，写入待上传队列
- 同一天上午再次识别同一员工 → 命中已有标记，不上传，屏幕显示"今日上班已打卡"

**服务端去重（最终保障）**：
- `attendance_record` 表有唯一复合索引 `(employee_id, local_date, type)`
- 如果后上传的记录时间更早 → 更新为更早时间
- 如果更晚或相同 → 忽略

**为什么设备端和服务端都做去重？**
- 设备端去重：减少不必要的网络传输
- 服务端去重：防止设备端 bug 或恶意攻击导致重复数据入库

---

## Q8: 断网了怎么办？记录会丢吗？

**不会丢。** 系统的离线容错设计：

1. 识别成功 → 写入 `localAttendanceMarks`（内存/文件）+ `pendingUploadQueue`（LittleFS 持久化）
2. 断网 → 队列持续累积
3. 恢复联网 → `runtime_network_ops` 检测到 Wi-Fi 连接 → 批量调用 `/api/device/attendance/upload`
4. 上传响应中每条记录有 `status`：
   - `saved` → 从队列删除
   - `ignored_duplicate` → 从队列删除（服务器已有更早记录）
   - `rejected` → 记录失败日志后从队列删除（如员工不存在、时间不合法）

**如果设备断电？** 队列持久化在 LittleFS（Flash 文件系统），重启后恢复。

**Wi-Fi 一直连不上？** 队列保留在 Flash 中，Wi-Fi 恢复后自动批量上传。设备不会因为队列满而拒绝新打卡（MVP 规模小，队列不会爆）。

---

## Q9: Web Serial 配网是怎么实现的？

利用浏览器 **Web Serial API**（Chrome/Edge 支持）直接通过 USB 串口与 ESP32 通信。

**流程**：
1. 管理员在网页点击"连接设备"
2. 浏览器弹出串口选择对话框 → 选择 ESP32 的 USB CDC 串口
3. 连接成功后进入 `/devices/serial` 配置页面
4. 页面发送 JSON 命令（通过串口）：
   - `{"type":"set_wifi_profiles","profiles":[...]}` — 配置 Wi-Fi
   - `{"type":"set_backend_origin","origin":"..."}` — 配置后台地址
   - `{"type":"set_bootstrap_identity","deviceSerial":"...","bootstrapSecret":"..."}` — 下发激活凭据
5. ESP32 接收后写入 NVS，回复 `HITOMI_USB_RESPONSE {...}`
6. 终端拿到 Wi-Fi 凭据后自动连接网络，然后用 bootstrap 凭据调用 `/api/device/bootstrap/claim-status` 换取正式 `apiKey`

**为什么不用屏幕键盘输入？**
- 在 2.4 寸屏幕上输入 Wi-Fi 密码体验极差
- Web Serial 方案把配置责任交给管理后台（大屏 + 键盘），终端只负责接收

---

## Q10: 终端选用的 Wi-Fi 连接策略是什么？

采用**多级回退连接策略**，不依赖简单的"扫描 → 连第一个"：

1. **Directed Connect（最快路径）**：用上次成功连接的 `BSSID + Channel` 直接定向连接，跳过扫描。
2. **Scan + Rank（fallback）**：扫描周边 AP → 匹配已配置的 Wi-Fi profiles → 按优先级排序（profile 优先级 > 上次连接时间 > BSSID 匹配 > RSSI 信号强度）。
3. **Direct Connect without BSSID（兜底）**：对隐藏 SSID 或扫描结果过期的情况，直接尝试连接。

**认证失败冷却（Cooldown）**：
- 某个 Wi-Fi profile 认证失败（密码错误）后，该 profile 进入当前启动会话的冷却期
- 避免坏密码反复重试，耽误其他可用 profile

**加速重连元数据**：
- 连接成功后持久化 `lastSuccessAt`、`lastSuccessBssid`、`lastSuccessChannel`
- 下次启动时优先走路径 1，大幅加快重连速度

---

## Q11: 终端有触摸屏幕吗？UI 是怎么做的？

**硬件**：
- 显示屏：ST7789 驱动的 2.4 寸 SPI LCD（240×320 分辨率）
- 触摸：FT6336 电容触摸控制器，与 LCD 共用 I2C 总线

**UI 框架**：LVGL 9.5.0
- 状态屏幕（Status Screen）：显示 Wi-Fi 连接状态、设备激活状态、考勤配置、录脸任务数量、最近打卡事件
- 录制人脸界面：摄像头实时预览 + 人脸框叠加 + 录入进度提示
- 打卡结果界面：识别成功 → 员工姓名 + 打卡类型 + 打卡时间

**触摸交互**：
- LVGL indev 回调轮询 FT6336 触摸坐标
- 当前版本主要用于状态查看，后续可扩展按钮交互（如手动重连、手动触发录脸）

---

## Q12: 为什么人脸模板只存 SD 卡不存 Flash？为什么服务端不存人脸数据？

**存 SD 卡不存 Flash 的原因**：
- Flash 空间有限（16MB，系统 + SPIFFS + LittleFS 共享）
- Flash 有擦写次数限制（~10 万次），频繁更新模板会加速磨损
- SD 卡容量大（GB 级），可插拔替换，扩容方便

**服务端不存人脸数据的原因**：
- **隐私保护**：人脸特征属于生物识别信息，不出设备是最安全的做法
- **降低传输开销**：模板文件 MB 级别，Wi-Fi 上传慢
- **简化合规**：本系统是 MVP 学术项目，暂不涉及 GDPR/个人信息保护法的合规改造；架构上做好了"数据不出设备"的准备，未来合规时改动最小

---

## Q13: ESP32-S3 启动后做了哪些初始化？

`app_main()` → `initArduino()` → 创建 FreeRTOS 任务 → `AppRuntime::setup()`：

1. 读 NVS：设备身份（deviceCode, apiKey）、Wi-Fi profiles、后台地址
2. 检查 LittleFS 可用性
3. 读 LittleFS（如可用）：考勤配置、员工快照、录脸任务列表、待上传队列
4. 检查 SD 卡可用性
5. 初始化 SD 卡 → 读模板 manifest → 加载模板到 PSRAM
6. 初始化摄像头（DVP 接口）
7. 初始化显示屏（ST7789 + LVGL）和触摸（FT6336）
8. 连接 Wi-Fi
9. 连接成功 → 调用 `/api/device/sync` 拉取最新配置
10. 进入主循环 `AppRuntime::tick(nowMs)`

**核心 1 分配 FreeRTOS 任务**（优先级 1）跑主循环，Wi-Fi 事件在 core 0 处理，LVGL 渲染在 core 1。

**为什么模板加载到 PSRAM？**
ESP32-S3 有 8MB OPI PSRAM。1:N 人脸比对需要反复访问模板，在 PSRAM 中比对比每次从 SD 卡读快 100+ 倍。
