# hardware — ESP32-S3 固件 (PlatformIO · 独立项目)

## Responsibility
SZPI ESP32-S3 设备端运行时 — 人脸识别考勤终端。独立于根 monorepo 的 `bun`/`turbo` 任务图，通过 PlatformIO (`espidf` framework) 构建和烧录。

## Architecture
```
hardware/
├── src/
│   ├── main.cpp              # 🚀 app_main() 入口 → initArduino() → FreeRTOS task (core 1)
│   ├── app/                  # 🎯 应用编排层 — AppRuntime setup/tick 主循环
│   ├── core/                 # 🧠 纯领域逻辑 — 无硬件依赖, native-testable
│   ├── infra/                # 🔌 硬件适配层 — Port/Adapter 模式 (display, storage, camera, face, network)
│   ├── ui/                   # 🖥️ 展示层 — StatusScreenPresenter: RuntimeStatus → AppViewModel
│   ├── face/                 # 👤 人脸服务端口 — 抽象接口 (CameraPort, EnrollmentServicePort)
│   └── fonts/                # LVGL 字体资产
├── include/                  # 公共头文件 (镜像 src/ 结构)
├── test/                     # PlatformIO native 单元测试 (6 suites)
├── docs/                     # 机制说明文档 (6 篇)
├── platformio.ini            # 双目标: szpi_esp32s3 (硬件) + native (测试)
├── boards/szpi_esp32s3_n16r8.json  # 自定义板定义 (16MB Flash, 8MB OPI PSRAM)
├── sdkconfig.defaults        # ESP-IDF Kconfig 预设
├── default_16MB.csv          # OTA 分区表
├── arduino-component.lock.json     # 锁定的 Arduino 核心 (v3.3.7)
└── dependencies.lock          # 锁定的 ESP-IDF 管理组件
```

## 层依赖 (严格自上而下)
```
app/ → core/, infra/, face/, ui/, board/
ui/  → app/ (RuntimeStatus)
core/ → 无 (纯 C++ 结构体和函数)
infra/ → core/, face/
face/ → 无 (纯虚接口)
board/ → 无 (编译时常量 + 引脚定义)
```

## 构建系统
两步构建: PlatformIO → 预构建脚本下载+修补 Arduino → ESP-IDF CMake + 组件管理器解析依赖。

关键依赖：`lvgl` 9.5.0, `ArduinoJson` 7.4.3, `esp32-camera` 2.1.5, ESP-WHO 人脸检测/识别模型, LittleFS 1.20.4。

## 双目标
| 目标 | 用途 | 框架 |
|---|---|---|
| `szpi_esp32s3` | 设备烧录 | ESP-IDF + Arduino-as-component |
| `native` | 单元测试 (`pio test -e native`) | 仅编译可移植代码 (core/, 选定的 app/ 和 infra/ 文件) |

## 测试
6 个测试套件在 `test/` 下，每个含 `test_main.cpp`。使用 `expect()` + try/catch，无测试框架。运行: `cd hardware && platformio test -e native`。新增测试需将 `.cpp` 加入 `platformio.ini` `build_src_filter`。

## Commands
| Command | Purpose |
|---|---|
| `cd hardware && platformio run -e szpi_esp32s3` | 构建固件 |
| `cd hardware && platformio run -e szpi_esp32s3 -t upload` | 烧录到设备 |
| `cd hardware && platformio device monitor -b 115200` | 串口监视器 |
| `cd hardware && platformio test -e native` | 本地测试 |

## Port/Adapter 映射
| 抽象端口 (include/) | 具体适配器 (src/infra/) |
|---|---|
| `infra::DisplayPort` | `LvglStatusDisplay` (LVGL + ST7789 SPI LCD + FT6336 触摸) |
| `infra::LocalStore` | `JsonLocalStore` (Preferences NVS + LittleFS JSON) |
| `infra::TemplateStorePort` | `SdMmcTemplateStore` (SD/MMC + manifest 索引) |
| `infra::DeviceApiClient` | `HttpDeviceApiClient` (WiFiClient HTTP JSON POST) |
| `face::CameraPort` | `Esp32CameraPort` (DVP camera + RAII frame lease) |
| `face::EnrollmentServicePort` | `EspWhoEnrollmentService` (ESP-WHO 注册流程) |
| `face::RecognitionServicePort` | `NoopRecognitionServicePort` ⚠️ — 端口已定义但未使用；实际识别在 `app/` 层直接调用 ESP-WHO |

## Architectural Boundaries
- **无 Arduino `loop()`**: 主循环是 `AppRuntime::tick(nowMs)` 在 FreeRTOS 任务中运行 (core 1, 优先级 1)
- **WiFi 在 core 0, LVGL 在 core 1**: 异步 HTTP 执行器在独立 FreeRTOS 任务中运行
- **非可重入的人脸模型**: 所有 ESP-WHO 调用必须通过 `face::tryLockModel()` 获取互斥锁
- **platformio.local.ini 已 gitignore**: Wi-Fi 密码和 API URL 的本地覆盖 — 每位开发者维护自己的副本

<important if="you are adding a new PlatformIO test">
## 新增测试
1. 创建 `test/test_<module>/test_main.cpp`
2. 仅包含可移植头文件（无 Arduino, 无 FreeRTOS 直接依赖）
3. 使用 `expect()` + try/catch 在 `main()` 中
4. 如需，将 `.cpp` 加入 `[env:native]` 的 `build_src_filter`
5. 运行: `pio test -e native`
</important>

<important if="you are changing partition layout or SDK config">
## 分区表 / SDK 配置
- 分区表: `default_16MB.csv` — 两个 OTA 分区 (app0, app1) 各 5MB, 人脸模型在 spiffs 分区中
- SDK: `sdkconfig.defaults` — 预设项 (PSRAM Octal, Arduino 选择性编译, LittleFS 调优, 人脸模型存闪存分区)
- 永远不要手动编辑 `sdkconfig.szpi_esp32s3` (构建生成)
</important>
