#!/usr/bin/env bash

set -euo pipefail

ESP_IDF_VERSION="${ESP_IDF_VERSION:-v5.3.2}"
ESP_IDF_PATH="${ESP_IDF_PATH:-$HOME/esp-idf}"
IDF_TARGET="${IDF_TARGET:-esp32s3}"

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required but not found." >&2
  exit 1
fi

if ! python3 -m venv --help >/dev/null 2>&1; then
  echo "Installing python venv support (python3.12-venv)..."
  sudo apt-get update
  sudo apt-get install -y python3.12-venv
fi

if [ ! -d "$ESP_IDF_PATH/.git" ]; then
  echo "Cloning ESP-IDF ${ESP_IDF_VERSION} into ${ESP_IDF_PATH}"
  git clone --depth 1 --branch "${ESP_IDF_VERSION}" https://github.com/espressif/esp-idf.git "$ESP_IDF_PATH"
else
  echo "ESP-IDF already exists at ${ESP_IDF_PATH}; reusing it."
fi

echo "Installing ESP-IDF tools for target: ${IDF_TARGET}"
"${ESP_IDF_PATH}/install.sh" "${IDF_TARGET}"

echo
echo "Setup complete."
echo "To build now, run:"
echo "  ./scripts/build.sh"
