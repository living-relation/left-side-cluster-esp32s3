---
name: cloud-agent-starter
description: Use when starting work in Cursor Cloud on this ESP-IDF + LVGL firmware repo and needing bootstrap, build, flash/monitor, Kconfig flags, hardware-less gates, or area-specific verification steps.
---

# Cloud agent starter (ESP32-S3 gauge cluster firmware)

Runbook for agents in headless Linux (Cursor Cloud). There is no web server or npm app: the “app” is firmware you build and optionally flash to hardware.

## 0) Auth and permissions (before scripts)

- **GitHub / PRs:** `git status`; `gh auth status` (use the environment’s token or `gh auth login` if needed).
- **`dev-setup.sh`:** clones ESP-IDF and may run `sudo apt-get` for Python venv support — needs **network + sudo**.
- **Firmware runtime:** no cloud login; UART/BT are code + Kconfig, not repo credentials.

## 1) Bootstrap and build (minimum viable loop)

1. `./scripts/dev-setup.sh` — ESP-IDF default `v5.3.2` → `$HOME/esp-idf`, then `install.sh` for the target.
2. `./scripts/build.sh` — `export.sh`, `idf.py set-target esp32s3`, `idf.py build`.

Overrides: `ESP_IDF_VERSION`, `ESP_IDF_PATH`, `IDF_TARGET` (same defaults as `README.md`). Example: `ESP_IDF_PATH=/opt/esp-idf ./scripts/build.sh`.

Missing `export.sh` → run `dev-setup.sh` or fix `ESP_IDF_PATH`.

## 2) “Starting” the firmware (hardware / serial)

1. USB + serial port (often `/dev/ttyUSB0` or `/dev/ttyACM0`).
2. ESP-IDF shell: `source "$ESP_IDF_PATH/export.sh"`.

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

**No hardware:** require `./scripts/build.sh`; say explicitly that flash/monitor was not run.

## 3) Feature flags and configuration (Kconfig / sdkconfig)

Toggles: **`main/Kconfig.projbuild`** (LV demos, frame buffer, BT defaults, fonts, …).

Flow: `source "$ESP_IDF_PATH/export.sh"` → `idf.py set-target esp32s3` → `idf.py menuconfig` (**Example Configuration**) → `idf.py build` (or `./scripts/build.sh` after export).

Defaults for clones/CI: `sdkconfig.defaults`, `sdkconfig.defaults.esp32s3`.

**Mock / stub without device:** Kconfig or tight compile-time guards in the touched file; rebuild. Strip debug spam before merge unless requested.

## 4) Workflows by codebase area

| Area | Paths | Run | Test / verify |
|------|--------|-----|----------------|
| **Build + wiring** | `CMakeLists.txt`, `main/CMakeLists.txt`, `scripts/*.sh`, `partitions.csv` | `./scripts/build.sh`; optional `shellcheck scripts/*.sh` | Clean build; CMake/source list edits → no missing symbols. |
| **Components** | `main/idf_component.yml`, `dependencies.lock` | `./scripts/build.sh` | Resolves deps; lockfile diffs minimal and intentional. |
| **Sdkconfig** | `sdkconfig`, `sdkconfig.defaults*`, `sdkconfig.old` | `idf.py menuconfig` + build | Commit only intentional sdkconfig lines. |
| **App / UART** | `main/main.c` | build; device: `idf.py -p PORT flash monitor` | HW: CRC/telemetry sane; monitor not spammy. |
| **UI / LVGL** | `main/ui/**`, `main/LVGL_Driver/**`, `main/ui/boot/**` | build; device: flash + monitor | HW: boot UI, widgets, tearing/perf OK. |
| **Drivers** | `main/LCD_Driver/**`, `main/Touch_Driver/**`, `main/I2C_Driver/**`, `main/EXIO/**`, `main/Buzzer/**`, `main/SD_Card/**`, `main/PCF85063/**` | build; device: flash + monitor | HW: init logs + peripherals behave. |

**Full clean when builds act “stuck” after config or generator changes:**

```bash
source "$ESP_IDF_PATH/export.sh"
idf.py fullclean
./scripts/build.sh
```

## 5) Updating this skill

On a new repeatable trick: add **one** bullet or extend the matching table row (**symptom → command/file → how verified**); remove stale text in the same edit. Broader contributor docs → `README.md`; keep this file as the **short agent command path** only.
