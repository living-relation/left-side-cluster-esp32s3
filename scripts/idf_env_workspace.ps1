# Workspace-local ESP-IDF environment (no flash). Dot-source before idf.py:
#   . .\scripts\idf_env_workspace.ps1
#
# Auto-discovers the installed ESP-IDF toolchain from the ESP-IDF Installation
# Manager registry (eim_idf.json) instead of hard-coding one PC's folder layout.
# Resolution order:
#   1. $env:IDF_ACTIVATION_SCRIPT  - explicit activation .ps1 override
#   2. An already-working environment (e.g. the ESP-IDF VS Code terminal / CI)
#   3. eim_idf.json  -> selected install, else newest v5.4.x, else newest valid
#   4. Legacy hard-coded fallback paths (older manual all-in-one installs)
# The chosen install's activation script sets IDF_PATH, IDF_TOOLS_PATH,
# IDF_PYTHON_ENV_PATH and the full toolchain PATH (xtensa/riscv/cmake/ninja).

$ProjectRoot = Split-Path -Parent $PSScriptRoot

function Test-TcIdfReady {
    # The flash/build helpers call "<python> <idf.py>" directly and shell out to
    # cmake, so all three must be resolvable for the environment to be usable.
    return (
        $env:IDF_PATH -and (Test-Path (Join-Path $env:IDF_PATH 'tools\idf.py')) -and
        $env:IDF_PYTHON_ENV_PATH -and (Test-Path (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe')) -and
        [bool](Get-Command cmake -ErrorAction SilentlyContinue)
    )
}

function Import-TcActivation {
    param([string]$Script)
    if ($Script -and (Test-Path $Script)) {
        # Tolerate a corrupt/partial install: on failure report it and return
        # false so the caller falls through to the next candidate / fallback
        # instead of aborting the whole environment setup.
        try { . $Script *> $null }
        catch { Write-Warning "ESP-IDF activation failed ($Script): $($_.Exception.Message)"; return $false }
        return $true
    }
    return $false
}

# 1) Explicit activation-script override.
if ($env:IDF_ACTIVATION_SCRIPT) { [void](Import-TcActivation $env:IDF_ACTIVATION_SCRIPT) }

# 2) Environment already usable (ESP-IDF terminal, CI, or override above).
if (Test-TcIdfReady) { return }

# 3) Discover via the ESP-IDF Installation Manager registry (eim_idf.json).
# Build candidates guardedly - Join-Path throws on an empty/unset base path.
$eimCandidates = @()
if ($env:IDF_TOOLS_PATH) { $eimCandidates += (Join-Path $env:IDF_TOOLS_PATH 'eim_idf.json') }
$eimCandidates += 'C:\Espressif\tools\eim_idf.json'
if ($env:USERPROFILE)  { $eimCandidates += (Join-Path $env:USERPROFILE '.espressif\tools\eim_idf.json') }
if ($env:LOCALAPPDATA) { $eimCandidates += (Join-Path $env:LOCALAPPDATA 'Espressif\tools\eim_idf.json') }
if ($env:APPDATA)      { $eimCandidates += (Join-Path $env:APPDATA 'Espressif\tools\eim_idf.json') }
$eimCandidates = $eimCandidates | Where-Object { Test-Path $_ }

foreach ($eim in $eimCandidates) {
    try { $reg = Get-Content -Raw $eim | ConvertFrom-Json } catch { continue }
    $installs = @($reg.idfInstalled) | Where-Object { $_ -and $_.path }
    if (-not $installs) { continue }

    # Sort by parsed [version] so e.g. v5.4.10 ranks above v5.4.2 (a plain
    # lexical name sort would order those backwards).
    $verKey = {
        $n = ($_.name -replace '^[vV]', '') -replace '[^0-9.].*$', ''
        if (-not $n) { $n = '0.0' }
        try { [version]$n } catch { [version]'0.0' }
    }
    # Priority: selected id -> newest v5.4.x -> newest of anything, de-duped.
    $seen = New-Object 'System.Collections.Generic.HashSet[string]'
    $ordered = New-Object 'System.Collections.Generic.List[object]'
    foreach ($cand in @(
        ($installs | Where-Object { $_.id -eq $reg.idfSelectedId } | Select-Object -First 1)
        ($installs | Where-Object { $_.name -like 'v5.4*' } | Sort-Object $verKey -Descending)
        ($installs | Sort-Object $verKey -Descending)
    )) {
        foreach ($i in @($cand)) {
            if (-not $i) { continue }
            $key = "$($i.id)|$($i.path)"
            if ($seen.Add($key)) { $ordered.Add($i) }
        }
    }

    foreach ($idf in $ordered) {
        if (-not (Test-Path (Join-Path $idf.path 'tools\idf.py'))) { continue }
        if (Import-TcActivation $idf.activationScript) {
            if (Test-TcIdfReady) { return }
        }
        # Activation script missing/incomplete: set vars and fall back to export.ps1.
        $env:IDF_PATH = $idf.path
        if ($idf.idfToolsPath) { $env:IDF_TOOLS_PATH = $idf.idfToolsPath }
        if ($idf.python) { $env:IDF_PYTHON_ENV_PATH = Split-Path -Parent (Split-Path -Parent $idf.python) }
        $export = Join-Path $idf.path 'export.ps1'
        if (Test-Path $export) { . $export *> $null }
        if (Test-TcIdfReady) { return }
    }
}

# 4) Legacy fallback for older manual installs (pre-EIM layout).
$legacyCandidates = @('C:\esp\v5.4.2\esp-idf')
if ($env:USERPROFILE) { $legacyCandidates += (Join-Path $env:USERPROFILE 'esp\v5.4.2\esp-idf') }
$legacy = $legacyCandidates | Where-Object { Test-Path (Join-Path $_ 'tools\idf.py') } | Select-Object -First 1
if ($legacy) {
    $env:IDF_PATH = $legacy
    $export = Join-Path $legacy 'export.ps1'
    if (Test-Path $export) { . $export *> $null }
    if (Test-TcIdfReady) { return }
}

throw "ESP-IDF 5.4.x not found. Launch the ESP-IDF terminal, or set `$env:IDF_ACTIVATION_SCRIPT to an activation .ps1 (see SETUP_BEFORE_YOU_BUILD.txt)."
