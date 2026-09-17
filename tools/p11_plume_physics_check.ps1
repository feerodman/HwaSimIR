param(
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $root ("logs\p11\plume_physics\" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
elseif (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path $root $OutputDirectory
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$compiler = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {
    throw "Missing compiler: $compiler"
}

$profilePath = Join-Path $root "HwaSim_IR\Bin\Config\IRPlume\engine_plume_profiles.json"
$modelHeader = Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IREnginePlumeModel.h"
$modelSource = Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IREnginePlumeModel.cpp"
$radianceSource = Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IRRadianceModelV2.cpp"
$temperatureSource = Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IRTemperatureModel.cpp"
$unitSource = Join-Path $root "tools\p11_plume_physics_unit.cpp"
$civilOpticsPath = Join-Path $root "HwaSim_IR\Bin\Config\TargetLib\p11\civil_van\p11_civil_van_band_optics.csv"
$unitExe = Join-Path $OutputDirectory "p11_plume_physics_unit.exe"
$unitCsv = Join-Path $OutputDirectory "p11_plume_physics_qc.csv"

$profile = Get-Content -LiteralPath $profilePath -Raw -Encoding UTF8 | ConvertFrom-Json
if ([int]$profile.version -lt 3) { throw "P11 plume profile schema must be version 3 or later" }
$layers = @($profile.defaults.core, $profile.defaults.halo)
foreach ($platformProperty in $profile.platforms.PSObject.Properties) {
    $layers += @($platformProperty.Value.core, $platformProperty.Value.halo)
}
foreach ($layer in $layers) {
    if ($null -eq $layer.bandEmissivity) { throw "Every plume layer must define bandEmissivity" }
    if ($null -ne $layer.bandGain) { throw "Production P11 profile must not define legacy bandGain" }
    foreach ($bandProperty in $layer.bandEmissivity.PSObject.Properties) {
        $value = [double]$bandProperty.Value
        if ([double]::IsNaN($value) -or [double]::IsInfinity($value) -or $value -lt 0.0 -or $value -gt 1.0) {
            throw "Invalid bandEmissivity $($bandProperty.Name)=$value; expected 0..1"
        }
    }
}

$civilProfile = $profile.platforms.'P11-CIVIL-VAN'
if ($null -eq $civilProfile) { throw "Missing explicit P11-CIVIL-VAN plume profile" }
if (-not [bool]$civilProfile.enabledByEngineState) {
    throw "P11-CIVIL-VAN plume must require engineState"
}
if ([double]$civilProfile.core.temperatureK -ge 700.0 -or
    [double]$civilProfile.core.lengthM -ge 1.0 -or
    [double]$civilProfile.halo.lengthM -ge 2.0) {
    throw "P11-CIVIL-VAN profile has non-civil/default-scale exhaust values"
}
$modelSourceText = Get-Content -LiteralPath $modelSource -Raw -Encoding UTF8
if ($modelSourceText -notmatch '"P11-CIVIL-VAN"') {
    throw "IREnginePlumeModel parser whitelist does not include P11-CIVIL-VAN"
}
$civilOptics = @(Import-Csv -LiteralPath $civilOpticsPath)
$tailpipe = @($civilOptics | Where-Object Region -eq 'exhaust_tailpipe')
if ($tailpipe.Count -ne 1 -or
    [double]$tailpipe[0].NominalTemperatureK -ne 303.0 -or
    [double]$tailpipe[0].EngineOnTemperatureK -ne 475.0) {
    throw "Civil solid tailpipe 303/475 K material contract changed or missing"
}

& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic `
    -I (Join-Path $root "HwaSim_IR\HwaSim_IR\IR") `
    $unitSource $modelSource $radianceSource $temperatureSource `
    -o $unitExe
if ($LASTEXITCODE -ne 0) { throw "P11 plume physics unit compilation failed" }

& $unitExe $profilePath $unitCsv
if ($LASTEXITCODE -ne 0) { throw "P11 plume physics unit failed" }

$rows = @(Import-Csv -LiteralPath $unitCsv)
$failures = @($rows | Where-Object { $_.status -ne "PASS" })
if ($failures.Count -gt 0) {
    $failures | Format-Table -AutoSize
    throw "P11 plume physics QC contains failed rows"
}

$summary = [ordered]@{
    schema = "HwaSimIR.P11.PlumePhysicsCheck.v1"
    generatedUtc = [DateTime]::UtcNow.ToString("o")
    status = "PASS"
    checks = $rows.Count
    swirRangeUm = @(1.10, 2.50)
    mwirRangeUm = @(3.00, 5.00)
    radianceUnit = "W/(m^2 sr um)"
    integrationReference = "independent composite Simpson 16384 intervals"
    productionIntegration = "IRRadianceModelV2 shared band-mean Planck implementation"
    sourceRadianceDefinition = "bandEmissivity * max(B_band(T) - B_band(Tambient), 0)"
    emittedRadianceDefinition = "opacity * sourceRadiance"
    opacityApplicationCount = 1
    civilVan = [ordered]@{
        profileKey = "P11-CIVIL-VAN"
        engineStateRequired = $true
        tailpipeSolidTemperatureK = [ordered]@{ off = 303.0; on = 475.0 }
        plumeCoreTemperatureK = [double]$civilProfile.core.temperatureK
        plumeHaloTemperatureK = [double]$civilProfile.halo.temperatureK
        plumeCoreLengthM = [double]$civilProfile.core.lengthM
        plumeHaloLengthM = [double]$civilProfile.halo.lengthM
        localPosM = @([double]$civilProfile.localPos[0], [double]$civilProfile.localPos[1], [double]$civilProfile.localPos[2])
        assumption = [string]$civilProfile.assumption
    }
    profileSha256 = (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash
    modelHeaderSha256 = (Get-FileHash -LiteralPath $modelHeader -Algorithm SHA256).Hash
    modelSourceSha256 = (Get-FileHash -LiteralPath $modelSource -Algorithm SHA256).Hash
    csv = $unitCsv
}
$summaryPath = Join-Path $OutputDirectory "p11_plume_physics_summary.json"
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "P11 plume physics QC PASS"
Write-Host "checks=$($rows.Count)"
Write-Host "qcCsv=$unitCsv"
Write-Host "summary=$summaryPath"
