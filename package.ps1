#Requires -Version 5.1
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
Set-Location $Root

function Find-CMake {
    $fromPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($fromPath) {
        return $fromPath.Source
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $found = & $vswhere -latest -products * -find "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" |
            Select-Object -First 1
        if ($found -and (Test-Path $found)) {
            return $found
        }
    }

    foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
        $fallback = "C:\Program Files\Microsoft Visual Studio\2022\$edition\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $fallback) {
            return $fallback
        }
    }

    throw "CMake not found. Install Visual Studio 2022 with the CMake workload, or add cmake to PATH."
}

function Test-FileLocked([string] $Path) {
    if (-not (Test-Path $Path)) {
        return $false
    }

    try {
        $stream = [System.IO.File]::Open($Path, "Open", "ReadWrite", "None")
        $stream.Close()
        return $false
    } catch {
        return $true
    }
}

$cmake = Find-CMake
Write-Host "CMake: $cmake"

$exe = Join-Path $Root "build\GuitarMonitor_artefacts\Release\ToneStar.exe"
Get-Process ToneStar -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

if (Test-FileLocked $exe) {
    throw "Close ToneStar.exe (it is still locked) and run package again."
}

& $cmake -S $Root -B (Join-Path $Root "build")
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }

& $cmake --build (Join-Path $Root "build") --config Release --target GuitarMonitor
if ($LASTEXITCODE -ne 0) { throw "Release build failed ($LASTEXITCODE)" }

if (-not (Test-Path $exe)) {
    throw "Built exe missing: $exe"
}

$distDir = Join-Path $Root "dist\ToneStar"
$zip = Join-Path $Root "dist\ToneStar.zip"
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
Copy-Item $exe (Join-Path $distDir "ToneStar.exe") -Force

@'
ToneStar
========

Double-click ToneStar.exe. No install. Windows 10/11.

1. Devices: pick your interface as both input and output (ASIO if you have it).
2. Input: the channel your guitar is on.
3. Turn Direct Monitor OFF on the interface or you will hear double.
4. Shape the star. Drag FX handles out from the ring. Out is volume.
5. Right-click Bloom for Shimmer. Cab: wheel = size, click = jump, right-click = open/closed.
6. Presets (top left) grow right. X deletes a preset.
7. Advanced grows left into Tape: 8 lanes, play/pause/stop, rec, Q. Folder opens the tape.
8. Looper grows down. Space is the pedal only while the window is focused.

Share copies a tone slug (star, FX, shimmer, binaural, cab). In / Out / mute / devices stay as they are.

Tape clips land in Documents\ToneStar\tape
Settings save under %APPDATA%\ToneStar\
'@ | Set-Content -Encoding utf8 (Join-Path $distDir "README.txt")

if (Test-Path $zip) {
    Remove-Item $zip -Force
}
Compress-Archive -Path (Join-Path $distDir "*") -DestinationPath $zip -Force

$exeItem = Get-Item (Join-Path $distDir "ToneStar.exe")
$zipItem = Get-Item $zip
Write-Host ""
Write-Host ("exe  {0:N0} bytes  {1}" -f $exeItem.Length, $exeItem.FullName)
Write-Host ("zip  {0:N0} bytes  {1}" -f $zipItem.Length, $zipItem.FullName)
Write-Host "Done."
