# Build firmware for morning flash (no USB). From project root:
#   .\scripts\prepare_flash.ps1
#   .\scripts\prepare_flash.ps1 -FullClean
param([switch]$FullClean)
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

. (Join-Path $PSScriptRoot "idf_env_flash.ps1")

$logDir = Join-Path $ProjectRoot "build\log"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$py = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"
$idfPy = Join-Path $env:IDF_PATH "tools\idf.py"

if ($FullClean) {
    & $py $idfPy fullclean
    if ($LASTEXITCODE -ne 0) { throw "idf.py fullclean failed: $LASTEXITCODE" }
}

& (Join-Path $PSScriptRoot "patch_esp_lvgl_port_iram.ps1")

$logFile = Join-Path $logDir "prepare_flash.log"
# idf.py/cmake write warnings to stderr; with the "*>" redirect under
# -ErrorAction Stop (Windows PowerShell 5.1) that stderr would surface as a
# terminating error. Relax to Continue for the native call and gate on the
# real process exit code instead.
$eap = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
& $py $idfPy build *> $logFile
$buildExit = $LASTEXITCODE
$ErrorActionPreference = $eap
if ($buildExit -ne 0) {
    Get-Content $logFile -Tail 30
    throw "idf.py build failed: $buildExit"
}

& (Join-Path $PSScriptRoot "verify_build_no_flash.ps1") -SkipBuild

$head = git -C $ProjectRoot rev-parse --short HEAD 2>$null
$bin = Join-Path $ProjectRoot "build\left_cluster.bin"
Write-Host ""
Write-Host "READY  left_cluster @ $head"
Write-Host "       $bin ($((Get-Item $bin).Length) bytes)"
Write-Host "Flash: .\scripts\flash_cluster.ps1 -Port COM5"
