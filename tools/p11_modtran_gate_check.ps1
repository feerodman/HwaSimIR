param(
    [string]$OutputDirectory = "logs/p11/modtran_gate"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory
} else {
    Join-Path $root $OutputDirectory
}
New-Item -ItemType Directory -Force -Path $out | Out-Null

$compiler = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
if (-not (Test-Path -LiteralPath $compiler)) {
    throw "Missing compiler: $compiler"
}

$probeExe = Join-Path $out "p11_modtran_gate_unit.exe"
$probeLog = Join-Path $out "p11_modtran_gate_unit.log"
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic `
    -I (Join-Path $root "HwaSim_IR\HwaSim_IR\IR") `
    (Join-Path $root "tools\p11_modtran_gate_unit.cpp") `
    (Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IRModtranRadianceLut.cpp") `
    -o $probeExe
if ($LASTEXITCODE -ne 0) {
    throw "P11 MODTRAN gate unit compilation failed"
}

$probeOutput = & $probeExe $out | Out-String
$probeExit = $LASTEXITCODE
[IO.File]::WriteAllText($probeLog, $probeOutput, [Text.UTF8Encoding]::new($false))
Write-Host $probeOutput.TrimEnd()
if ($probeExit -ne 0) {
    throw "P11 MODTRAN gate unit failed; see $probeLog"
}

# The renderer is not instantiated in this small unit executable.  Keep a strict
# source-wiring gate beside the numeric LUT tests so a later refactor cannot
# accidentally re-open the legacy image path on an invalid formal query.
$appPath = Join-Path $root "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$appText = Get-Content -LiteralPath $appPath -Raw -Encoding UTF8
$sourceChecks = [ordered]@{
    missing_band_init_rejected = $appText.Contains("reason=formal_atmosphere_band_missing")
    formal_fail_closed_decision = $appText.Contains("const bool formalM1FailClosed")
    formal_fail_closed_black = $appText.Contains("action=black_target_no_legacy")
    formal_fail_closed_no_legacy = $appText.Contains("legacyRendered=0")
    formal_fail_closed_display_gate = $appText.Contains("m_enableStage5RadianceDebug || stage5Input.m1RuntimeAffectsImage || formalM1FailClosed")
    stale_m1_uniform_cleared = $appText.Contains('SetShaderInputCached(targetPlat.nodePath, "u_m1_physics_runtime_en", LVecBase2i(0, 0))')
}
$failedSourceChecks = @($sourceChecks.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object { $_.Key })
$summary = [ordered]@{
    schema = "HwaSimIR.P11.ModtranFormalGateCheck.v1"
    status = if ($failedSourceChecks.Count -eq 0) { "PASS" } else { "FAIL" }
    unitLog = $probeLog
    sourceChecks = $sourceChecks
    failures = $failedSourceChecks
}
$summaryPath = Join-Path $out "p11_modtran_gate_summary.json"
[IO.File]::WriteAllText(
    $summaryPath,
    (($summary | ConvertTo-Json -Depth 5) + "`n"),
    [Text.UTF8Encoding]::new($false))
if ($failedSourceChecks.Count -ne 0) {
    throw "P11 formal fail-closed source gate failed: $($failedSourceChecks -join ', ')"
}
Write-Host "P11 MODTRAN/formal fail-closed gate PASS: $summaryPath"
