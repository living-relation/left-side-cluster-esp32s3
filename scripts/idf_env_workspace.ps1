# Workspace-local ESP-IDF environment (no flash). Dot-source before idf.py:
#   . .\scripts\idf_env_workspace.ps1
$ProjectRoot = Split-Path -Parent $PSScriptRoot

if (-not $env:IDF_PATH) { $env:IDF_PATH = "C:\esp\v5.4.2\esp-idf" }
$env:IDF_TOOLS_PATH = Join-Path $ProjectRoot "tools\espidf"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v5.4.2\venv"

$XtensaBin = "C:\Espressif\tools\tools\xtensa-esp-elf\esp-14.2.0_20241119\xtensa-esp-elf\bin"
$CmakeBin   = "C:\Espressif\tools\tools\cmake\3.30.2\bin"
$NinjaDir   = "C:\Espressif\tools\tools\ninja\1.12.1"
foreach ($dir in @($XtensaBin, $CmakeBin, $NinjaDir)) {
    if (Test-Path $dir) { $env:Path = "$dir;$env:Path" }
}
