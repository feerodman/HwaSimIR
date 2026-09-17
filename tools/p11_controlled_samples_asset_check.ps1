[CmdletBinding()]
param(
    [string]$PandaBin = "F:\Programs\Panda3D-1.10.15-x64\bin",
    [string]$Ppython = "F:\Programs\Panda3D-1.10.15-x64\python\ppython.exe"
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$assetDir = Join-Path $root 'HwaSim_IR\Bin\Config\TargetLib\p11\controlled_samples'
$outDir = Join-Path $root 'logs\p11\work\controlled_samples'
$python = (Get-Command python -ErrorAction Stop).Source
$obj2egg = Join-Path $PandaBin 'obj2egg.exe'
$egg2bam = Join-Path $PandaBin 'egg2bam.exe'
$eggTrans = Join-Path $PandaBin 'egg-trans.exe'
foreach ($tool in @($obj2egg, $egg2bam, $eggTrans, $Ppython)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Required Panda3D tool is missing: $tool" }
}

New-Item -ItemType Directory -Path $outDir -Force | Out-Null
& $python (Join-Path $PSScriptRoot 'p11_build_controlled_samples.py')
if ($LASTEXITCODE -ne 0) { throw "controlled sample generator failed: $LASTEXITCODE" }

$obj = Join-Path $assetDir 'p11_controlled_samples.obj'
$egg = Join-Path $assetDir 'p11_controlled_samples.egg'
$bam = Join-Path $assetDir 'p11_controlled_samples.bam'
& $obj2egg -noabs -nv 42 -uo m -o $egg $obj
if ($LASTEXITCODE -ne 0) { throw "obj2egg failed: $LASTEXITCODE" }
& $python (Join-Path $PSScriptRoot 'p11_finalize_controlled_samples_egg.py') $egg
if ($LASTEXITCODE -ne 0) { throw "EGG tag finalization failed: $LASTEXITCODE" }
& $eggTrans -noabs -c -o (Join-Path $outDir 'roundtrip_checked.egg') $egg
if ($LASTEXITCODE -ne 0) { throw "egg-trans validation failed: $LASTEXITCODE" }
& $egg2bam -ps rel -pd $assetDir -o $bam $egg
if ($LASTEXITCODE -ne 0) { throw "egg2bam failed: $LASTEXITCODE" }
& $Ppython (Join-Path $PSScriptRoot 'p11_render_controlled_samples_preview.py')
if ($LASTEXITCODE -ne 0) { throw "offscreen sample preview failed: $LASTEXITCODE" }

$expectedFiles = @(
    'p11_controlled_samples.obj', 'p11_controlled_samples.mtl',
    'p11_controlled_samples.egg', 'p11_controlled_samples.bam',
    'p11_controlled_samples_visible.ppm', 'p11_controlled_samples_material_id.pgm',
    'p11_controlled_samples_material_id.pgm.xml', 'p11_controlled_samples_band_optics.csv',
    'manifest.json', 'README.md', 'LICENSE.txt'
)
foreach ($name in $expectedFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $assetDir $name) -PathType Leaf)) {
        throw "Controlled sample asset is missing: $name"
    }
}

$materials = @(Import-Csv -LiteralPath (Join-Path $assetDir 'p11_controlled_samples_band_optics.csv'))
if ($materials.Count -ne 8) { throw "Expected exactly 8 sample panels, got $($materials.Count)" }
$ids = @($materials | ForEach-Object { [int]$_.MaterialId })
if (($ids -join ',') -ne ((21..28) -join ',')) { throw "Material IDs must be exactly ordered 21..28: $($ids -join ',')" }
[xml]$xml = Get-Content -LiteralPath (Join-Path $assetDir 'p11_controlled_samples_material_id.pgm.xml') -Raw
$xmlEntries = @($xml.Composite_Material_Table.Composite_Material)
if ($xmlEntries.Count -ne 8) { throw 'Material XML must contain exactly 8 entries' }

