# MidiSpace installer
# Installs the VST3 plugin and the bundled inference server (with model).
# Run as administrator.

$ErrorActionPreference = 'Stop'

$scriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$vst3Src     = Join-Path $scriptDir 'MidiSpace.vst3'
$serverSrc   = Join-Path $scriptDir 'midispace_server.exe'
$ckptSrc     = Join-Path $scriptDir 'checkpoints'
$vst3Dst     = 'C:\Program Files\Common Files\VST3\MidiSpace.vst3'
$serverDst   = "$env:ProgramData\MidiSpace\server"
$ckptDst     = "$env:ProgramData\MidiSpace\server\checkpoints"

Write-Host '=== MidiSpace installer ===' -ForegroundColor Cyan

Write-Host "Installing VST3 -> $vst3Dst" -ForegroundColor Green
if (Test-Path $vst3Dst) { Remove-Item $vst3Dst -Recurse -Force }
Copy-Item $vst3Src $vst3Dst -Recurse -Force

Write-Host "Installing server -> $serverDst" -ForegroundColor Green
New-Item -ItemType Directory -Force -Path $serverDst | Out-Null
Copy-Item $serverSrc (Join-Path $serverDst 'midispace_server.exe') -Force

Write-Host "Installing model -> $ckptDst" -ForegroundColor Green
New-Item -ItemType Directory -Force -Path $ckptDst | Out-Null
Copy-Item "$ckptSrc\*" $ckptDst -Recurse -Force

Write-Host ''
Write-Host 'Done. MidiSpace is installed.' -ForegroundColor Green
Write-Host 'The inference server starts automatically when the plugin loads.'
Write-Host 'Press Enter to exit.'
Read-Host
