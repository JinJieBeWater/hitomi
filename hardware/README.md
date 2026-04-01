# Hardware Runtime

`hardware/` is a standalone PlatformIO project for the SZPI ESP32-S3 device runtime.

It is intentionally **not** wired into the root `bun` / `turbo` task graph yet. Build, flash, and test it from the `hardware/` directory.

## Tooling

Install PlatformIO CLI on a machine that will build the firmware:

```bash
python3 -m pip install --user platformio
platformio --version
```

If `platformio` is not on your shell path, use:

```bash
python3 -m platformio --version
```

## Build

```bash
cd hardware
platformio run -e szpi_esp32s3
```

## Flash

```bash
cd hardware
platformio run -e szpi_esp32s3 -t upload
```

## Serial Monitor

```bash
cd hardware
platformio device monitor -b 115200
```

## Host Tests

PlatformIO native env:

```bash
cd hardware
platformio test -e native
```

Local fallback without PlatformIO CLI:

```bash
clang++ -std=c++17 -Ihardware/include \
  hardware/test/test_core_rules/test_core.cpp \
  -o /tmp/hardware_test_core && /tmp/hardware_test_core
```

## Current Layout

- `include/board`: board constants and runtime defaults
- `include/core` + `src/core`: pure device models and business rules
- `include/app` + `src/app`: runtime orchestration
- `include/infra` + `src/infra`: display, storage, and device HTTP adapters
- `include/ui` + `src/ui`: view models and status screen presenter
- `include/face`: camera / enrollment / recognition ports with no-op defaults

## Current Scope

- Loads device credentials, snapshots, queue, and failure logs from local storage
- Renders a single LVGL status screen
- Periodically probes Wi-Fi state
- Syncs `/api/device/sync` on startup / reconnect / interval when configured
- Uploads pending attendance records in batches when configured
- Leaves camera / enrollment / recognition implementations as abstract ports
