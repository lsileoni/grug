param(
    [switch]$Debug,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Run
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$buildDir = Join-Path $root "build"

$clang = (Get-Command clang -ErrorAction SilentlyContinue).Source
if (-not $clang) { $clang = "C:\Program Files\LLVM\bin\clang.exe" }
if (-not (Test-Path $clang)) { throw "clang not found. Install LLVM or add it to PATH." }

$sources = Get-ChildItem -Path (Join-Path $root "src") -Recurse -Filter *.c | ForEach-Object { $_.FullName }
$out = Join-Path $buildDir "grug.exe"

if ($Debug) {
    $flags = @("-std=c11", "-O0", "-g", "-Wall", "-Wextra")
} else {
    $flags = @("-std=c11", "-O2", "-DNDEBUG", "-Wall", "-Wextra", "-mpopcnt")
}

Write-Host "Compiling $($sources.Count) files with clang..." -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
& $clang @flags @sources -o $out
if ($LASTEXITCODE -ne 0) { throw "build failed" }
Write-Host "Built $out" -ForegroundColor Green

if ($Run) { & $out @Run }
