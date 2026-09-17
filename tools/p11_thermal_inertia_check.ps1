param(
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repo ("logs\p11\thermal_inertia\" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
elseif (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path $repo $OutputDirectory
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$compiler = 'D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw "Missing compiler: $compiler" }
$exe = Join-Path $OutputDirectory 'p11_thermal_inertia_unit.exe'
$csv = Join-Path $OutputDirectory 'p11_thermal_inertia_qc.csv'
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic `
    -I (Join-Path $repo 'HwaSim_IR\HwaSim_IR\IR') `
    (Join-Path $repo 'tools\p11_thermal_inertia_unit.cpp') `
    (Join-Path $repo 'HwaSim_IR\HwaSim_IR\IR\IRMaterialThermalState.cpp') `
    (Join-Path $repo 'HwaSim_IR\HwaSim_IR\IR\IRRadianceModelV2.cpp') `
    (Join-Path $repo 'HwaSim_IR\HwaSim_IR\IR\IRTypes.cpp') `
    -o $exe
if ($LASTEXITCODE -ne 0) { throw 'P11 thermal inertia unit compilation failed' }
$unitText = & $exe | Out-String
if ($LASTEXITCODE -ne 0) { throw "P11 thermal inertia unit failed; see $csv" }
[IO.File]::WriteAllText($csv, $unitText, [Text.UTF8Encoding]::new($false))
$rows = @(Import-Csv -LiteralPath $csv)
if ($rows.Count -lt 10 -or @($rows | Where-Object status -ne 'PASS').Count -ne 0) {
    throw 'P11 thermal inertia QC rows incomplete or failed'
}
$summary = [ordered]@{
    schema = 'HwaSimIR.P11.ThermalInertia.v1'
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    result = 'PASS'
    checks = $rows.Count
    model = 'first-order six-normal-bin surface energy balance'
    interpretation = 'engineering proxy; not CFD or measured target calibration'
    separation = 'solar reflection immediate; solar temperature and MWIR emission stateful'
    csv = $csv
    sourceHashes = [ordered]@{
        unit = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $repo 'tools\p11_thermal_inertia_unit.cpp')).Hash
        thermalModel = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $repo 'HwaSim_IR\HwaSim_IR\IR\IRMaterialThermalState.cpp')).Hash
        radianceModel = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $repo 'HwaSim_IR\HwaSim_IR\IR\IRRadianceModelV2.cpp')).Hash
    }
}
$summaryPath = Join-Path $OutputDirectory 'p11_thermal_inertia_summary.json'
[IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
Write-Host "P11 thermal inertia PASS: $($rows.Count) checks"
Write-Host "summary=$summaryPath"
