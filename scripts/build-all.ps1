<#
.SYNOPSIS
  Clean, reproducible MSVC build of PixelCSS: configure, build and test x64 Debug and Release, then package the
  Release ZIP and verify its layout (SC-001, FR-044, FR-048).

.PARAMETER Force
  Remove the out/ directory without asking.

.PARAMETER KeepOut
  Do not remove out/ (incremental rebuild).

.PARAMETER Perf
  Also run the performance gate (ctest --preset x64-release-perf).
#>
param([switch]$Force, [switch]$KeepOut, [switch]$Perf)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$results = [ordered]@{}
function Step([string]$name, [scriptblock]$action) {
    Write-Host "==> $name" -ForegroundColor Cyan
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & $action
    if ($LASTEXITCODE -ne 0) {
        $results[$name] = "FAILED (exit $LASTEXITCODE)"
        Show-Summary
        throw "$name failed with exit code $LASTEXITCODE"
    }
    $results[$name] = "ok ({0:N0}s)" -f $sw.Elapsed.TotalSeconds
}
function Show-Summary {
    Write-Host ""
    Write-Host "Summary" -ForegroundColor Cyan
    foreach ($k in $results.Keys) { "{0,-34} {1}" -f $k, $results[$k] | Write-Host }
}

if (-not $KeepOut -and (Test-Path out)) {
    if (-not $Force) {
        $answer = Read-Host "Remove '$root\out' for a clean build? [y/N]"
        if ($answer -notmatch '^(y|yes)$') { Write-Host "Aborted."; exit 1 }
    }
    Remove-Item -Recurse -Force out
}

foreach ($cfg in 'x64-debug', 'x64-release') {
    Step "configure $cfg" { cmake --preset $cfg }
    Step "build $cfg"     { cmake --build --preset $cfg -- /m /v:minimal }
    Step "test $cfg"      { ctest --preset $cfg }
}
Step "package (cpack)"      { cpack --preset x64-release }
Step "package layout check" { ctest --preset x64-release-package }
if ($Perf) { Step "performance gate" { ctest --preset x64-release-perf } }

Show-Summary
Write-Host "Package: $root\out\package" -ForegroundColor Green
