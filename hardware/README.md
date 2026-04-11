# Hardware Runtime

`hardware/` is a standalone PlatformIO project for the SZPI ESP32-S3 device runtime.
It now uses an `ESP-IDF primary + Arduino as component` layout, with PlatformIO
acting as the build/flash/test frontend.

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

The runtime uses an explicit `app_main() + initArduino()` entry, with the
Arduino core provided as a local ESP-IDF component.

The IDF-primary project skeleton now lives in:

- `CMakeLists.txt`
- `src/CMakeLists.txt`
- `sdkconfig.defaults`
- `components/`

Arduino is now installed through PlatformIO package resolution, pinned to
`arduino-esp32` `3.3.7`, instead of being vendored wholesale under
`hardware/components/`. The build copies that installed package into the
project-local `.pio/installed/` area and patches only that local copy before
CMake resolves components.

LittleFS is resolved at build time through the Arduino component's ESP-IDF
dependency manifest, so `managed_components/` remains a build artifact rather
than repository content.

PlatformIO also materializes a per-environment `sdkconfig.<env>` file during the
first ESP-IDF configure. It is intentionally ignored in git. If you change board
memory wiring or `sdkconfig.defaults` and the generated firmware still behaves as
if the old settings were active, remove that `sdkconfig.<env>` once and rebuild so
the effective config is regenerated from the updated defaults.

`dependencies.lock` is committed so the resolved build-time component graph stays
stable across machines, while `managed_components/` remains a build artifact.

Maintenance details for this dependency-install flow live in:

- `hardware/docs/dependency-install-mode.md`
- `hardware/docs/build-system-mechanism.md`

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
- `sdkconfig.defaults`: defaults used by the current ESP-IDF-primary build
- `platformio.ini`: pins the installed Arduino package version used as an
  ESP-IDF component
- `arduino-component.lock.json`: source lock for Arduino tarball URL/version/SHA256 and pinned LittleFS version
- `.pio/installed/`: project-local installed Arduino component copy generated at build time
- `components/`: reserved for project-local overrides only
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
- Reconnects to known Wi-Fi profiles using last-known-good directed connect, scan-ranked candidates, and auth-failure cooldown
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

The firmware now accepts newline-delimited JSON commands on the USB CDC serial port. This remains the protocol contract for first-run provisioning. The recommended operator experience is to start from the web admin's device page, let Chromium prompt for a serial port, and then continue in the dedicated `/devices/serial` configuration console. PlatformIO / serial-monitor tools remain the fallback.

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

Responses include the current editable config snapshot for Web Serial forms, while still omitting activation secrets such as the runtime `apiKey`.

## Wi-Fi Connection Mechanism

The current firmware uses a staged connection strategy instead of the older
"scan first, then try one profile" loop.

At a high level:

1. Provisioning stores a list of Wi-Fi profiles with operator-assigned priority.
2. Successful connections persist last-known-good metadata per profile:
   - `lastSuccessAt`
   - `lastSuccessBssid`
   - `lastSuccessChannel`
3. On boot or reconnect, the runtime first attempts a directed connect using the
   best last-known-good profile and its stored `BSSID/channel` when available.
4. If that fails, the runtime performs one async scan, builds one best visible
   candidate per configured profile, and ranks those candidates by:
   - profile priority
   - prior success recency
   - last-known-good BSSID match
   - RSSI
5. If scan-based candidates still produce no usable target, the runtime performs
   one fallback direct connect against the best configured profile. This keeps
   hidden SSIDs and stale scan results usable.
6. Authentication-style failures such as wrong passwords trigger a runtime
   cooldown for that profile so a bad credential does not repeatedly delay
   better profiles in the same boot session.
7. Connection state is updated from Wi-Fi driver events. `GOT_IP` persists the
   latest last-known-good metadata; disconnect reasons are used to classify
   retry behavior.

Important constraint:

- A scan can tell the device which APs are visible, their RSSI, auth mode,
  BSSID, and channel. It cannot prove a password is correct. Password validity
  is only known after a real association/authentication attempt.

Detailed design notes live in:

- `hardware/docs/wifi-connection-mechanism.md`
- `hardware/docs/network-request-mechanism.md`
- `hardware/docs/board-control-bus-mechanism.md`

## Demo Flow

Suggested demo flow:

1. Flash firmware and open the admin backend in a Chromium browser.
2. Create a device and keep the bootstrap credentials available.
3. Open the serial configuration entry from the device page header or the creation result modal.
4. Select the device in the browser's serial chooser, then let the `/devices/serial` page write `set_wifi_profiles`, `set_backend_origin`, and `set_bootstrap_identity`.
5. Let the device auto-connect to a known Wi-Fi profile.
6. Continue activation from the same serial configuration page.
7. Device receives runtime `deviceCode/apiKey` and continues with normal `/api/device/sync`.

Fallback path:

- If Web Serial is unavailable, use PlatformIO / a serial monitor to send the same JSON commands manually.
