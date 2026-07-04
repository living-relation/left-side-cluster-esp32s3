# Build-only verification (no flash / no USB). Run from project root:
#   .\scripts\verify_build_no_flash.ps1
#   .\scripts\verify_build_no_flash.ps1 -SkipBuild   # kconfig checks only
param([switch]$SkipBuild)
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

function Test-KconfigEnabled {
    param([string]$Path, [string]$Symbol)
    $text = Get-Content -Raw -Path $Path
    if ($text -notmatch "(?m)^${Symbol}=y") {
        throw "FAIL: $Path missing ${Symbol}=y (RGB ISR IRAM + esp_lvgl_port IRAM_ATTR patch required)"
    }
    Write-Host "OK  $Path - ${Symbol}=y"
}

Test-KconfigEnabled (Join-Path $ProjectRoot "sdkconfig.defaults") "CONFIG_LCD_RGB_ISR_IRAM_SAFE"
$LvglDisp = Join-Path $ProjectRoot "managed_components\espressif__esp_lvgl_port\src\lvgl9\esp_lvgl_port_disp.c"
if (-not (Select-String -Path $LvglDisp -Pattern "IRAM_ATTR lvgl_port_flush_rgb_vsync_ready_callback" -Quiet)) {
    throw "FAIL: esp_lvgl_port RGB vsync callback missing IRAM_ATTR patch"
}
Write-Host "OK  esp_lvgl_port_disp.c - RGB vsync IRAM_ATTR patch present"

if ($SkipBuild) {
    Write-Host "PASS - kconfig checks only (-SkipBuild)."
    exit 0
}

# Auto-discover the ESP-IDF toolchain (see idf_env_workspace.ps1).
. (Join-Path $PSScriptRoot "idf_env_workspace.ps1")
$py = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"
$idfPy = Join-Path $env:IDF_PATH "tools\idf.py"
& $py $idfPy reconfigure build
if ($LASTEXITCODE -ne 0) { throw "idf.py build failed with exit $LASTEXITCODE" }

$CmakeConfig = Join-Path $ProjectRoot "build\config\sdkconfig.cmake"
if (-not (Test-Path $CmakeConfig)) { throw "Missing $CmakeConfig after build" }
$cmake = Get-Content -Raw $CmakeConfig
if ($cmake -notmatch 'set\(CONFIG_LCD_RGB_ISR_IRAM_SAFE "y"\)') {
    throw "FAIL: built config missing CONFIG_LCD_RGB_ISR_IRAM_SAFE=y"
}
Write-Host "OK  build/config - CONFIG_LCD_RGB_ISR_IRAM_SAFE enabled"

$Bin = Join-Path $ProjectRoot "build\left_cluster.bin"
if (-not (Test-Path $Bin)) { throw "Missing firmware binary $Bin" }
Write-Host "OK  $Bin exists ($((Get-Item $Bin).Length) bytes)"
Write-Host "PASS - virtual build verification complete (no device flash)."
