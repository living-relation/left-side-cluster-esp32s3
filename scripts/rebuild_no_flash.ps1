# Clean rebuild in this workspace only (no USB flash).
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectRoot

. (Join-Path $PSScriptRoot "idf_env_workspace.ps1")

$StaleBuild = Join-Path $ProjectRoot "build\CMakeCache.txt"
if (Test-Path $StaleBuild) {
    $cache = Get-Content -Raw $StaleBuild
    if ($cache -match "left-side-cluster-esp32s3" -and $cache -notmatch [regex]::Escape($ProjectRoot)) {
        Remove-Item -LiteralPath (Join-Path $ProjectRoot "build") -Recurse -Force
        Write-Host "Removed stale build/ (CMakeCache from another project path)."
    }
}
# Recover from interrupted ninja (truncated .obj under lvgl)
$LvglBuild = Join-Path $ProjectRoot "build\esp-idf\lvgl__lvgl"
if (Test-Path $LvglBuild) {
    $trunc = Get-ChildItem $LvglBuild -Recurse -Filter "*.obj" -ErrorAction SilentlyContinue |
        Where-Object { $_.Length -lt 64 }
    if ($trunc) {
        Remove-Item -LiteralPath $LvglBuild -Recurse -Force
        Write-Host "Removed partial lvgl__lvgl build (truncated objects)."
    }
}

$py = "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe"
$idfPy = "$env:IDF_PATH\tools\idf.py"
New-Item -ItemType Directory -Force -Path (Join-Path $ProjectRoot "build\log") | Out-Null
# Relax -ErrorAction Stop around the native call: cmake/idf.py stderr warnings
# would otherwise become a terminating error on Windows PowerShell 5.1 when
# merged via "2>&1". Gate on the real exit code instead.
$eap = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
& $py $idfPy build 2>&1 | Tee-Object -FilePath (Join-Path $ProjectRoot "build\log\rebuild_no_flash.log")
$buildExit = $LASTEXITCODE
$ErrorActionPreference = $eap
if ($buildExit -ne 0) { throw "idf.py build failed with exit $buildExit" }

& (Join-Path $PSScriptRoot "verify_build_no_flash.ps1") -SkipBuild
$CmakeConfig = Join-Path $ProjectRoot "build\config\sdkconfig.cmake"
$cmake = Get-Content -Raw $CmakeConfig
if ($cmake -notmatch 'set\(CONFIG_LCD_RGB_ISR_IRAM_SAFE "y"\)') {
    throw "Built config missing CONFIG_LCD_RGB_ISR_IRAM_SAFE=y"
}
Write-Host "PASS - rebuild_no_flash complete."
