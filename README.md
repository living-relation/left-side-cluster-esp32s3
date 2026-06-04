# TrackCluster — Left Cluster (ESP32-S3)

Firmware for the **left** instrument-cluster display: a 480×480 round ST7701S panel on a
**Waveshare ESP32-S3-Touch-LCD-2.8C**. Shows the **speedometer** plus four mini-gauges (oil temp,
oil pressure, fuel pressure, fuel level).

This board is a **pure consumer** — it receives display data from the center cluster over a single
UART wire. No CAN, no ECU connection here.

One of three projects: **center-cluster-esp32-p4** (brain) · **left-side-cluster-esp32s3** (this) ·
**right-side-cluster-esp32s3**.

---

## Flash it (quick start in VS Code)

Full beginner walkthrough: **[`SETUP_BEFORE_YOU_BUILD.txt`](SETUP_BEFORE_YOU_BUILD.txt)**.

Short version:
1. Install **VS Code** + the **Espressif IDF** extension; run
   *Command Palette → “ESP-IDF: Configure ESP-IDF Extension” → Express → 5.4.2*.
2. **File → Open Folder →** this repo.
3. Set target = **esp32s3** (chip icon, bottom bar).
4. Plug the board in via USB-C and pick the **COM port** on the bottom bar.
5. **Build → Flash → Monitor** (flame icon).

Ships in **BENCH MODE** — runs a self-test demo with nothing wired up so you can confirm the screen
works standalone. Turn off later via *menuconfig → TrackCluster → Bench mode*.

### Key settings (already in `sdkconfig.defaults`)
| Setting | Value |
|---|---|
| Target | esp32s3 |
| Flash size | 16 MB (confirm; set 8 MB if your board is smaller) |
| PSRAM | Octal, 80 MHz (**required** — the framebuffer lives here) |
| Bench mode | ON (self-test) |
| ESP-IDF | 5.4.2 (matches the center board's toolchain) |

No baud rate to set — see the setup guide.

---

## Wiring (this board)

Powered from the same 12 V→5 V buck as the others; one data wire from the center.

| Pin | Connect to |
|---|---|
| **5V** | buck converter 5 V output |
| **GND** | common ground (shared with center + buck) |
| **GPIO44 (UART RX)** | center cluster **UART1 TX = GPIO20** |

That single UART wire (+ common ground) is all the data this screen needs. The **full harness
diagram** (power, CAN, both side links, buttons/encoders) lives in the center repo’s `WIRING.md`.

---

## What’s in the build
```
main/            firmware source (the only thing compiled/flashed)
  ui/            LVGL screens + fonts + Toyota logo
sdkconfig.defaults   board config (PSRAM, flash, bench mode)
partitions.csv       flash layout
CMakeLists.txt       project file
```
