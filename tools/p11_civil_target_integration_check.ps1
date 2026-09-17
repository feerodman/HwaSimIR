[CmdletBinding()]
param(
    [string]$OutputDirectory = 'logs/p11/work/civil_integration'
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
$rendererProtocolNormalized = ($rendererProtocol -replace "`r`n", "`n").TrimEnd()
$senderProtocolNormalized = ($senderProtocol -replace "`r`n", "`n").TrimEnd()
if ($rendererProtocolNormalized -cne $senderProtocolNormalized) { throw 'Sender and renderer protocol headers differ' }

$rendererSource = Get-Content -LiteralPath (Join-Path $root 'HwaSim_IR\HwaSim_IR\HwaSimIR.cpp') -Raw
$senderSource = Get-Content -LiteralPath (Join-Path $root 'DataDrivenTestQT\mainwindow.cpp') -Raw
$fixtureSource = Get-Content -LiteralPath (Join-Path $root 'DataDrivenTestQT\OrdinaryWeatherInput.h') -Raw
$annotationSource = Get-Content -LiteralPath (Join-Path $root 'HwaSim_IR\HwaSim_IR\Annotation\AnnotationConfig.cpp') -Raw

Assert-Contains $rendererSource 'case 0x55: return Resv1;' 'Renderer does not map protocol type 0x55 to Resv1'
Assert-Contains $rendererSource 'm_platformResMap[Resv1]' 'Renderer has no additive Resv1 resource mapping'
Assert-Contains $rendererSource 'Config/TargetLib/p11/civil_van/p11_civil_van.bam' 'Renderer does not load the native P11 civil BAM'
Assert-Contains $rendererSource 'm_initSceneData.MissileMaxCountResv1' 'Renderer does not honor the existing Resv1 capacity field'
Assert-Contains $rendererSource 'targetPlat.targetState.targetPlatID == targetState.targetPlatID' 'Target mapping no longer checks targetPlatID'
Assert-Contains $rendererSource 'targetPlat.targetState.targetID == targetState.targetID' 'Target mapping no longer checks targetID'
Assert-Contains $senderSource 'testTarget==0x55' 'Stimulus does not permit the reserved P11 civil type'
Assert-Contains $senderSource 'cmd.MissileMaxCountResv1 = p11TargetTypeOk && p11TargetType == 0x55 ? 1 : 0;' 'Stimulus does not provision exactly one explicit P11 civil slot'
Assert-Contains $fixtureSource 'LookAtTargetKey must exactly match a SyntheticTelemetryTargets full key' 'Fixture full-key guard is missing'
Assert-Contains $fixtureSource 'data.weaponState.targetPlatID=fixture.lookAtTargetKey[1].toInt();' 'Fixture does not copy targetPlatID to WeaponState'
Assert-Contains $annotationSource 'case 0x55: return Resv1;' 'Annotation identity does not recognize the P11 civil type'

$assetDir = Join-Path $root 'HwaSim_IR\Bin\Config\TargetLib\p11\civil_van'
$requiredAssets = @(
    'p11_civil_van.bam', 'p11_civil_van_visible.ppm',
    'p11_civil_van_material_id.pgm', 'p11_civil_van_material_id.pgm.xml',
    'p11_civil_van_band_optics.csv', 'manifest.json', 'README.md', 'LICENSE.txt'
)
foreach ($name in $requiredAssets) {
    if (-not (Test-Path -LiteralPath (Join-Path $assetDir $name) -PathType Leaf)) {
        throw "P11 civil asset is missing: $name"
    }
}

$fixturePath = Join-Path $root 'tools\p11_inputs\civil_van_close_lookat.json'
$fixtureBytes = [IO.File]::ReadAllBytes($fixturePath)
$fixture = Get-Content -LiteralPath $fixturePath -Raw | ConvertFrom-Json
if ($fixture.Schema -ne 'ordinary_weather_camera_1') { throw 'Unexpected fixture schema' }
if (@($fixture.SyntheticTelemetryTargets).Count -ne 1) { throw 'Controlled fixture must contain exactly one target' }
if (@($fixture.LookAtTargetKey).Count -ne 3) { throw 'Controlled fixture must contain one full look-at key' }
$target = @($fixture.SyntheticTelemetryTargets)[0]
$key = @($fixture.LookAtTargetKey)
if ([int]$target[0] -ne 0x55 -or [int]$key[0] -ne 0x55 -or
    [int]$target[0] -ne [int]$key[0] -or [int]$target[1] -ne [int]$key[1] -or [int]$target[2] -ne [int]$key[2]) {
    throw 'Controlled fixture target and camera keys are not identical full keys'
}

$previousTime = -1.0
$ranges = @()
foreach ($frame in @($fixture.Keyframes)) {
    if (@($frame).Count -ne 7 -or [double]$frame[0] -le $previousTime) { throw 'Camera keyframes are not strictly ordered' }
    $previousTime = [double]$frame[0]
    $lat1 = [double]$frame[1] * [Math]::PI / 180.0
    $lat2 = [double]$target[3] * [Math]::PI / 180.0
    $dLat = $lat2 - $lat1
    $dLon = ([double]$target[4] - [double]$frame[2]) * [Math]::PI / 180.0
    $a = [Math]::Pow([Math]::Sin($dLat / 2.0), 2) + [Math]::Cos($lat1) * [Math]::Cos($lat2) * [Math]::Pow([Math]::Sin($dLon / 2.0), 2)
    $horizontalM = 6371008.8 * 2.0 * [Math]::Atan2([Math]::Sqrt($a), [Math]::Sqrt(1.0 - $a))
    $rangeM = [Math]::Sqrt($horizontalM * $horizontalM + [Math]::Pow([double]$target[5] - [double]$frame[3], 2))
    # The production P11 MODTRAN grid is audited only through 1 km.  Keep this
    # deterministic fixture inside that domain instead of preserving the
    # pre-P11 3-5 km presentation-only range.
    if ($rangeM -lt 100.0 -or $rangeM -gt 1000.0) { throw "Controlled target range outside audited 0.1-1.0 km domain: $rangeM" }
    $ranges += $rangeM
}

$manifest = Get-Content -LiteralPath (Join-Path $assetDir 'manifest.json') -Raw | ConvertFrom-Json
$vehicleLengthM = [double]$manifest.nominalBoundsM.y[1] - [double]$manifest.nominalBoundsM.y[0]
$minRangeM = ($ranges | Measure-Object -Minimum).Minimum
$sensorPixelAngleRad = 100.0e-6
$estimatedLongAxisPixels = 2.0 * [Math]::Atan($vehicleLengthM / (2.0 * $minRangeM)) / $sensorPixelAngleRad
if ($estimatedLongAxisPixels -lt 100.0 -or $estimatedLongAxisPixels -gt 790.0) {
    throw "Controlled close view does not meet the 100-790 px long-axis design range at 100 urad/pixel: $estimatedLongAxisPixels"
}

$compiler = 'D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw "Missing compiler: $compiler" }
$unitExe = Join-Path $out 'p11_civil_target_protocol_unit.exe'
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic `
    -I (Join-Path $root 'DataDrivenTestQT') `
    (Join-Path $root 'tools\p11_civil_target_protocol_unit.cpp') -o $unitExe
if ($LASTEXITCODE -ne 0) { throw 'P11 civil protocol unit compilation failed' }
$unitJsonText = (& $unitExe | Out-String).Trim()
if ($LASTEXITCODE -ne 0) { throw 'P11 civil protocol unit failed' }
$unit = $unitJsonText | ConvertFrom-Json
if ($unit.result -ne 'PASS') { throw 'P11 civil protocol unit did not pass' }
[IO.File]::WriteAllText((Join-Path $out 'protocol_unit.json'), $unitJsonText + "`n", [Text.UTF8Encoding]::new($false))

$senderInitLogPath = Join-Path $out 'sender_init.err.log'
$senderInitSmoke = $null
if (Test-Path -LiteralPath $senderInitLogPath -PathType Leaf) {
    $senderInitLog = Get-Content -LiteralPath $senderInitLogPath -Raw
    Assert-Contains $senderInitLog '[ProtocolLayout] component=DataDrivenTestQT ControlP2cX1ObjTrackingCmd=24 InitP2cObjectTrackingCmd=385 DisplayC2cObjTrackingData=506 InitAckC2pObjectTrackingCmd=17' 'Built sender protocol layout smoke failed'
    Assert-Contains $senderInitLog '[StimTargetPool] requestedType=0X55 resv1Count=1 resv2Count=0 protocolLayoutUnchanged=1' 'Built sender did not provision only the P11 civil pool'
    Assert-Contains $senderInitLog '[StimInit]' 'Built sender did not send its loopback init datagram'
    $senderInitSmoke = [ordered]@{
        result = 'PASS'
        transport = 'udp_loopback_no_receiver'
        log = $senderInitLogPath
        sha256 = (Get-FileHash -LiteralPath $senderInitLogPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$assetHashes = foreach ($name in $requiredAssets) {
    $path = Join-Path $assetDir $name
    [ordered]@{
        file = $name
        bytes = (Get-Item -LiteralPath $path).Length
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
$summary = [ordered]@{
    result = 'PASS'
    protocolLayout = $unit.protocol
    targetType = '0x55'
    platform = 'Resv1'
    displayName = 'P11-CIVIL-VAN'
    capacitySource = 'InitP2cObjectTrackingCmd.MissileMaxCountResv1'
    defaultRunChanged = $false
    targetKey = @([int]$key[0], [int]$key[1], [int]$key[2])
    fullKeyCameraLock = $true
    frameOrdering = 'strictly_increasing_no_replay'
    rangesM = @($ranges | ForEach-Object { [Math]::Round($_, 3) })
    estimatedNearestLongAxisPixelsAt800 = [Math]::Round($estimatedLongAxisPixels, 2)
    fixture = [ordered]@{
        path = $fixturePath
        sha256 = ([Security.Cryptography.SHA256]::Create().ComputeHash($fixtureBytes) | ForEach-Object { $_.ToString('x2') }) -join ''
    }
    builtSenderInitSmoke = $senderInitSmoke
    assetHashes = $assetHashes
}
$summaryPath = Join-Path $out 'integration_check.json'
[IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
$summary | ConvertTo-Json -Depth 5 -Compress
