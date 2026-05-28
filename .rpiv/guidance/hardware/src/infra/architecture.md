# hardware/src/infra — 硬件抽象层 (Ports & Adapters)

## Responsibility
嵌入式设备六边形架构的边界。定义核心应用依赖的抽象接口（"ports"）以及具体 ESP32 实现（"adapters"）。核心应用永远不会导入 `<esp_camera.h>`、`<lvgl.h>` 或 `<sdmmc_cmd.h>` — 它基于 `face::` 和 `infra::` 命名空间中的纯虚接口编码。

## 子层
| 子层 | Port (抽象接口) | Adapter (具体实现) | 硬件 |
|---|---|---|---|
| `camera/` | `face::CameraPort` | `Esp32CameraPort` | DVP 相机传感器 + IO 扩展器电源控制 |
| `display/` | `infra::DisplayPort` | `LvglStatusDisplay` (包含 `SzpiLvglDisplay`) | ST7789 SPI LCD + FT6336 I2C 触摸 |
| `face/` | `face::EnrollmentServicePort` | `EspWhoEnrollmentService` | ESP-WHO 人脸检测 + 特征提取 |
| `network/` | `infra::DeviceApiClient` | `HttpDeviceApiClient` | WiFiClient HTTP JSON POST |
| `storage/` | `infra::LocalStore` | `JsonLocalStore` | Preferences NVS + LittleFS |
| `storage/` | `infra::TemplateStorePort` | `SdMmcTemplateStore` | SD/MMC (SDMMC 1-bit 模式) |
| `board/` | **无端口** — 共享工具 | 自由函数 (`board_control_bus.cpp`) | PCA9557 IO 扩展器 (共享 I2C 总线) |

每个端口都有对应的 `Noop*Port` 空实现，返回安全默认值（`false`、`nullptr`、`std::nullopt`），用于硬件缺失或测试场景。

## 通用模式

### Port/Adapter 规范形态
```cpp
// === PORT (抽象接口) ===
// File: hardware/include/infra/some_port.hpp
class SomePort {
 public:
  virtual ~SomePort() = default;           // 始终虚析构
  virtual bool init() = 0;                 // 两阶段初始化约定
  virtual bool ready() const = 0;
};

// === ADAPTER (具体实现) ===
// File: hardware/include/infra/domain/some_adapter.hpp
class SomeAdapter final : public SomePort {   // final: 无进一步子类化
 public:
  SomeAdapter();
  ~SomeAdapter() override;
  SomeAdapter(const SomeAdapter&) = delete;          // 禁止拷贝
  SomeAdapter& operator=(const SomeAdapter&) = delete;
  SomeAdapter(SomeAdapter&&) noexcept;               // 允许移动
  SomeAdapter& operator=(SomeAdapter&&) noexcept;

  bool init() override;
  bool ready() const override;

 private:
  struct Impl;                                      // 不透明 Impl (Pimpl 惯用法)
  std::unique_ptr<Impl> impl_;                      // 堆分配, 仅移动
};
```

### Pimpl (所有适配器)
`struct Impl` 在 `.cpp` 中完全定义 — 驱动头文件（`<esp_camera.h>`、`<lvgl.h>`）不会泄露给 public header 的包含者。构造/析构函数**必须**在 `.cpp` 中定义。移动操作可以在 `.cpp` 中 `= default`。

### FreeRTOS 互斥锁保护的线程安全
```cpp
struct Impl {
  SemaphoreHandle_t mutex = xSemaphoreCreateMutex();  // FreeRTOS 互斥锁, 非 std::mutex
  Status status = {};

  template <typename Fn>
  void withLock(Fn&& updater) {                        // 互斥锁保护的变更
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      std::forward<Fn>(updater)(status);
      xSemaphoreGive(mutex);
    }
  }

  Status copyStatus() const {                          // 互斥锁保护的快照（返回副本）
    Status snapshot;
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      snapshot = status;
      xSemaphoreGive(mutex);
    }
    return snapshot;
  }
};
```

