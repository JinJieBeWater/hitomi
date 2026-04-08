# Hardware Runtime

`hardware/` is a standalone PlatformIO project for the SZPI ESP32-S3 device runtime.
It now uses an `ESP-IDF primary + Arduino as component` mixed-framework layout,
with PlatformIO acting as the build/flash/test frontend.

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

Phase 1 keeps the existing `setup()/loop()` runtime path via Arduino autostart.
The IDF-primary project skeleton now lives in:

- `CMakeLists.txt`
- `src/CMakeLists.txt`
- `sdkconfig.defaults`
- `components/`

The mixed-framework build also pulls `esp_littlefs` through PlatformIO `lib_deps`
and exposes it to ESP-IDF via `EXTRA_COMPONENT_DIRS`, which keeps the existing
`LittleFS`-based local store working under Arduino-as-component mode.

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

- `CMakeLists.txt` + `src/CMakeLists.txt`: ESP-IDF primary project skeleton
- `sdkconfig.defaults`: IDF defaults used by the mixed-framework build
- `components/`: reserved for future managed IDF components
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

- Loads device config, snapshots, queue, and failure logs from local storage
- Supports USB-C serial provisioning for Wi-Fi profiles, backend origin, and bootstrap identity
- Automatically reconnects to known Wi-Fi profiles based on configured priority / prior success
- Supports bootstrap activation against the local backend before switching to runtime device credentials
- Renders a single LVGL status screen
- Registers FT6336 touch as an LVGL pointer input on the shared I2C bus
- Periodically probes Wi-Fi state
- Syncs `/api/device/sync` on startup / reconnect / interval when configured
- Uploads pending attendance records in batches when configured
- Leaves camera / enrollment / recognition implementations as abstract ports

## Touchscreen

The SZPI ESP32-S3 runtime now probes the onboard FT6336 touch controller on the existing I2C bus and registers it as an LVGL pointer device.

- Touch reads are polled from the LVGL indev callback.
- The touch path uses a short read timeout to reduce the chance that controller failures visibly stall the UI loop.
- The status screen exposes a diagnostic `Touch 0` counter when touch initialization succeeds. The counter increments on each detected touch-release cycle.
- The status screen also renders five labeled touch probes: `TL`, `TR`, `C`, `BL`, and `BR`. When a probe is hit, the screen updates `Last: ...` and logs the clicked control id to serial. This is the primary control-hit verification path for later interactive UI work.
- BOOT-key recovery remains the fallback path if touch is unavailable or miscalibrated.

## USB Provisioning

The firmware now accepts newline-delimited JSON commands on the USB CDC serial port. This remains the protocol contract for first-run provisioning. The recommended operator experience is to trigger these commands from the web admin's Web Serial activation wizard, with PlatformIO / serial-monitor tools kept as fallback.

Supported commands:

```json
{"type":"get_config"}
{"type":"set_wifi_profiles","profiles":[{"ssid":"Lab","password":"secret","priority":5}]}
{"type":"set_backend_origin","origin":"http://192.168.1.10:3000"}
{"type":"set_bootstrap_identity","deviceSerial":"BOOT-001","bootstrapSecret":"secret"}
{"type":"clear_runtime_credentials"}
{"type":"clear_wifi_profiles"}
{"type":"reset_device_config"}
```

Responses are JSON summaries that intentionally omit secrets such as the runtime `apiKey`.

## Demo Flow

Suggested demo flow:

1. Flash firmware and open the admin backend in a Chromium browser.
2. Create a device and keep the bootstrap credentials available.
3. Open the USB activation wizard from the device page or creation result modal.
4. Connect the device over Web Serial and write `set_wifi_profiles`, `set_backend_origin`, and `set_bootstrap_identity`.
5. Let the device auto-connect to a known Wi-Fi profile.
6. Issue activation from the same wizard flow.
7. Device receives runtime `deviceCode/apiKey` and continues with normal `/api/device/sync`.

Fallback path:

- If Web Serial is unavailable, use PlatformIO / a serial monitor to send the same JSON commands manually.
