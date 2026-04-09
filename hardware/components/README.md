# Local Components

This directory contains vendored ESP-IDF components used by the current
`hardware/` runtime.

At the moment it hosts:

- `arduino-esp32` `3.3.7`
- `littlefs` `1.20.4`

These components are checked into the repository so the current
`ESP-IDF-primary + Arduino as component` build remains reproducible without
depending on PlatformIO-managed downloads at build time.
