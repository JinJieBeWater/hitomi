# AGENTS

This repository uses `AGENTS.md` as the cross-agent entrypoint for project-specific guidance.

## Source of Truth

- Business rules, API contracts, page requirements, and device-side data semantics live in `spec/`.
- Engineering entrypoints and commands live in `README.md`, `hardware/README.md`, and `package.json`.
- Existing code is the integration truth for current implementation details, but it does not override explicit spec requirements.

## Global Rules

- For any non-trivial change, align with the relevant `spec/` document before editing code.
- Use Chinese for code review findings, review summaries, and review discussions.
- Treat `web/api/db` and `hardware` as two different development tracks.
- `hardware/` is a standalone PlatformIO project and is not part of the root `bun` / `turbo` task graph.
- Prefer the smallest verification command set that matches the scope of the change.

## Hardware Rules

- The device-side hardware target for this project is the [`SZPI ESP32-S3` development board](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/).
- The current firmware project targets the corresponding PlatformIO board definition `szpi_esp32s3_n16r8`.
- Any hardware-related implementation must be based on documentation first, then on the repository code. Do not guess hardware behavior.
- Hardware-related tasks include anything involving `hardware/`, pins, display, touch, LVGL, SD card, camera, audio, Wi-Fi, Bluetooth, sensors, power, flashing, serial, PlatformIO, or onboard peripherals.

For any hardware-related task, follow this order:

1. Read the relevant chapter in the `SZPI ESP32-S3` board wiki first.
2. Read the directly dependent official documentation next when needed, such as PlatformIO, ESP-IDF, Arduino-ESP32, `esp_lcd`, LVGL, or chip/peripheral driver documentation.
3. Inspect the current repository implementation after the documentation pass.
4. Implement only after the documentation basis is clear.

If the current repository implementation and the documentation appear inconsistent:

- Identify the source of the difference first.
- Confirm whether the repo is intentionally diverging or simply incomplete/outdated.
- Do not overwrite the implementation blindly.

If the documentation basis is still incomplete after checking the board wiki and the direct dependency docs:

- State the assumption explicitly before implementing.
- Keep the implementation conservative and evidence-based.

When reading the board wiki, start from these sections as relevant to the task:

- 第 1 章 开发板介绍
- 第 2 章 安装开发环境
- 第 3 章 开发流程详解
- 第 6 章 读写SD卡
- 第 9 章 液晶显示
- 第 10 章 摄像头
- 第 11 章 LVGL
- 第 12 章 WiFi扫描并连接

## Task Routing

- Admin pages, interactions, and UI behavior:
  - Read `spec/admin-pages.md` first.
  - Then read `spec/admin-api.md` if the page depends on API changes.
  - Use `apps/web/` for implementation.
- Admin APIs, business logic, and database work:
  - Read `spec/admin-api.md` first.
  - Read `spec/database-design.md` when schema or persistence rules are involved.
  - Use `packages/api/` and `packages/db/` for implementation.
- Device sync, local storage, runtime behavior, and hardware-side features:
  - Read `spec/device-api.md` first for protocol behavior.
  - Read `spec/device-local-design.md` first for local data and runtime semantics.
  - Read `hardware/README.md` for build, flash, monitor, and test entrypoints.
  - Read the `SZPI ESP32-S3` board wiki before implementation.

## Verification Entry Points

- Root type checks:
  - `bun run check-types`
- Admin/API smoke tests:
  - `bun run smoke:admin`
  - `bun run smoke:device`
  - `bun run smoke:admin-ui`
- Hardware host test:
  - `cd hardware && platformio test -e native`
- Hardware build / flash / monitor:
  - `cd hardware && platformio run -e szpi_esp32s3`
  - `cd hardware && platformio run -e szpi_esp32s3 -t upload`
  - `cd hardware && platformio device monitor -b 115200`
