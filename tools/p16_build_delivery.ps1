param(
    [string]$DeliveryRoot = ""
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($DeliveryRoot)) {
    $DeliveryRoot = Join-Path $repo 'deliverables\HwaSimIR_P16'
}

function Copy-P16File([string]$Source, [string]$RelativeDestination) {
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) {
        throw "Missing delivery source: $Source"
    }
    $destination = Join-Path $DeliveryRoot $RelativeDestination
    $parent = Split-Path -Parent $destination
    New-Item -ItemType Directory -Force -Path $parent | Out-Null
    Copy-Item -LiteralPath $Source -Destination $destination -Force
}

New-Item -ItemType Directory -Force -Path $DeliveryRoot | Out-Null

Copy-P16File (Join-Path $repo 'docs\HwaSimIR_P16_Closeout.md') 'docs\HwaSimIR_P16_Closeout.md'
Copy-P16File (Join-Path $repo 'docs\HwaSimIR_P16_Issue_Ledger.csv') 'docs\HwaSimIR_P16_Issue_Ledger.csv'
Copy-P16File (Join-Path $repo 'docs\HwaSimIR_P16_Delivery_README.md') 'README.md'

Copy-P16File (Join-Path $repo 'HwaSim_IR\Bin\HwaSim_IR.exe') 'deployment\windows\HwaSim_IR.exe'
Copy-P16File (Join-Path $repo 'logs\p16\build\HwaSim_IR_effects7') 'deployment\board\HwaSim_IR'
Copy-P16File (Join-Path $repo 'logs\p16\build\effects7_deployment\deployment_receipt.json') 'deployment\board\deployment_receipt.json'
Copy-P16File (Join-Path $repo 'logs\p16\build\effects7_deployment\deployment_version.env') 'deployment\board\deployment_version.env'

$overlayFiles = @(
    'HwaSim_IR\Bin\Config\HwaSimIRRuntime.ini',
    'HwaSim_IR\Bin\Config\GameVFX\sprite.vert',
    'HwaSim_IR\Bin\Config\GameVFX\sprite.frag',
    'HwaSim_IR\Bin\Config\Weather\precipitation.vert',
    'HwaSim_IR\Bin\Config\Weather\precipitation.frag',
    'HwaSim_IR\Bin\Config\Weather\weather_textures.json',
    'HwaSim_IR\Bin\Config\Weather\Textures\rain.rgba',
    'HwaSim_IR\Bin\Config\Weather\Textures\snow.rgba',
    'HwaSim_IR\Bin\Config\Weather\Textures\cloud_scattered.png',
    'HwaSim_IR\Bin\Config\Weather\Textures\cloud_overcast.png',
    'HwaSim_IR\Bin\Config\Weather\Textures\smoke.png'
)
foreach ($relative in $overlayFiles) {
    $overlayRelative = $relative -replace '^HwaSim_IR\\Bin\\Config\\', 'deployment\config_overlay\Config\'
    Copy-P16File (Join-Path $repo $relative) $overlayRelative
}

$productionRoot = Join-Path $repo 'logs\p16\production_effects'
Copy-P16File (Join-Path $productionRoot 'media_index.json') 'evidence\production\media_index.json'
Copy-P16File (Join-Path $productionRoot 'model_flicker\comparison.json') 'evidence\model_flicker\comparison.json'
foreach ($image in Get-ChildItem -LiteralPath (Join-Path $productionRoot 'model_flicker') -Filter '*.png' -File) {
    Copy-P16File $image.FullName (Join-Path 'evidence\model_flicker' $image.Name)
}

foreach ($caseResultPath in Get-ChildItem -Path (Join-Path $productionRoot 'runs\p16_final_*_effects7\case_result.json') -File) {
    $caseResult = Get-Content -Raw -LiteralPath $caseResultPath.FullName | ConvertFrom-Json
    $caseId = [string]$caseResult.name
    $caseDir = $caseResultPath.Directory.FullName
    Copy-P16File ([string]$caseResult.products.mp4) ("media\production\{0}.mp4" -f $caseId)
    Copy-P16File $caseResultPath.FullName ("media\production\{0}.case_result.json" -f $caseId)
    Copy-P16File (Join-Path $caseDir 'media_probe.json') ("media\production\{0}.media_probe.json" -f $caseId)
    Copy-P16File (Join-Path $caseDir 'contact_6x.png') ("media\production\{0}.contact.png" -f $caseId)
    Copy-P16File (Join-Path $caseDir 'frame_0900.png') ("media\production\{0}.frame_0900.png" -f $caseId)
}

$rawRoots = @(
    'plume_mwir_effects7',
    'mwir_rain_effects7',
    'mwir_snow_effects7',
    'swir_snow_effects7',
    'mwir_cloud_original_effects7'
)
foreach ($rawRootName in $rawRoots) {
    $rawRoot = Join-Path $productionRoot ("raw_qc\{0}" -f $rawRootName)
    foreach ($file in Get-ChildItem -LiteralPath $rawRoot -File) {
        Copy-P16File $file.FullName ("evidence\raw_qc\{0}\{1}" -f $rawRootName, $file.Name)
    }
}

foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo 'logs\p16\annotation') -File) {
    Copy-P16File $file.FullName (Join-Path 'media\annotation' $file.Name)
}
foreach ($file in Get-ChildItem -LiteralPath (Join-Path $repo 'logs\p16\ui_smoke') -File | Where-Object { $_.Name -like 'p16_ui_smoke_final*' }) {
    Copy-P16File $file.FullName (Join-Path 'evidence\ui_smoke' $file.Name)
}

$independentRoot = Join-Path $repo 'logs\p16\independent_sample'
Copy-P16File (Join-Path $independentRoot 'media_index.json') 'evidence\independent\media_index.json'
Copy-P16File (Join-Path $independentRoot 'analysis\pixel_evidence.json') 'evidence\independent\pixel_evidence.json'
Copy-P16File (Join-Path $independentRoot 'analysis\pixel_evidence.csv') 'evidence\independent\pixel_evidence.csv'
foreach ($mp4 in Get-ChildItem -LiteralPath $independentRoot -Recurse -Filter '*.mp4' -File) {
    Copy-P16File $mp4.FullName (Join-Path 'media\independent' $mp4.Name)
}

Copy-P16File (Join-Path $repo 'logs\p16\flicker_audit\p16_flicker_audit.json') 'evidence\flicker_readonly\p16_flicker_audit.json'

$finalStatus = [ordered]@{
    schema = 'HwaSimIR.P16.FinalStatus.1'
    createdUtc = [DateTime]::UtcNow.ToString('o')
    actualHead = (git -C $repo rev-parse HEAD).Trim()
    calibration = 'NOT_VERIFIED_CALIBRATION'
    statuses = [ordered]@{
        build = 'PASS'
        boardDeployment = 'PASS'
        annotationFormat = 'PASS'
        annotationCoordinates = 'PASS'
        annotationStableIndex = 'PASS'
        ordinaryUiRegression = 'PASS'
        productionMediaDecode = 'PASS_8_OF_8'
        productionPlumeControlled = 'PASS'
        productionWeatherControlled = 'PASS'
        modelSamplingControlled = 'MITIGATED'
        existingMediaFlickerReproducer = 'NO_REPRODUCER'
        ordinaryPlumeFeedback = 'REOPENED_NEEDS_USER_ENTRY'
        ordinaryWeatherFeedback = 'REOPENED_PARTIAL_NEEDS_USER_ENTRY'
        ordinaryModelFlickerFeedback = 'OPEN_NEEDS_ORDINARY_REPRODUCER'
    }
    identity = [ordered]@{
        windowsExeSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $repo 'HwaSim_IR\Bin\HwaSim_IR.exe')).Hash.ToLowerInvariant()
        boardElfSha256 = '3d8d97ded05014a11e78983770bd4dacfb0136ded7156e38cefcac4672c40bde'
        boardBuildId = 'bea3b43a2c51c892735edf44d35359bf40a6d814'
        configManifestSha256 = 'c8c6573fd57fb6261fc8a011c06167e5aa75dfc56636e50884736cb5914e66b7'
        original1Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $repo 'DataDrivenTestQT\1.txt')).Hash.ToLowerInvariant()
    }
    evidence = [ordered]@{
        closeout = 'docs/HwaSimIR_P16_Closeout.md'
        issueLedger = 'docs/HwaSimIR_P16_Issue_Ledger.csv'
        productionMediaIndex = 'evidence/production/media_index.json'
        independentMediaIndex = 'evidence/independent/media_index.json'
    }
}
$statusPath = Join-Path $DeliveryRoot 'final_status.json'
$finalStatus | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $statusPath -Encoding UTF8

$excludedNames = @('manifest.json', 'SHA256SUMS.txt')
$entries = @()
foreach ($file in Get-ChildItem -LiteralPath $DeliveryRoot -Recurse -File | Where-Object { $excludedNames -notcontains $_.Name } | Sort-Object FullName) {
    $relative = $file.FullName.Substring($DeliveryRoot.Length).TrimStart('\').Replace('\', '/')
    $entries += [ordered]@{
        path = $relative
        bytes = $file.Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
    }
}
$manifest = [ordered]@{
    schema = 'HwaSimIR.P16.DeliveryManifest.1'
    createdUtc = [DateTime]::UtcNow.ToString('o')
    root = $DeliveryRoot
    fileCount = $entries.Count
    files = $entries
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $DeliveryRoot 'manifest.json') -Encoding UTF8
$sumLines = $entries | ForEach-Object { "{0}  {1}" -f $_.sha256, $_.path }
$sumLines | Set-Content -LiteralPath (Join-Path $DeliveryRoot 'SHA256SUMS.txt') -Encoding ASCII

Write-Host ("[P16Delivery] result=PASS root={0} files={1}" -f $DeliveryRoot, $entries.Count)
