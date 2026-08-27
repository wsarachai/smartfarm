<#
.SYNOPSIS
    Flash an F103 bring-up build and stream its semihosting output over the ST-Link.

.DESCRIPTION
    The F103 test/diagnostic envs print through ARM semihosting (a BKPT the
    debugger services), so their output only appears while OpenOCD is attached
    with `arm semihosting enable`. This wraps both halves: upload, then watch.

    Only one process can hold the ST-Link at a time, so the script stops any
    OpenOCD left running from a previous watch before it does anything.

.PARAMETER PioEnv
    PlatformIO env to flash and watch. Defaults to the DS18B20 test.
      bluepill_f103c8_semihosting  two DS18B20 probes, ~1 Hz   (default)
      bluepill_f103c8_dump         raw scratchpad + pin diagnostics
      bluepill_f103c8_node         full LoRa node (DS18B20 + BME280 -> SX1278)
      bluepill_f103c8_sx1278       SX1278 radio self-test

.PARAMETER NoUpload
    Skip the build/flash step and just attach to whatever is already on the board.

.EXAMPLE
    .\tools\watch.ps1
.EXAMPLE
    .\tools\watch.ps1 -PioEnv bluepill_f103c8_dump
.EXAMPLE
    .\tools\watch.ps1 -NoUpload

.NOTES
    Ctrl+C stops the watch and leaves the target halted at the last semihosting
    breakpoint; re-running the script resets and resumes it.
#>
[CmdletBinding()]
param(
    [string]$PioEnv = 'bluepill_f103c8_semihosting',
    [switch]$NoUpload
)

$ErrorActionPreference = 'Stop'
$projectDir = Split-Path -Parent $PSScriptRoot

# PlatformIO installs its tools under ~/.platformio and does not add them to PATH,
# so prefer those and fall back to anything the user has installed themselves.
$pio = Join-Path $HOME '.platformio\penv\Scripts\pio.exe'
if (-not (Test-Path $pio)) { $pio = (Get-Command pio -ErrorAction SilentlyContinue).Source }
if (-not $pio) { throw 'pio.exe not found (looked in ~/.platformio/penv/Scripts and on PATH).' }

$ocdRoot = Join-Path $HOME '.platformio\packages\tool-openocd'
$openocd = Join-Path $ocdRoot 'bin\openocd.exe'
$scripts = Join-Path $ocdRoot 'openocd\scripts'
if (-not (Test-Path $openocd)) {
    $openocd = (Get-Command openocd -ErrorAction SilentlyContinue).Source
    $scripts = $null      # a PATH openocd knows its own script dir
}
if (-not $openocd) { throw 'openocd.exe not found. Flash once with pio to install tool-openocd.' }

# The ST-Link takes one client at a time: a leftover watch blocks the next upload
# with "shutdown error".
$stale = Get-Process openocd -ErrorAction SilentlyContinue
if ($stale) {
    Write-Host "Stopping $($stale.Count) leftover openocd process(es)..." -ForegroundColor Yellow
    $stale | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}

if (-not $NoUpload) {
    Write-Host "Flashing $PioEnv ..." -ForegroundColor Cyan
    & $pio run -d $projectDir -e $PioEnv -t upload
    if ($LASTEXITCODE -ne 0) { throw "Upload failed (exit $LASTEXITCODE)." }
}

Write-Host "Watching $PioEnv - Ctrl+C to stop." -ForegroundColor Cyan
$ocdArgs = @()
if ($scripts) { $ocdArgs += @('-s', $scripts) }
$ocdArgs += @(
    '-f', 'interface/stlink.cfg',
    '-f', 'target/stm32f1x.cfg',
    '-c', 'init',
    '-c', 'arm semihosting enable',
    '-c', 'reset run'
)
& $openocd @ocdArgs
