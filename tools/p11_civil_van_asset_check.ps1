[CmdletBinding()]
param(
    [string]$PandaBin = "F:\Programs\Panda3D-1.10.15-x64\bin",
    [string]$Ppython = "F:\Programs\Panda3D-1.10.15-x64\python\ppython.exe"
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$assetDir = Join-Path $root 'HwaSim_IR\Bin\Config\TargetLib\p11\civil_van'
$outDir = Join-Path $root 'logs\p11\work\civil_model'
$python = (Get-Command python -ErrorAction Stop).Source
$obj2egg = Join-Path $PandaBin 'obj2egg.exe'
$egg2bam = Join-Path $PandaBin 'egg2bam.exe'
$eggTrans = Join-Path $PandaBin 'egg-trans.exe'

foreach ($tool in @($obj2egg, $egg2bam, $eggTrans, $Ppython)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Required Panda3D tool is missing: $tool"
    }
}

New-Item -ItemType Directory -Path $outDir -Force | Out-Null
& $python (Join-Path $PSScriptRoot 'p11_build_civil_van.py')
if ($LASTEXITCODE -ne 0) { throw "civil van generator failed: $LASTEXITCODE" }

$obj = Join-Path $assetDir 'p11_civil_van.obj'
$egg = Join-Path $assetDir 'p11_civil_van.egg'
$bam = Join-Path $assetDir 'p11_civil_van.bam'
& $obj2egg -noabs -nv 42 -uo m -o $egg $obj
if ($LASTEXITCODE -ne 0) { throw "obj2egg failed: $LASTEXITCODE" }
& $python (Join-Path $PSScriptRoot 'p11_finalize_civil_van_egg.py') $egg
if ($LASTEXITCODE -ne 0) { throw "EGG material-tag finalization failed: $LASTEXITCODE" }
& $eggTrans -noabs -c -o (Join-Path $outDir 'roundtrip_checked.egg') $egg
if ($LASTEXITCODE -ne 0) { throw "egg-trans validation failed: $LASTEXITCODE" }
& $egg2bam -ps rel -pd $assetDir -o $bam $egg
if ($LASTEXITCODE -ne 0) { throw "egg2bam failed: $LASTEXITCODE" }

& $Ppython (Join-Path $PSScriptRoot 'p11_render_civil_van_preview.py')
if ($LASTEXITCODE -ne 0) { throw "offscreen preview failed: $LASTEXITCODE" }

$expected = @(
    'p11_civil_van.obj',
    'p11_civil_van.mtl',
    'p11_civil_van.egg',
    'p11_civil_van.bam',
    'p11_civil_van_visible.ppm',
    'p11_civil_van_material_id.pgm',
    'p11_civil_van_material_id.pgm.xml',
    'p11_civil_van_band_optics.csv',
    'manifest.json',
    'README.md',
    'LICENSE.txt'
)

$missing = @($expected | Where-Object { -not (Test-Path -LiteralPath (Join-Path $assetDir $_) -PathType Leaf) })
if ($missing.Count -ne 0) { throw "Missing asset files: $($missing -join ', ')" }