$temperatureContract = @{ 21=250.0; 22=300.0; 23=350.0; 24=475.0; 25=300.0; 26=300.0; 27=300.0; 28=275.0 }
foreach ($row in $materials) {
    $id = [int]$row.MaterialId
    $swir = [double]$row.SWIRReflectance + [double]$row.SWIREmissivity + [double]$row.SWIRTransmissivity
    $mwir = [double]$row.MWIRReflectance + [double]$row.MWIREmissivity + [double]$row.MWIRTransmissivity
    if ([Math]::Abs($swir - 1.0) -gt 1e-9 -or [Math]::Abs($mwir - 1.0) -gt 1e-9) {
        throw "Energy balance failed for sample ID=$id"
    }
    if ([double]$row.NominalTemperatureK -ne [double]$temperatureContract[$id]) {
        throw "Temperature contract failed for sample ID=$id"
    }
    if ($row.Source -ne 'P11_controlled_ideal_or_engineering_assumption') {
        throw "Sample ID=$id lacks explicit assumption provenance"
    }
    $entry = @($xmlEntries | Where-Object { [int]$_.index -eq $id })
    if ($entry.Count -ne 1) { throw "Expected one XML entry for sample ID=$id" }
    foreach ($field in @('SWIRReflectance','SWIREmissivity','SWIRTransmissivity','MWIRReflectance','MWIREmissivity','MWIRTransmissivity','NominalTemperatureK')) {
        if ([Math]::Abs([double]$entry[0].$field - [double]$row.$field) -gt 1e-9) {
            throw "CSV/XML mismatch ID=$id field=$field"
        }
    }
}
foreach ($id in 21..24) {
    $row = @($materials | Where-Object { [int]$_.MaterialId -eq $id })[0]
    if ([double]$row.SWIRReflectance -ne 0.0 -or [double]$row.SWIREmissivity -ne 1.0 -or
        [double]$row.MWIRReflectance -ne 0.0 -or [double]$row.MWIREmissivity -ne 1.0) {
        throw "Blackbody ID=$id is not an exact ideal blackbody"
    }
}
$glass = @($materials | Where-Object { [int]$_.MaterialId -eq 27 })[0]
if ([double]$glass.SWIRTransmissivity -ne 0.85 -or [double]$glass.MWIREmissivity -ne 0.90) {
    throw 'Glass sample band contract mismatch'
}

$eggText = Get-Content -LiteralPath $egg -Raw
foreach ($id in 21..28) {
    if ($eggText -notmatch "<Tag>\s+p11_material_id\s+\{\s+$id\s+\}") {
        throw "EGG material tag missing for ID=$id"
    }
}
$pgmTokens = @(
    Get-Content -LiteralPath (Join-Path $assetDir 'p11_controlled_samples_material_id.pgm') |
        Where-Object { $_ -notmatch '^\s*#' } |
        ForEach-Object { $_ -split '\s+' } |
        Where-Object { $_ -ne '' }
)
$expectedPgm = @('P2','8','1','255','21','22','23','24','25','26','27','28')
if (($pgmTokens -join ',') -ne ($expectedPgm -join ',')) {
    throw "Material-ID atlas mismatch: $($pgmTokens -join ',')"
}

$manifest = Get-Content -LiteralPath (Join-Path $assetDir 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.protocolTargetType -ne '0x66' -or $manifest.platform -ne 'Resv2' -or
    [int]$manifest.geometry.materials -ne 8 -or @($manifest.panels).Count -ne 8) {
    throw 'Controlled sample manifest identity/geometry mismatch'
}
$hashes = foreach ($name in $expectedFiles) {
    $path = Join-Path $assetDir $name
    [ordered]@{
        file = $name
        bytes = (Get-Item -LiteralPath $path).Length
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
$hashPath = Join-Path $outDir 'asset_sha256.json'
$hashes | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $hashPath -Encoding UTF8
$summary = [ordered]@{
    result = 'PASS'
    targetType = '0x66'
    platform = 'Resv2'
    canonicalSource = $obj
    egg = $egg
    bam = $bam
    frontPreview = (Join-Path $outDir 'p11_controlled_samples_front_preview.png')
    partitionPreview = (Join-Path $outDir 'p11_controlled_samples_partition_preview.png')
    materialIds = @(21..28)
    materialPartitions = 8
    swirMwirEnergyBalance = 'PASS'
    explicitTemperatures = 'PASS'
    visibleRgbUsedForIr = $false
    gmcDependency = $false
    hashManifest = $hashPath
}
$summaryPath = Join-Path $outDir 'asset_check.json'
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
$summary | ConvertTo-Json -Compress
