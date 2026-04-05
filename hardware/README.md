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

This repository is known to work with `python3 -m platformio` even when `platformio` is not on `PATH`.

## Build

```bash
cd hardware
python3 -m platformio run -e szpi_esp32s3
```

## Flash

```bash
cd hardware
python3 -m platformio run -e szpi_esp32s3 -t upload
```

## Serial Monitor

```bash
cd hardware
python3 -m platformio device monitor -b 115200
```

## Host Tests

PlatformIO native env:

```bash
cd hardware
python3 -m platformio test -e native
```

Native tests are isolated PlatformIO suites under `hardware/test/`. They link against a curated subset of project sources and do not rely on direct inclusion of production `.cpp` files.
If `platformio` is not on `PATH`, the same `python3 -m platformio ...` invocation above is the supported fallback.

## Current Layout

- `include/board`: board constants and runtime defaults
- `include/core` + `src/core`: pure device models and business rules
- `include/app` + `src/app`: runtime orchestration
- `include/infra` + `src/infra`: display, storage, and device HTTP adapters
- `include/ui` + `src/ui`: view models and status screen presenter
- `include/face`: camera / enrollment / recognition ports with no-op defaults

## Compile-Speed Guardrails

- Keep public headers lightweight. Do not expose LVGL, ESP-IDF LCD/SPI/I2C, `Preferences`, or other heavy implementation details from `.hpp` files unless the contract truly requires those types.
- Prefer composition/bootstrap files and implementation-private helpers over letting `main.cpp` or application-facing headers include concrete hardware adapter internals.
- When refactoring runtime code, move explicit state and service boundaries first; splitting one large `.cpp` into many files without reducing shared-state coupling is not considered an improvement.
- Keep host tests adapter-light and avoid direct inclusion of production `.cpp` files. Use `platformio.ini` source filtering so native tests only build the logic they need.
- For compile-scope checks, compare verbose `Compiling` output after touching a single `src/app/*.cpp` or `src/ui/*.cpp` file. Those edits should not force display/storage implementation objects to rebuild unless a shared public header changed.

## Current Scope

- Loads device credentials, snapshots, queue, and failure logs from local storage
- Renders a single LVGL status screen
- Periodically probes Wi-Fi state
- Syncs `/api/device/sync` on startup / reconnect / interval when configured
- Uploads pending attendance records in batches when configured
- Leaves camera / enrollment / recognition implementations as abstract ports
