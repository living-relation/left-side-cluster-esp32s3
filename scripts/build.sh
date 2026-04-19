#!/usr/bin/env bash

set -euo pipefail

ESP_IDF_PATH="${ESP_IDF_PATH:-$HOME/esp-idf}"
IDF_TARGET="${IDF_TARGET:-esp32s3}"

if [ ! -f "${ESP_IDF_PATH}/export.sh" ]; then
  echo "ESP-IDF not found at ${ESP_IDF_PATH}."
  echo "Run ./scripts/dev-setup.sh first, or set ESP_IDF_PATH."
  exit 1
fi

# shellcheck source=/dev/null
source "${ESP_IDF_PATH}/export.sh"

echo "Building firmware for target: ${IDF_TARGET}"
idf.py set-target "${IDF_TARGET}"
idf.py build

echo
echo "Build complete. Firmware output is in ./build/"
