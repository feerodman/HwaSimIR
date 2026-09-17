[CmdletBinding()]
param(
    [string]$OutputDirectory = 'logs/p11/work/controlled_samples_integration'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $root $OutputDirectory }
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Assert-Contains([string]$Text, [string]$Needle, [string]$Message) {
    if (-not $Text.Contains($Needle)) { throw $Message }
}

$rendererProtocol = Get-Content -LiteralPath (Join-Path $root 'HwaSim_IR\HwaSim_IR\Common\CommonData.h') -Raw
$senderProtocol = Get-Content -LiteralPath (Join-Path $root 'DataDrivenTestQT\CommonData.h') -Raw
if ((($rendererProtocol -replace "`r`n", "`n").TrimEnd()) -cne
    (($senderProtocol -replace "`r`n", "`n").TrimEnd())) {
    throw 'Sender and renderer protocol headers differ'
}

$rendererSource = Get-Content -LiteralPath (Join-Path $root 'HwaSim_IR\HwaSim_IR\HwaSimIR.cpp') -Raw
$senderSource = Get-Content -LiteralPath (Join-Path $root 'DataDrivenTestQT\mainwindow.cpp') -Raw
$fixtureParserSource = Get-Content -LiteralPath (Join-Path $root 'DataDrivenTestQT\OrdinaryWeatherInput.h') -Raw
$annotationSource = Get-Content -LiteralPath (Join-Path $root 'HwaSim_IR\HwaSim_IR\Annotation\AnnotationConfig.cpp') -Raw
$annotationProfile = Get-Content -LiteralPath (Join-Path $root 'HwaSim_IR\Bin\Config\Annotation\annotation_profiles.json') -Raw
$targetsText = Get-Content -LiteralPath (Join-Path $root 'HwaSim_IR\Bin\Config\TargetLib\Targets.json') -Raw

Assert-Contains $rendererSource 'case 0x66: return Resv2;' 'Renderer does not map protocol type 0x66 to Resv2'
Assert-Contains $rendererSource 'm_platformResMap[Resv2]' 'Renderer has no additive Resv2 resource mapping'
Assert-Contains $rendererSource 'Config/TargetLib/p11/controlled_samples/p11_controlled_samples.bam' 'Renderer does not load controlled sample BAM'
Assert-Contains $rendererSource 'm_initSceneData.MissileMaxCountResv2' 'Renderer does not honor Resv2 capacity'
Assert-Contains $rendererSource '[P11 ControlledSamplePool] targetType=0x66 platform=Resv2' 'Renderer controlled-pool audit log missing'
Assert-Contains $rendererSource 'm_platformResMap[Resv1]' 'Independent 0x55 civil mapping was lost'
Assert-Contains $rendererSource 'targetPlat.targetState.targetPlatID == targetState.targetPlatID' 'Target mapping no longer checks targetPlatID'
Assert-Contains $rendererSource 'targetPlat.targetState.targetID == targetState.targetID' 'Target mapping no longer checks targetID'
Assert-Contains $senderSource 'testTarget==0x66' 'Stimulus does not permit explicit 0x66 input'
Assert-Contains $senderSource 'cmd.MissileMaxCountResv2 = p11TargetTypeOk && p11TargetType == 0x66 ? 1 : 0;' 'Stimulus does not provision exactly one explicit Resv2 slot'
Assert-Contains $senderSource 'cmd.MissileMaxCountResv1 = p11TargetTypeOk && p11TargetType == 0x55 ? 1 : 0;' 'Stimulus 0x55 isolation was changed'
Assert-Contains $fixtureParserSource 'LookAtTargetKey must exactly match a SyntheticTelemetryTargets full key' 'Fixture full-key guard missing'
Assert-Contains $annotationSource 'case 0x66: return Resv2;' 'Annotation identity does not recognize 0x66'
Assert-Contains $annotationSource 'm_configs[Resv2]' 'Annotation Resv2 fallback/profile mapping missing'
Assert-Contains $annotationProfile '"P11ControlledSamples"' 'Annotation JSON identity missing'
Assert-Contains $targetsText '"protocol_target_type":"0x66"' 'Target catalog 0x66 identity missing'

$assetDir = Join-Path $root 'HwaSim_IR\Bin\Config\TargetLib\p11\controlled_samples'
$requiredAssets = @(
    'p11_controlled_samples.obj', 'p11_controlled_samples.egg', 'p11_controlled_samples.bam',
    'p11_controlled_samples_visible.ppm', 'p11_controlled_samples_material_id.pgm',
    'p11_controlled_samples_material_id.pgm.xml', 'p11_controlled_samples_band_optics.csv',
    'manifest.json', 'README.md', 'LICENSE.txt'
)
foreach ($name in $requiredAssets) {
    if (-not (Test-Path -LiteralPath (Join-Path $assetDir $name) -PathType Leaf)) {
        throw "Controlled sample asset missing: $name"
    }
}

$fixturePath = Join-Path $root 'tools\p11_inputs\controlled_samples_close_lookat.json'
$fixtureBytes = [IO.File]::ReadAllBytes($fixturePath)
$fixture = Get-Content -LiteralPath $fixturePath -Raw | ConvertFrom-Json
if ($fixture.Schema -ne 'ordinary_weather_camera_1' -or
    @($fixture.SyntheticTelemetryTargets).Count -ne 1 -or @($fixture.LookAtTargetKey).Count -ne 3) {
    throw 'Controlled sample fixture schema/cardinality mismatch'
}
$target = @($fixture.SyntheticTelemetryTargets)[0]
$key = @($fixture.LookAtTargetKey)
if ([int]$target[0] -ne 0x66 -or [int]$key[0] -ne 0x66 -or
    [int]$target[0] -ne [int]$key[0] -or [int]$target[1] -ne [int]$key[1] -or
    [int]$target[2] -ne [int]$key[2] -or [int]$target[9] -ne 0 -or [int]$target[10] -ne 1) {
    throw 'Controlled sample fixture identity/full-key/view/engine-off contract mismatch'
}

