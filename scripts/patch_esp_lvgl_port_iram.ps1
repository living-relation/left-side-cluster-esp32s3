# Apply IRAM_ATTR to esp_lvgl_port RGB vsync (after component manager fetch).
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$LvglDisp = Join-Path $ProjectRoot "managed_components\espressif__esp_lvgl_port\src\lvgl9\esp_lvgl_port_disp.c"
if (-not (Test-Path $LvglDisp)) {
    Write-Host "SKIP patch: $LvglDisp not found yet (run idf.py build first)"
    exit 0
}
$c = Get-Content -Raw -Path $LvglDisp
if ($c -match "IRAM_ATTR lvgl_port_flush_rgb_vsync_ready_callback") {
    Write-Host "OK  esp_lvgl_port IRAM patch already present"
    exit 0
}
$c = $c -replace "static bool lvgl_port_flush_rgb_vsync_ready_callback", "static bool IRAM_ATTR lvgl_port_flush_rgb_vsync_ready_callback"
Set-Content -Path $LvglDisp -Value $c -NoNewline
Write-Host "OK  applied IRAM_ATTR to esp_lvgl_port_disp.c"