## 关键适配器详情

### Esp32CameraPort — RAII Frame Lease
`capture()` 返回 `unique_ptr<CameraFrameLease>` — 析构函数调用 `esp_camera_fb_return()`。若调用者提前返回或抛出异常，帧缓冲区永不泄漏。上电时序: IO 扩展器去断言 PWDN → 稳定延迟 → `esp_camera_init()`。

### LvglStatusDisplay — 三级架构
1. `SzpiLvglDisplay`: SPI 总线 + ST7789 面板 + LVGL 驱动注册 + FT6336 触摸 I2C（**仅此处调用 ESP-IDF 显示 API**）
2. `LvglStatusDisplayData`: 所有 LVGL widget 创建和样式。`constexpr` 布局常量，`apply*Style()` 辅助函数，`StatusTone` 枚举驱动语义着色 (绿=正常, 琥珀=警告, 红=错误)
3. `LvglStatusDisplay` (实现 `DisplayPort`): `render(viewModel)` → `refreshUi()` → 按页刷新 (Home/Enroll/Capture/System)。通过环形缓冲区出队触摸命令。通过降级-排空通知队列

### JsonLocalStore — 双后端
- **Preferences (NVS)**: 设备配置 (JSON 序列化) + 凭证 (deviceCode + apiKey) — 跨 OTA 更新持久存在
- **LittleFS**: JSON 文件，分别用于快照、注册报告、考勤队列、考勤标记、故障日志、存储辅助数据
- 8 个私有内部存储类，每个负责一个文件 — `load()` 在文件缺失时返回空，永不出错

### SdMmcTemplateStore — 原子写入
通过 temp-then-rename 保证清单和模板块的原子性:
`/templates/manifest.tmp.json` → `/templates/manifest.json`
`/templates/.tmp_{id}.bin` → `/templates/{id}.bin`
POSIX `::rename` 在同一文件系统上是原子的。失败时调用 `std::remove(temp)`。

### HttpDeviceApiClient — 模板方法 POST
单个 `postJson<T>(url, requestDoc, parseFn)` 模板处理所有端点。每个端点调用注入特定领域的解析函数。错误分类: 传输错误始终可重试，4xx 不可重试，5xx 可重试。

### Board Control Bus — 共享 I2C 工具
自由函数模块（非类），内部有单例状态。4 个槽位的设备池，惰性分配。`ensureBoardControlBusReady()` 是幂等的。`updateBoardControlRegisterBit()` 实现 I2C 的读-改-写。

<important if="you are adding a new hardware port">
## 新增硬件端口
1. 在 `hardware/include/infra/` 或 `hardware/include/face/` 中创建抽象接口（纯虚类）
2. 在同一头文件中添加 `Noop*Port` 实现（返回安全默认值）
3. 在 `include/infra/<domain>/` 中创建具体适配器头文件（`final class : public Port`，Pimpl）
4. 在 `src/infra/<domain>/` 中创建具体适配器源文件（定义 `struct Impl`，实现所有纯虚方法）
5. 在 `bootstrap.cpp` 中实例化为函数局部静态变量，注入 `AppRuntime` 构造函数
6. 将引用添加到 `RuntimeContext`
</important>

<important if="you are adding a new LVGL page or UI element">
## 新增 LVGL 页面
1. 在 `lvgl_status_display.cpp` 中的 `SidebarPage` 枚举添加值
2. 添加上下文布局常量块 (`constexpr lv_coord_t kNewPage*`)
3. 创建页面容器 + widget，使用 `apply*Style()` 辅助函数
4. 在 `LvglStatusDisplayData` 中添加成员指针
5. 创建 `refresh*Page()` 函数，从 `lastViewModel` 更新标签
6. 在 `refreshCurrentPage()` 和 `showCurrentPage()` 中添加 case
7. 事件回调通过 `enqueueCommand()` 生成 `DisplayCommand`
</important>
