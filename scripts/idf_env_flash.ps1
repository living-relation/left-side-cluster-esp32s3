# ESP-IDF environment for build/flash. Dot-source from other scripts:
#   . .\scripts\idf_env_flash.ps1
. (Join-Path $PSScriptRoot "idf_env_workspace.ps1")

$RiscvBin = "C:\Espressif\tools\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin"
if (Test-Path $RiscvBin) { $env:Path = "$RiscvBin;$env:Path" }
