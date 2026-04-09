# Local Overrides

`hardware/` now resolves third-party Arduino/ESP-IDF dependencies through
PlatformIO package installation plus a project-local `.pio/installed/`
component copy, instead of checking full third-party source trees into the
repository.

Keep this directory only for project-local component overrides that cannot be
expressed as managed dependencies.
