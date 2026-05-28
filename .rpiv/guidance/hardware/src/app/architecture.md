# hardware/src/app — 应用运行时层（编排器）

## Responsibility
拥有主游戏循环 (`setup`/`tick`)、共享的 `RuntimeState` 结构体和保存所有 infra 端口引用的 `RuntimeContext`。**非 OOP 框架** — 是自由函数过程式运行时，按领域"ops"模块组织。每个模块接收 `(const RuntimeContext&, RuntimeState&, uint32_t nowMs)` 并对共享状态进行原地修改。

## Dependencies
- **`core/`**: 领域模型和纯函数 (`classifyAttendanceType`, `enqueueAttendanceRecord`, `rankWifiProfiles`, `applySyncSnapshot`)
- **`infra/`**: 所有抽象端口 (`DisplayPort`, `LocalStore`, `TemplateStorePort`, `DeviceApiClient`)
- **`face/`**: 相机 + 注册端口 (`CameraPort`, `EnrollmentServicePort`)
- **`board/`**: 编译时常量 (`board::app_config.hpp` 中的所有 `k*` 值)

## Module Structure
```
app/
├── app_runtime.cpp           # [编排器] AppRuntime::setup() 线性初始化序列 + tick() 主循环调度
├── bootstrap.cpp             # [组合根] 单例注入 — 创建所有具体 infra 适配器，注入 AppRuntime
├── runtime_state.cpp         # [状态] buildRuntimeStatus() 投影 + resolveWallClockTimeMs()
├── runtime_lifecycle_service.cpp # [系统] 390 行 WiFi 状态机 + 按钮轮询 + 串口等待
├── runtime_network_executor.cpp  # [异步IO] FreeRTOS HTTP 工作线程，互斥锁保护的双端队列
├── runtime_network_planner.cpp   # [调度] 纯函数 nextNetworkRequestType() — 优先级链
├── runtime_network_ops.cpp       # [网络] 请求构建/提交 + 结果消费 (350 行编排)
├── runtime_camera_ops.cpp        # [相机] 初始化 + 帧轮询 (含性能监控)
├── runtime_face_engine_ops.cpp   # [人脸] 检测 + 异步识别 FreeRTOS 任务 + 考勤事件发布
├── runtime_enrollment_ops.cpp    # [注册] 异步注册 FreeRTOS 任务 + 进度 + 模板保存
├── runtime_storage_ops.cpp       # [持久化] 粒度为每个字段的 LittleFS JSON 持久化 + SD 模板协调
├── runtime_provisioning_ops.cpp  # [USB] 逐行串口 JSON 命令解析 + 应用 + 响应
├── runtime_view_ops.cpp          # [视图] 7 行桥接: buildStatus → presenter.build → display.render
├── runtime_diagnostics.cpp       # [诊断] 人类可读的串口诊断行构建器
└── usb_provisioning_protocol.cpp # [协议] USB JSON 命令解析器 + 带帧响应构建器
```

## AppRuntime::tick() 主循环 (顺序固定)
```
display.tick(nowMs)
processUsbProvisioning(...)         — 读取串口 JSON provisioning 命令
pollBootButton(state, nowMs)        — 消抖 GPIO 读取
processWifiDriverEvents(...)        — 消费原子事件桥 (WiFi 回调 → 主线轮询)
ensureWifiConnection(...)           — 分层连接: last-known-good → scan-candidate → fallback
consumeCompletedNetworkRequest(...) — 出队已完成的 HTTP 响应
while (cmd = display.consumeCommand()) handleDisplayCommand(...)  — 触摸屏操作
pollCamera(...)                     — 帧 → 注册处理 或 人脸检测 → 预览
probeConnectivity(...)              — [周期] WiFi 状态检查
probeTemplateStore(...)             — [周期] SD 重试
dispatchNetworkRequest(...)         — 规划并提交下一个 HTTP 请求
if (renderDirty) updateView(...)    — 脏标记触发 LVGL 重建
```

