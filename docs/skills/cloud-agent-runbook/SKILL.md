# Cloud Agent Runbook — ESP32-S3 Gauge Cluster Firmware

**Use this skill** when starting any work on this repository as a Cloud agent: building, changing firmware behaviour, editing the LVGL UI, modifying drivers, or debugging a build failure.

---

## 1. What this project is

ESP-IDF (CMake) firmware for an **ESP32-S3** with a round LCD. It reads a UART telemetry stream (2 Mbaud, pin 44) and drives an LVGL gauge cluster display via the ST7701S LCD panel and GT911 touch controller. There are no web services, no databases, and no automated test suite — verification is **build-time** (compiler + linker) plus **hardware** (flash → observe on device).

---

## 2. Environment setup (run once per Cloud agent VM)

```bash
./scripts/dev-setup.sh
```

What it does:
- Checks for `python3` and `python3-venv`; installs `python3.12-venv` via `apt` if missing.
- Clones ESP-IDF at `$HOME/esp-idf` (shallow, tag `v5.3.2`) unless it already exists.
- Runs `$HOME/esp-idf/install.sh esp32s3` to download toolchains and host tools.

Override defaults with environment variables:

| Variable | Default | Notes |
|---|---|---|
| `ESP_IDF_VERSION` | `v5.3.2` | Tag to clone |
| `ESP_IDF_PATH` | `$HOME/esp-idf` | Where ESP-IDF lives |
| `IDF_TARGET` | `esp32s3` | Must match the board |

> **Version note:** `dependencies.lock` records IDF 5.5.0 in its metadata while the script defaults to v5.3.2. If you see component-manager warnings or API mismatches, set `ESP_IDF_VERSION=v5.5.0` before running setup.

---

## 3. Building

```bash
./scripts/build.sh
```

What it does: sources `$ESP_IDF_PATH/export.sh`, runs `idf.py set-target esp32s3`, then `idf.py build`. Output lands in `./build/`.

For **incremental** work once ESP-IDF is sourced in the shell:

```bash
source $HOME/esp-idf/export.sh
idf.py build
```

Clean build (resolves stale CMake cache):

```bash
idf.py fullclean && idf.py build
```

A successful build ends with a line like:
```
Project build complete. To flash, run:
 idf.py flash
```

The three key output files are `./build/Left-Side-Cluster-ESP32-S3.bin`, `./build/bootloader/bootloader.bin`, and `./build/partition_table/partition-table.bin`.

---

## 4. Flash and monitor (requires physical hardware)

Cloud agents cannot flash remotely. These commands are here for reference and for human operators:

```bash
source $HOME/esp-idf/export.sh
idf.py -p /dev/ttyUSB0 flash        # replace port as needed
idf.py -p /dev/ttyUSB0 monitor      # Ctrl+] to exit
```

The `.vscode/settings.json` `idf.port` key records the last-used port on the developer's machine.

---

## 5. Compile-time configuration (feature flags)

This project uses **Kconfig** — there is no SaaS feature-flag service. All toggles live in `sdkconfig` (generated, do not commit unless intentional) and their defaults in `sdkconfig.defaults` (committed).

### Interactive configuration

```bash
source $HOME/esp-idf/export.sh
idf.py menuconfig
```

### Key options — `main/Kconfig.projbuild` (menu: "Example Configuration")

| Config symbol | Purpose | Default |
|---|---|---|
| `CONFIG_EXAMPLE_DOUBLE_FB` | Allocate two frame buffers for the LCD | n |
| `CONFIG_EXAMPLE_USE_BOUNCE_BUFFER` | Higher PCLK, more CPU; mutually exclusive with double FB | y |
| `CONFIG_EXAMPLE_AVOID_TEAR_EFFECT_WITH_SEM` | Semaphore-based vsync; mutually exclusive with double FB | y |
| `CONFIG_LV_USE_DEMO_WIDGETS` | Enable LVGL widget demo screens | y |
| `CONFIG_LV_USE_DEMO_BENCHMARK` | LVGL benchmark mode | y |
| `CONFIG_BT_ENABLED` | Bluetooth stack | y |

### Editing `sdkconfig` directly (CI / scripted)

Append or replace a line in `sdkconfig` using `sed` or a Python script. Example — disable BT to reduce binary size:

```bash
grep -q "^CONFIG_BT_ENABLED=" sdkconfig \
  && sed -i 's/^CONFIG_BT_ENABLED=.*/CONFIG_BT_ENABLED=n/' sdkconfig \
  || echo "CONFIG_BT_ENABLED=n" >> sdkconfig
```

Then rebuild. Kconfig propagates dependent symbols automatically on the next build.

### In-code compile-time flag

`main/main.c` line 32 controls UART payload logging:

```c
#define ENABLE_LOGGING 0   // set to 1 to emit ESP_LOGI for each received packet
```

Change this when debugging UART packet parsing; revert before committing.

---

## 6. Codebase areas and testing workflows

### 6.1 Core application logic — `main/main.c`

Responsibilities: UART init, packet framing + CRC-CCITT validation, gauge state globals, LVGL timer callbacks, `app_main`.

**Packet structure** (`gauge_payload_t`, 26 bytes total):
- Byte 0: SOF `0xA5`
- Byte 1: reserved
- Bytes 2–23: `gauge_payload_t` (packed struct: oil_temp, water_temp, oil_pressure, fuel_pressure, fuel_level, afr, boost, lap_time_ms, lap_delta_ms)
- Bytes 24–25: CRC-16/CCITT over bytes 1–22