$previousTime = -1.0
$ranges = @()
foreach ($frame in @($fixture.Keyframes)) {
    if (@($frame).Count -ne 7 -or [double]$frame[0] -le $previousTime) {
        throw 'Camera keyframes are not strictly ordered'
    }
    $previousTime = [double]$frame[0]
    $lat1 = [double]$frame[1] * [Math]::PI / 180.0
    $lat2 = [double]$target[3] * [Math]::PI / 180.0
    $dLat = $lat2 - $lat1
    $dLon = ([double]$target[4] - [double]$frame[2]) * [Math]::PI / 180.0
    $a = [Math]::Pow([Math]::Sin($dLat / 2.0), 2) + [Math]::Cos($lat1) * [Math]::Cos($lat2) * [Math]::Pow([Math]::Sin($dLon / 2.0), 2)
    $horizontalM = 6371008.8 * 2.0 * [Math]::Atan2([Math]::Sqrt($a), [Math]::Sqrt(1.0 - $a))
    $range = [Math]::Sqrt($horizontalM * $horizontalM + [Math]::Pow([double]$target[5] - [double]$frame[3], 2))
    # Formal P11 imagery must stay inside the audited 0.1-1.0 km MODTRAN grid.
    if ($range -lt 100.0 -or $range -gt 1000.0) { throw "Fixture range outside audited 0.1-1.0 km domain: $range" }
    $ranges += $range
}

$manifest = Get-Content -LiteralPath (Join-Path $assetDir 'manifest.json') -Raw | ConvertFrom-Json
$rackWidthM = [double]$manifest.nominalBoundsM.x[1] - [double]$manifest.nominalBoundsM.x[0]
$nearestRangeM = ($ranges | Measure-Object -Minimum).Minimum
$sensorPixelAngleRad = 100.0e-6
$estimatedWidthPixels = 2.0 * [Math]::Atan($rackWidthM / (2.0 * $nearestRangeM)) / $sensorPixelAngleRad
if ($estimatedWidthPixels -lt 100.0 -or $estimatedWidthPixels -gt 790.0) {
    throw "Controlled rack does not meet 100-790 px close-view design at 100 urad/pixel: $estimatedWidthPixels"
}

$materials = @(Import-Csv -LiteralPath (Join-Path $assetDir 'p11_controlled_samples_band_optics.csv'))
if ($materials.Count -ne 8 -or (($materials | ForEach-Object { [int]$_.MaterialId }) -join ',') -ne ((21..28) -join ',')) {
    throw 'Controlled rack material IDs are not exactly 21..28'
}
foreach ($row in $materials) {
    if ([Math]::Abs(([double]$row.SWIRReflectance + [double]$row.SWIREmissivity + [double]$row.SWIRTransmissivity) - 1.0) -gt 1e-9 -or
        [Math]::Abs(([double]$row.MWIRReflectance + [double]$row.MWIREmissivity + [double]$row.MWIRTransmissivity) - 1.0) -gt 1e-9) {
        throw "Material energy balance failed ID=$($row.MaterialId)"
    }
}

$compiler = 'D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw "Missing compiler: $compiler" }
$unitExe = Join-Path $out 'p11_controlled_samples_protocol_unit.exe'
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic `
    -I (Join-Path $root 'DataDrivenTestQT') `
    (Join-Path $root 'tools\p11_controlled_samples_protocol_unit.cpp') -o $unitExe
if ($LASTEXITCODE -ne 0) { throw 'Controlled sample protocol unit compilation failed' }
$unitText = (& $unitExe | Out-String).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Controlled sample protocol unit failed' }
$unit = $unitText | ConvertFrom-Json
if ($unit.result -ne 'PASS') { throw 'Controlled sample protocol unit result failed' }
[IO.File]::WriteAllText((Join-Path $out 'protocol_unit.json'), $unitText + "`n", [Text.UTF8Encoding]::new($false))

$assetHashes = foreach ($name in $requiredAssets) {
    $path = Join-Path $assetDir $name
    [ordered]@{ file=$name; bytes=(Get-Item -LiteralPath $path).Length; sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
}
$summary = [ordered]@{
    result = 'PASS'
    protocolLayout = $unit.protocol
    targetType = '0x66'
    platform = 'Resv2'
    displayName = 'P11-CONTROLLED-SAMPLES'
    capacitySource = 'InitP2cObjectTrackingCmd.MissileMaxCountResv2'
    defaultRunChanged = $false
    civilVanMappingPreserved = $true
    targetKey = @([int]$key[0], [int]$key[1], [int]$key[2])
    fullKeyCameraLock = $true
    engineState = 0
    frameOrdering = 'strictly_increasing_no_replay'
    rangesM = @($ranges | ForEach-Object { [Math]::Round($_, 3) })
    estimatedNearestRackWidthPixelsAt800 = [Math]::Round($estimatedWidthPixels, 2)
    fixture = [ordered]@{ path=$fixturePath; sha256=([Security.Cryptography.SHA256]::Create().ComputeHash($fixtureBytes) | ForEach-Object { $_.ToString('x2') }) -join '' }
    materialIds = @(21..28)
    energyBalance = 'PASS'
    assetHashes = $assetHashes
}
$summaryPath = Join-Path $out 'integration_check.json'
[IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
$summary | ConvertTo-Json -Depth 5 -Compress