$materials = Import-Csv -LiteralPath (Join-Path $assetDir 'p11_civil_van_band_optics.csv')
if ($materials.Count -ne 6) { throw "Expected 6 material partitions, got $($materials.Count)" }
[xml]$materialXml = Get-Content -LiteralPath (Join-Path $assetDir 'p11_civil_van_material_id.pgm.xml') -Raw
if (@($materialXml.Composite_Material_Table.Composite_Material).Count -ne 6) {
    throw 'Material XML must contain exactly 6 Composite_Material entries'
}
foreach ($row in $materials) {
    $swirSum = [double]$row.SWIRReflectance + [double]$row.SWIREmissivity + [double]$row.SWIRTransmissivity
    $mwirSum = [double]$row.MWIRReflectance + [double]$row.MWIREmissivity + [double]$row.MWIRTransmissivity
    if ([Math]::Abs($swirSum - 1.0) -gt 1e-9 -or [Math]::Abs($mwirSum - 1.0) -gt 1e-9) {
        throw "Band energy balance failed for materialId=$($row.MaterialId)"
    }
    $xmlEntry = @($materialXml.Composite_Material_Table.Composite_Material | Where-Object { [int]$_.index -eq [int]$row.MaterialId })
    if ($xmlEntry.Count -ne 1) { throw "Expected exactly one XML material for ID=$($row.MaterialId)" }
    foreach ($field in @('SWIRReflectance','SWIREmissivity','SWIRTransmissivity','MWIRReflectance','MWIREmissivity','MWIRTransmissivity','NominalTemperatureK')) {
        if ($null -eq $xmlEntry[0].$field -or [string]::IsNullOrWhiteSpace([string]$xmlEntry[0].$field)) {
            throw "Explicit XML field $field missing for materialId=$($row.MaterialId)"
        }
        if ([Math]::Abs([double]$xmlEntry[0].$field - [double]$row.$field) -gt 1e-9) {
            throw "XML/CSV mismatch field=$field materialId=$($row.MaterialId)"
        }
    }
    if ([string]::IsNullOrWhiteSpace([string]$row.EngineOnTemperatureK)) {
        if ($null -ne $xmlEntry[0].EngineOnTemperatureK) { throw "Unexpected EngineOnTemperatureK for materialId=$($row.MaterialId)" }
    }
    elseif ($null -eq $xmlEntry[0].EngineOnTemperatureK -or
        [Math]::Abs([double]$xmlEntry[0].EngineOnTemperatureK - [double]$row.EngineOnTemperatureK) -gt 1e-9) {
        throw "XML/CSV EngineOnTemperatureK mismatch for materialId=$($row.MaterialId)"
    }
}
$engineBay = @($materials | Where-Object { [int]$_.MaterialId -eq 15 })[0]
$tailpipe = @($materials | Where-Object { [int]$_.MaterialId -eq 16 })[0]
if ([double]$engineBay.NominalTemperatureK -ne 303.0 -or [double]$engineBay.EngineOnTemperatureK -ne 345.0 -or
    [double]$tailpipe.NominalTemperatureK -ne 303.0 -or [double]$tailpipe.EngineOnTemperatureK -ne 475.0) {
    throw 'Engine bay/tailpipe off/on temperature locality policy mismatch'
}

$eggText = Get-Content -LiteralPath $egg -Raw
foreach ($materialId in 11..16) {
    if ($eggText -notmatch "<Tag>\s+p11_material_id\s+\{\s+$materialId\s+\}") {
        throw "EGG material tag is missing for ID=$materialId"
    }
}
$pgmTokens = @(
    Get-Content -LiteralPath (Join-Path $assetDir 'p11_civil_van_material_id.pgm') |
        Where-Object { $_ -notmatch '^\s*#' } |
        ForEach-Object { $_ -split '\s+' } |
        Where-Object { $_ -ne '' }
)
$expectedPgmTokens = @('P2', '6', '1', '255', '11', '12', '13', '14', '15', '16')
if (($pgmTokens -join ',') -ne ($expectedPgmTokens -join ',')) {
    throw "Material-ID atlas payload mismatch: $($pgmTokens -join ',')"
}

$hashes = foreach ($name in $expected) {
    $path = Join-Path $assetDir $name
    $item = Get-Item -LiteralPath $path
    $hash = Get-FileHash -LiteralPath $path -Algorithm SHA256
    [pscustomobject]@{ file = $name; bytes = $item.Length; sha256 = $hash.Hash.ToLowerInvariant() }
}
$hashPath = Join-Path $outDir 'asset_sha256.json'
$hashes | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $hashPath -Encoding UTF8

$summary = [ordered]@{
    result = 'PASS'
    canonicalSource = $obj
    egg = $egg
    bam = $bam
    preview = (Join-Path $outDir 'p11_civil_van_close_preview.png')
    rearPreview = (Join-Path $outDir 'p11_civil_van_rear_preview.png')
    materialPartitions = $materials.Count
    swirMwirEnergyBalance = 'PASS'
    explicitXmlBandOptics = 'PASS'
    engineOffOnTemperaturePolicy = 'PASS'
    gmcDependency = $false
    hashManifest = $hashPath
}
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outDir 'asset_check.json') -Encoding UTF8
$summary | ConvertTo-Json -Compress
