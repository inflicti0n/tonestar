#Requires -Version 5.1
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path (Join-Path $Root "build.ps1"))) {
    $Root = Split-Path -Parent $PSScriptRoot
}
Set-Location $Root

$exe = Join-Path $Root "build\ToneStar_artefacts\Release\ToneStar.exe"
$logDir = Join-Path $env:APPDATA "ToneStar"
$startupLog = Join-Path $logDir "startup.log"
$crashLog = Join-Path $logDir "crash.log"
$crashDump = Join-Path $logDir "crash.dmp"
$selfTest = Join-Path $logDir "self-test.txt"
$timeoutSec = 45

function Get-FileStamp([string] $Path) {
    if (Test-Path $Path) {
        return (Get-Item $Path).LastWriteTimeUtc.Ticks
    }
    return 0
}

function Show-LogTail([string] $Path, [int] $Lines = 40) {
    if (-not (Test-Path $Path)) {
        Write-Host "missing $Path"
        return
    }
    Write-Host "---- $Path ----"
    Get-Content $Path -Tail $Lines
}

Write-Host "Building ToneStar..."
& (Join-Path $Root "build.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "build.ps1 failed ($LASTEXITCODE)"
}

if (-not (Test-Path $exe)) {
    throw "Built exe missing: $exe"
}

New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$crashBefore = Get-FileStamp $crashLog
$dumpBefore = Get-FileStamp $crashDump
if (Test-Path $selfTest) {
    Remove-Item $selfTest -Force
}

Write-Host "Launching --self-test..."
$proc = Start-Process -FilePath $exe -ArgumentList "--self-test" -PassThru -WorkingDirectory $Root
$finished = $proc.WaitForExit($timeoutSec * 1000)

if (-not $finished) {
    Write-Host "TIMEOUT after ${timeoutSec}s - killing ToneStar"
    try { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue } catch {}
    Show-LogTail $startupLog
    Show-LogTail $selfTest
    Show-LogTail $crashLog
    throw "ToneStar --self-test timed out"
}

$exit = $proc.ExitCode
Write-Host "exit code $exit"

$crashAfter = Get-FileStamp $crashLog
$dumpAfter = Get-FileStamp $crashDump
$crashed = ($crashAfter -gt $crashBefore) -or ($dumpAfter -gt $dumpBefore)

Show-LogTail $selfTest 80
Show-LogTail $startupLog 50

if ($crashed) {
    Show-LogTail $crashLog
    throw "ToneStar wrote a new crash log or dump"
}

if (-not (Test-Path $selfTest)) {
    throw "self-test.txt was not written"
}

$report = Get-Content $selfTest -Raw
if ($report -notmatch '(?m)^PASS\s*$') {
    throw "self-test.txt did not end with PASS"
}

if ($exit -ne 0) {
    throw "ToneStar --self-test exited $exit"
}

Write-Host "UI smoke passed."
