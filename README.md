Esp32 Round Display for a digital gauge cluster

This project is source-available and free for personal and educational use.
Commercial use is NOT permitted.

### *****Please do not use these files to sell this to others.  I made these so that those wanting to modify their dash/cluster dont have to spend an arm and a leg to do so.***** ###


https://youtu.be/t7H6pevep40

1. Install [visual studio code](https://code.visualstudio.com/Download)
2. Install the ESP-IDF extension(shown here: https://www.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-3.4C#Introduction_to_ESP-IDF_and_Environment_Setup_.28VSCode_Column.29)
    * Open VSCode Package Manager
    * Search for the official ESP-IDF ide extension
3. Open this repo folder in VS code
4. Plug in Esp32s3/screen via usb c
5. Set target device to esp32s3(see "Description of Bottom Toolbar of VSCode User Interface" in the waveshare link above)
6. Build 

7. Flash

## Cursor Cloud / headless setup

If you are running in a Linux terminal environment (including Cursor Cloud), use:

1. `./scripts/dev-setup.sh`
2. `./scripts/build.sh`

Optional environment variables:
- `ESP_IDF_VERSION` (default: `v5.3.2`)
- `ESP_IDF_PATH` (default: `$HOME/esp-idf`)
- `IDF_TARGET` (default: `esp32s3`)
