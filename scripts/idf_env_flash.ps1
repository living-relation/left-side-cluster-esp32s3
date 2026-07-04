# ESP-IDF environment for build/flash. Dot-source from other scripts:
#   . .\scripts\idf_env_flash.ps1
#
# Delegates to idf_env_workspace.ps1, which auto-discovers the ESP-IDF install
# and puts the full toolchain PATH (xtensa/riscv/cmake/ninja/openocd/esptool)
# in scope. No extra flash-only setup is required beyond that.
. (Join-Path $PSScriptRoot "idf_env_workspace.ps1")
