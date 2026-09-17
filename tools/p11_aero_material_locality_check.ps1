param(
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $root ("logs\p11\aero_material_locality\" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
elseif (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path $root $OutputDirectory
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$compiler = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw "Missing compiler: $compiler" }
$modelSource = Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IRAeroThermalModel.cpp"
$unitSource = Join-Path $root "tools\p11_aero_local_unit.cpp"
$unitExe = Join-Path $OutputDirectory "p11_aero_local_unit.exe"
$unitCsv = Join-Path $OutputDirectory "p11_aero_local_qc.csv"
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic `
    -I (Join-Path $root "HwaSim_IR\HwaSim_IR\IR") `
    $unitSource $modelSource -o $unitExe
if ($LASTEXITCODE -ne 0) { throw "P11 aero locality unit compilation failed" }
$unitText = & $unitExe | Out-String
if ($LASTEXITCODE -ne 0) { throw "P11 aero locality unit failed`n$unitText" }
[IO.File]::WriteAllText($unitCsv, $unitText, [Text.UTF8Encoding]::new($false))

$sourcePath = Join-Path $root "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$source = Get-Content -LiteralPath $sourcePath -Raw -Encoding UTF8
$required = @(
    'stage5Input\.bodyAeroDeltaK\s*=\s*0\.0',
    'input\.aeroDeltaK\s*=\s*0\.0',
    'u_stage5_aero_local_en',
    'nose_mask\s*=\s*smoothstep',
    'edge_mask\s*=\s*smoothstep',
    'rear_mask\s*=\s*smoothstep',
    'aeroDistribution=gpu_local_bounds_normalized'
)
foreach ($pattern in $required) {
    if ($source -notmatch $pattern) { throw "Missing production locality guard: $pattern" }
}

$xmlPath = Join-Path $root "HwaSim_IR\Bin\Config\TargetLib\p11\civil_van\p11_civil_van_material_id.pgm.xml"
[xml]$xml = Get-Content -LiteralPath $xmlPath -Raw -Encoding UTF8
$materials = @($xml.Composite_Material_Table.Composite_Material)
if ($materials.Count -ne 6) { throw "Expected six civil material partitions" }
foreach ($material in $materials) {
    $swirSum = [double]$material.SWIRReflectance + [double]$material.SWIREmissivity + [double]$material.SWIRTransmissivity
    $mwirSum = [double]$material.MWIRReflectance + [double]$material.MWIREmissivity + [double]$material.MWIRTransmissivity
    if ([math]::Abs($swirSum - 1.0) -gt 1.0e-6 -or [math]::Abs($mwirSum - 1.0) -gt 1.0e-6) {
        throw "Material ID $($material.index) violates band energy balance"
    }
    if ([double]$material.NominalTemperatureK -lt 120.0) { throw "Missing material temperature for ID $($material.index)" }
}
$engineBay = @($materials | Where-Object { [int]$_.index -eq 15 })[0]
$tailpipe = @($materials | Where-Object { [int]$_.index -eq 16 })[0]
if ([double]$engineBay.EngineOnTemperatureK -ne 345.0 -or [double]$tailpipe.EngineOnTemperatureK -ne 475.0) {
    throw "Civil engine-local temperature contract mismatch"
}

$rows = @(Import-Csv -LiteralPath $unitCsv)
$summary = [ordered]@{
    schema = "HwaSimIR.P11.AeroMaterialLocality.v1"
    generatedUtc = [DateTime]::UtcNow.ToString("o")
    status = "PASS"
    numericalChecks = $rows.Count
    materialPartitions = $materials.Count
    wholeBodyAeroOffsetK = 0.0
    aeroDistribution = "GPU local, model-bounds normalized nose/edge/rear masks"
    materialOpticsSource = "model XML engineering assumptions, not visible RGB"
    temperaturePolicy = "per material; engine bay and tailpipe switch only with engineState"
    unitCsv = $unitCsv
    sourceSha256 = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
    materialXmlSha256 = (Get-FileHash -LiteralPath $xmlPath -Algorithm SHA256).Hash
}
$summaryPath = Join-Path $OutputDirectory "p11_aero_material_locality_summary.json"
[IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
Write-Host "P11 aero/material locality PASS"
Write-Host "checks=$($rows.Count) materials=$($materials.Count)"
Write-Host "summary=$summaryPath"
