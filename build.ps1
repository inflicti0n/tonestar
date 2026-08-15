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

    foreach ($year in @("2022", "18")) {
        foreach ($edition in @("Community", "Professional", "Enterprise", "BuildTools")) {
            $fallback = "C:\Program Files\Microsoft Visual Studio\$year\$edition\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $fallback) {
                return $fallback
            }
        }
    }

    throw "CMake not found. Install Visual Studio with the CMake workload, or add cmake to PATH."
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

$buildDir = Join-Path $Root "build"
$exe = Join-Path $Root "build\ToneStar_artefacts\Release\ToneStar.exe"
Get-Process ToneStar, Constellation -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

if (Test-FileLocked $exe) {
    throw "Close ToneStar.exe (it is still locked) and run build again."
}

& $cmake -S $Root -B $buildDir
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }

& $cmake --build $buildDir --config Release --target ToneStar
if ($LASTEXITCODE -ne 0) { throw "Release build failed ($LASTEXITCODE)" }

if (-not (Test-Path $exe)) {
    throw "Built exe missing: $exe"
}

$exeItem = Get-Item $exe
Write-Host ""
Write-Host ("exe  {0:N0} bytes  {1}" -f $exeItem.Length, $exeItem.FullName)
Write-Host "Done."
