---
name: cloud-agent-starter
description: Use when starting work in this ESP-IDF firmware repo from Cursor Cloud and needing practical setup, build, flash, monitor, feature-flag, and testing workflows by codebase area.
---

# Cloud Agent Starter (ESP32-S3 Gauge Cluster)

Minimal runbook for Cloud agents. Use this first, then dive into deeper skills.

## 0) Fast start (do this in order)

1. Confirm repo root:
   - `pwd` should be this project root.
2. Confirm auth/tooling state:
   - `git status`
   - `gh auth status` (optional; useful for PR/CI inspection)
3. Bootstrap ESP-IDF once:
   - `./scripts/dev-setup.sh`
4. Build firmware:
   - `./scripts/build.sh`

Notes:
- No app-level login is required for firmware runtime.
- `dev-setup.sh` clones and installs ESP-IDF (`v5.3.2` by default).
- `build.sh` sources `export.sh`, sets target (`esp32s3`), and builds.

## 1) Common environment controls

Environment variables used by repo scripts:
- `ESP_IDF_VERSION` (default `v5.3.2`)
- `ESP_IDF_PATH` (default `$HOME/esp-idf`)
- `IDF_TARGET` (default `esp32s3`)

Examples:
- `ESP_IDF_PATH=/opt/esp-idf ./scripts/build.sh`
- `IDF_TARGET=esp32s3 ./scripts/dev-setup.sh`

If `./scripts/build.sh` says ESP-IDF is missing, run `./scripts/dev-setup.sh` first.

## 2) Workflows by codebase area

### A. Build/config system (`CMakeLists.txt`, `main/CMakeLists.txt`, `sdkconfig*`, `scripts/*`)

Use when changing component lists, dependencies, target settings, or build scripts.

Run:
1. `./scripts/build.sh`
2. If config changed, run `idf.py menuconfig` (after ESP-IDF export) and rebuild.
3. For clean rebuilds: `idf.py fullclean && ./scripts/build.sh`

Test workflow:
- Primary gate: successful `idf.py build`.
- Confirm expected sources/dependencies are present in `main/CMakeLists.txt`.
- Watch for generated file churn after builds (`sdkconfig`, `sdkconfig.old`, `dependencies.lock`); commit only intentional config/dependency updates.

### B. App logic + telemetry parsing (`main/main.c`)

Use when changing UART packet parsing, CRC logic, mapping, label updates, timers.

Run:
1. `./scripts/build.sh`
2. Hardware path: `idf.py -p <PORT> flash monitor`
3. Feed real/simulated UART frames at the configured UART settings in `main.c`.

Test workflow:
- Compile gate: build passes.
- Runtime gate (hardware): verify labels/arcs update and monitor output has no CRC spam.
- If debugging parser issues, temporarily enable extra logging, validate in monitor, then remove debug-only edits before final commit.

### C. UI + LVGL assets (`main/ui/**`, `main/LVGL_Driver/**`)

Use when changing generated UI screens, styles, fonts, or LVGL integration.

Run:
1. `./scripts/build.sh`
2. Hardware path: `idf.py -p <PORT> flash monitor`

Test workflow:
- Compile gate: no unresolved symbols from generated UI files.
- Runtime gate (hardware): confirm screen renders, widgets update, no visible flicker/tearing regressions.
- Keep generated asset changes grouped in commits so reviewers can isolate logic vs generated code.

### D. Hardware drivers (`main/LCD_Driver/**`, `main/Touch_Driver/**`, `main/I2C_Driver/**`, `main/EXIO/**`, `main/Buzzer/**`, `main/SD_Card/**`)

Use when changing peripheral initialization or device communication.

Run:
1. `./scripts/build.sh`
2. Hardware path: `idf.py -p <PORT> flash monitor`

Test workflow:
- Compile gate: build succeeds with driver changes.
- Runtime gate (hardware): verify boot/init sequence in monitor logs and that peripherals respond (display, touch, etc.).
- Keep init order checks focused around `app_main()` in `main/main.c`.

## 3) Feature flags and mocking patterns

### Kconfig-style flags

Repo flags live in `main/Kconfig.projbuild` (for example: frame-buffer and LV demo options).

Workflow:
1. `idf.py menuconfig`
2. Toggle flags under the project config menus.
3. `idf.py build`
4. If a default should be versioned, propagate into `sdkconfig.defaults` / `sdkconfig.defaults.esp32s3`.

### Code-level debug toggles

For short-term diagnostics (example: parser logging), use small local compile-time toggles in the touched module, validate, then remove before final commit unless the flag is intended to be permanent.

### Hardware-missing fallback

In Cloud environments without serial hardware access:
- Treat `./scripts/build.sh` as the hard gate.
- Document exactly which runtime checks still require on-device validation.
- Do not claim flash/monitor validation was completed unless it actually ran.

## 4) Practical test matrix (copy/paste)

- Build-only sanity:
  - `./scripts/build.sh`
- Config regression:
  - `idf.py menuconfig`
  - `idf.py build`
- Full device loop:
  - `idf.py -p <PORT> flash monitor`

Use the smallest workflow that proves your change, but always run at least one concrete command.

## 5) Keep this skill fresh (short maintenance loop)

Whenever you discover a new runbook trick (setup fix, monitor filter, config gotcha, faster test path):

1. Add a short entry in the relevant area above.
2. Include:
   - symptom
   - exact command/change
   - how it was validated
3. Remove stale steps immediately when scripts or layout change.

Keep this file minimal and executable: every command here should be something a new Cloud agent can run right away.