## RuntimeState（超大型状态 — 80 多个字段）
单一的扁平结构体，保存**所有**可变运行时状态。无方法 — 所有修改通过 ops 模块中的自由函数进行。每个定时操作都有 `last*Ms` 字段；间隔守卫使用 `if (nowMs - state.last*Ms < kInterval) return;`。`renderDirty` 是显示流水线的唯一触发器。

## 网络请求优先级
```
1. 手动刷新 (用户点击 → ApiProbe → Activation → Sync)
2. 定期 ApiProbe (可配置间隔，由 shouldProbeApi() 控制)
3. 激活重试 (若未凭证化)
4. 注册上报重试
5. 定期同步
6. 考勤上传重试
```

## 代次失效（Stale Result Rejection）
`networkRequestGeneration` 计数器 — WiFi 断开或后端变更时递增。已完成的响应携带其生成编号；若与当前编号不匹配则静默丢弃。人脸识别结果同理。

## USB Provisioning 协议
换行符分隔的基于行的 JSON 命令，通过 `Serial` (USB-UART)。响应以 `"HITOMI_USB_RESPONSE "` 为前缀。命令: `GetConfig`, `SetWifiProfiles`, `SetBackendOrigin`, `SetBootstrapIdentity`, `ClearRuntimeCredentials`, `ClearWifiProfiles`, `ResetDeviceConfig`。

## Architectural Boundaries
- **无 ISR 阻塞**: 主线永不做阻塞 I/O — 异步任务通过 FreeRTOS 通知 + 互斥锁保护槽位
- **WiFi 事件通过原子桥**: `std::atomic` 全局变量捕获自 WiFi 回调 (ISR 上下文)；通过 `processWifiDriverEvents()` 在主线中轮询
- **人脸模型锁**: `face::tryLockModel(timeoutMs)` — 超时自旋锁。ESP-WHO 模型非可重入
- **⚠️ RecognitionServicePort 虚设**: 端口已定义但仅接 `NoopRecognitionServicePort`。实际识别在 `runtime_face_engine_ops.cpp` 中直接调用 ESP-WHO。若需要将识别抽象到端口后面（便于测试或替换），可后续实现具体适配器

<important if="you are adding a new runtime ops module">
## 新增 Runtime Ops 模块
1. 在 `include/app/` 中创建 `runtime_xyz_ops.hpp`，声明接收 `(const RuntimeContext&, RuntimeState&, uint32_t nowMs)` 的自由函数
2. 在 `src/app/` 中创建 `runtime_xyz_ops.cpp`，将私有辅助函数放入匿名命名空间
3. 在 `app_runtime.cpp` 中包含头文件
4. 在 `AppRuntime::tick()` 中于正确位置调用函数
5. 若函数产生可见变化，设置 `state.renderDirty = true`
</important>

<important if="you are adding a new network request type">
## 新增网络请求类型
1. 在 `runtime_network_executor.hpp` 中的 `NetworkRequestType` 枚举添加新值
2. 在 `PendingNetworkRequest` 中添加有效载荷字段，在 `CompletedNetworkRequest` 中添加结果字段
3. 在 `execute()` 中添加 case，调用对应的 `deviceApiClient.*()` 方法
4. 在 `consumeCompletedNetworkRequest()` 中添加 case，调用 `apply*Result()` 处理函数
5. 在 `nextNetworkRequestType()` 中添加资格检查和优先级
</important>

<important if="you are adding a new USB provisioning command">
## 新增 USB Provisioning 命令
1. 在 `UsbProvisioningCommandType` 添加枚举值
2. 在 `UsbProvisioningCommand` 中添加有效载荷字段
3. 在 `parseUsbProvisioningCommand()` 中添加 `if (type == "my_command")` 解析块
4. 在 `applyUsbProvisioningCommand()` 中添加 case
</important>
