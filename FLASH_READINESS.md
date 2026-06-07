# Left cluster — flash readiness

**Source tree (flash from here):** `C:\projects\left-side-cluster-esp32s3`  
**Git HEAD:** run `git rev-parse --short HEAD` after pull  
**Mode:** bench **OFF** — live UART from center (`# CONFIG_TC_BENCH_MODE is not set`)

## Unwired bench — flash this board alone

Nothing else needs to be connected. Plug **only** the left cluster via USB-C.

## Morning checklist

1. **ESP-IDF 5.4.2** — VS Code Espressif extension or `C:\esp\v5.4.2\esp-idf`
2. **USB** — left board only; note COM port (example: **COM4** — yours may differ)
3. **Target** — `esp32s3`
4. **Build + verify (no flash):**
   ```powershell
   cd C:\projects\left-side-cluster-esp32s3
   .\scripts\prepare_flash.ps1
   ```
   After `fullclean`, this re-applies the **esp_lvgl_port IRAM_ATTR** patch automatically.
5. **Flash:**
   ```powershell
   .\scripts\flash_cluster.ps1 -Port COM4
   ```
   Or skip rebuild if you just ran prepare: `.\scripts\flash_cluster.ps1 -Port COM4 -SkipBuild`

## What to expect (bench off, unwired)

- Toyota splash → speedo + mini arcs at **0** — **no demo sweep** (correct for live build)
- Center UART is not connected; sides will stay at zero until harness is wired
- Serial monitor: no `BENCH MODE` line
- **Blank screen after flash?** IRAM patch missing — rerun `prepare_flash.ps1`

## Settings (already in `sdkconfig.defaults`)

| Item | Value |
|------|--------|
| Target | esp32s3 |
| Flash | 16 MB |
| PSRAM | Octal 80 MHz (**required**) |
| LVGL buffer | 80-line partial RGB (matches right S3 panel path) |
| `CONFIG_LCD_RGB_ISR_IRAM_SAFE` | **y** (needs IRAM patch) |
| Bench mode | **off** |

## After you wire the car — reflash?

**No**, if this board already has the latest **bench-off** firmware. Connect **5 V**, **GND**, and
**GPIO44 ← center GPIO20** only. Reflash when you update firmware from git.

## Not in this firmware yet

Planned ECU alarm bytes (`0x3EF`, extended `0x3EE`) — see `st185-furyx-base-map\docs\CLUSTER_FIRMWARE_BACKLOG.md`.