**Testing main.c changes (build-only):**
1. `idf.py build` — confirms no syntax or type errors.
2. Enable `ENABLE_LOGGING 1`, flash, and watch `idf.py monitor` for `RX:` log lines with real telemetry.
3. To test colour-threshold logic (e.g. `update_gauge_values`) without hardware: temporarily set the global state vars (`g_water_temp`, etc.) to boundary values in `app_main` before `uart_init()`, build, and verify in monitor output or visually on screen.

### 6.2 LCD driver — `main/LCD_Driver/ST7701S.c`

Drives the ST7701S panel over the RGB parallel interface. Config: 480×480, SPI init sequence, backlight PWM.

**Testing:** build-only for code changes. Hardware verification: after flash, backlight should come on after ~750 ms (`Set_Backlight` call in `app_main`). A white or garbled screen indicates an init sequence regression.

### 6.3 Touch driver — `main/Touch_Driver/GT911.c`

GT911 capacitive touch over I2C. Integrates with `esp_lcd_touch` abstraction layer (`Touch_Driver/esp_lcd_touch/`).

**Testing:** build-only. Hardware: touch events surface as LVGL input events. Add a temporary `ESP_LOGI` in the read callback to confirm coordinates.

### 6.4 LVGL integration — `main/LVGL_Driver/LVGL_Driver.c`

Manages the LVGL tick, display driver registration, and flush callback to the ST7701S DMA. Frame buffer mode is controlled by `CONFIG_EXAMPLE_DOUBLE_FB` and `CONFIG_EXAMPLE_USE_BOUNCE_BUFFER`.

**Testing:** a clean build is the primary gate. On hardware, LVGL benchmark mode (`CONFIG_LV_USE_DEMO_BENCHMARK=y`) measures FPS and can be used to catch regressions in buffer configuration.

### 6.5 UI screens — `main/ui/`

Generated by SquareLine Studio. `ui_Screen1.c` is the main gauge screen. Key exported objects: `ui_Label5` (water temp), `ui_Label6` (fuel pressure), `ui_Label7` (oil temp), `ui_Label8` (oil pressure), `fuel_arc` (fuel level arc), `low_gas_img` (low-fuel warning icon).

**Testing UI changes:**
1. `idf.py build` — catches LVGL API mismatches (common after regenerating from SquareLine).
2. Flash and visually inspect the screen on hardware.
3. For label colour-threshold changes: set globals to boundary values before `uart_init()` to force a specific display state during boot, then flash and observe.

> **Caution:** `main/ui/screens/ui_Screen1.c` and sibling files under `main/ui/` are regenerated by SquareLine Studio. Hand edits will be overwritten. Place custom logic in `main/main.c` timer callbacks, not in the generated files.

### 6.6 I2C and GPIO expander — `main/I2C_Driver/`, `main/EXIO/`

I2C bus init and TCA9554 GPIO expander (controls LCD reset, backlight enable, and other board signals).

**Testing:** build-only. Hardware: if EXIO init fails, the LCD stays blank and `idf.py monitor` prints an ESP-IDF error from `I2C_Init` or `EXIO_Init`.

### 6.7 Peripheral drivers — `main/SD_Card/`, `main/Buzzer/`, `main/PCF85063/`

SD card (SDMMC, FAT), buzzer (ledc PWM), and PCF85063 RTC. These are not currently wired into `app_main` UI flows but their component sources are compiled.

**Testing:** build inclusion is sufficient. Unused `REQUIRES` entries (`esp_wifi`, `nvs_flash`) are compiled but do not affect binary correctness.

---

## 7. Common workflows

### Add a new gauge value

1. Add the field to `gauge_payload_t` in `main.c` (keep `__attribute__((packed))`; update `PKT_LEN` if the struct size changes).
2. Add a global state variable (e.g. `static int g_new_value = 0`).
3. Populate it in `uart_rx_task` after `memcpy`.
4. Add an LVGL label/arc object reference in `ui_Screen1.c` (or regenerate from SquareLine).
5. Call `update_label_if_needed` in `update_gauge_values`.
6. `idf.py build` to verify.

### Change warning thresholds

All thresholds are inline in `update_gauge_values()` and `isFuelPressureOK()` in `main.c`. Edit and rebuild.

### Change flash partition layout

Edit `partitions.csv` and rebuild. The factory app partition is `0xF00000` (15 MB); the FAT `flash_test` partition is at the end. Ensure total does not exceed 16 MB (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`).

### Diagnose a UART packet issue

1. Set `#define ENABLE_LOGGING 1` in `main.c`.
2. Build and flash.
3. Run `idf.py monitor` — each valid packet prints `RX: Oil ... Water ... AFR ... Boost ...`. A `CRC fail` log indicates framing drift.
4. Revert `ENABLE_LOGGING` to `0` before committing.

---

## 8. What "testing" means in this project

There are no automated tests (no Unity test components, no pytest, no CI pipelines). The verification matrix is:

| Check | How |
|---|---|
| Compilation | `idf.py build` exits 0 |
| Linker | Binary and map file produced in `./build/` |
| Runtime correctness | Flash + visual/serial inspection on hardware |
| Regression | Compare LVGL FPS via benchmark mode before/after |

For Cloud agents without physical hardware access: **a clean `idf.py build` is the completeness gate**. Document any hardware-only verification steps in the PR description.

---

## 9. Keeping this skill up to date

When you discover a new trick, gotcha, or runbook step — add it here. Guidelines:

- **Add to the relevant section** rather than appending at the bottom.
- **One concrete example** is worth more than a paragraph of prose. Prefer shell commands and file paths over descriptions.
- **Update the table in §8** if a new verification method is introduced (e.g. a Unity test component, a CI job).
- Commit the skill update in the same PR as the code change that prompted it, with a commit message like `docs: update cloud-agent runbook — add X`.
- If a section becomes stale (e.g. a driver is removed, a script is renamed), delete or rewrite it — stale instructions are worse than no instructions.
