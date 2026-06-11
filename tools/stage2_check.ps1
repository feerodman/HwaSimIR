param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path

function Add-Check {
    param(
        [string]$Name,
        [bool]$Ok,
        [string]$Detail
    )

    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Read-Text {
    param([string]$RelativePath)
    $path = Join-Path $rootPath $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        return $null
    }
    return Get-Content -LiteralPath $path -Raw -Encoding UTF8
}

function Test-WorkspacePath {
    param([string]$RelativePath)
    return Test-Path -LiteralPath (Join-Path $rootPath $RelativePath)
}

function Get-MaterialRows {
    param(
        [string]$Asset,
        [string]$RelativePath
    )

    $text = Read-Text $RelativePath
    $rows = New-Object System.Collections.Generic.List[object]
    if ($null -eq $text) {
        return $rows
    }

    $matches = [regex]::Matches($text, '<Composite_Material\s+index="(?<id>\d+)".*?</Composite_Material>', 'Singleline')
    foreach ($match in $matches) {
        $block = $match.Value
        $names = [regex]::Matches($block, '<Material>\s*<Name>(?<name>[^<]+)</Name>', 'Singleline')
        $material = if ($names.Count -gt 1) { $names[$names.Count - 1].Groups['name'].Value } elseif ($names.Count -eq 1) { $names[0].Groups['name'].Value } else { "" }
        $semanticMatch = [regex]::Match($block, '<Composite_Material[^>]*>.*?<Name>(?<semantic>[^<]+)</Name>', 'Singleline')
        $rows.Add([PSCustomObject]@{
            Asset = $Asset
            MaterialId = [int]$match.Groups['id'].Value
            MaterialName = $material
            Semantic = $semanticMatch.Groups['semantic'].Value
        })
    }
    return $rows
}

$checks = New-Object System.Collections.Generic.List[object]

$requiredPaths = @(
    "HwaSim_IR\HwaSim_IR\IR\IRSceneMaterialMapper.h",
    "HwaSim_IR\HwaSim_IR\IR\IRSceneMaterialMapper.cpp",
    "HwaSim_IR\Bin\Config\TargetLib\models\f35\F35C.obj",
    "HwaSim_IR\Bin\Config\TargetLib\models\f35\f35c.jpg",
    "HwaSim_IR\Bin\Config\TargetLib\models\f35\f35c_mat.tif",
    "HwaSim_IR\Bin\Config\TargetLib\models\f35\f35c_mat.tif.xml",
    "HwaSim_IR\Bin\Config\TargetLib\models\aim120\AIM120.obj",
    "HwaSim_IR\Bin\Config\TargetLib\models\aim120\aim120.jpg",
    "HwaSim_IR\Bin\Config\TargetLib\models\aim120\aim120_mat.tif",
    "HwaSim_IR\Bin\Config\TargetLib\models\aim120\aim120_mat.tif.xml",
    "HwaSim_IR\Bin\Config\TargetLib\models\aim9x\aim9x.obj",
    "HwaSim_IR\Bin\Config\TargetLib\models\aim9x\TX_AIM9X_Diffuse.png",
    "HwaSim_IR\Bin\Config\TargetLib\models\aim9x\TX_AIM9X_Diffuse_mat.tif",
    "HwaSim_IR\Bin\Config\TargetLib\models\aim9x\TX_AIM9X_Diffuse_mat.tif.xml",
    "HwaSim_IR\Bin\Config\TargetLib\models\f22\f22.obj",
    "HwaSim_IR\Bin\Config\TargetLib\models\f22\f22_mat.tif",
    "HwaSim_IR\Bin\Config\TargetLib\models\f22\f22_mat.tif.xml"
)

foreach ($relativePath in $requiredPaths) {
    $checks.Add((Add-Check "Stage2 file/resource" (Test-WorkspacePath $relativePath) $relativePath))
}

$hwa = Read-Text "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$common = Read-Text "HwaSim_IR\HwaSim_IR\Common\CommonDefine.h"
$mapper = Read-Text "HwaSim_IR\HwaSim_IR\IR\IRSceneMaterialMapper.cpp"
$f35Mtl = Read-Text "HwaSim_IR\Bin\Config\TargetLib\models\f35\F35C.mtl"
$aim120Mtl = Read-Text "HwaSim_IR\Bin\Config\TargetLib\models\aim120\AIM120.mtl"
$aim9xMtl = Read-Text "HwaSim_IR\Bin\Config\TargetLib\models\aim9x\aim9x.mtl"
$f22Mtl = Read-Text "HwaSim_IR\Bin\Config\TargetLib\models\f22\f22.mtl"
$vcxproj = Read-Text "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj"
$filters = Read-Text "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj.filters"
$cmake = Read-Text "HwaSim_IR\HwaSim_IR\CMakeLists.txt"

$checks.Add((Add-Check "Protocol keeps F35 default" ($hwa -match 'case 0x11:\s*return F35;') "TargetTypeToPlatformType"))
$checks.Add((Add-Check "Protocol keeps MMD mapping" ($hwa -match 'case 0x44:\s*return MMD;') "TargetTypeToPlatformType"))
$checks.Add((Add-Check "F35 nested resource path" ($hwa -match 'models/f35/F35C\.obj' -and $hwa -match 'models/f35/f35c_mat\.tif') "InitPlatformModels"))
$checks.Add((Add-Check "AIM120D nested resource path" ($hwa -match 'models/aim120/AIM120\.obj' -and $hwa -match 'models/aim120/aim120_mat\.tif') "InitPlatformModels"))
$checks.Add((Add-Check "AIM9X nested resource path" ($hwa -match 'models/aim9x/aim9x\.obj' -and $hwa -match 'models/aim9x/TX_AIM9X_Diffuse_mat\.tif') "InitPlatformModels"))
$checks.Add((Add-Check "J20 active binding removed" ($hwa -notmatch 'm_platformResMap\[J20\]') "J20 model deleted by user"))
$checks.Add((Add-Check "PlatformResPath includes material fields" ($common -match 'materialIdTexturePath' -and $common -match 'materialMapPath' -and $common -match 'defaultMaterialName') "CommonDefine.h"))
$checks.Add((Add-Check "Shader receives material ID texture" ($hwa -match 'p3d_Texture1' -and $mapper -match 'TextureStage' -and $hwa -match 'u_material_params\[8\]' -and $hwa -match 'u_debug_material_id') "InitInfraredShader"))
$checks.Add((Add-Check "Runtime binds material mapper" ($hwa -match 'LoadPlatformAssetNode' -and $hwa -match 'm_irSceneMaterialMapper\.bindPlatformNode') "HwaSimIR.cpp"))
$checks.Add((Add-Check "Mapper parses XML surface material" ($mapper -match 'Surface_Substrate' -and $mapper -match 'parseCompositeMaterialXml' -and $mapper -match '<Composite_Material ') "IRSceneMaterialMapper.cpp"))
$checks.Add((Add-Check "MTL paths are relative" (($f35Mtl -notmatch 'D:\\') -and ($aim120Mtl -notmatch 'D:\\') -and ($aim9xMtl -notmatch 'D:\\') -and ($f22Mtl -notmatch 'D:\\')) "MTL relative texture paths"))
$checks.Add((Add-Check "VS project includes mapper cpp" ($vcxproj -match 'IR\\IRSceneMaterialMapper\.cpp') "HwaSim_IR.vcxproj"))
$checks.Add((Add-Check "VS filters include mapper" ($filters -match 'IR\\IRSceneMaterialMapper\.h' -and $filters -match 'IR\\IRSceneMaterialMapper\.cpp') "HwaSim_IR.vcxproj.filters"))
$checks.Add((Add-Check "CMake includes mapper cpp" ($cmake -match 'IR/IRSceneMaterialMapper\.cpp') "CMakeLists.txt"))

$materialRows = New-Object System.Collections.Generic.List[object]
foreach ($row in (Get-MaterialRows "F35" "HwaSim_IR\Bin\Config\TargetLib\models\f35\f35c_mat.tif.xml")) { $materialRows.Add($row) }
foreach ($row in (Get-MaterialRows "AIM120D" "HwaSim_IR\Bin\Config\TargetLib\models\aim120\aim120_mat.tif.xml")) { $materialRows.Add($row) }
foreach ($row in (Get-MaterialRows "AIM9X" "HwaSim_IR\Bin\Config\TargetLib\models\aim9x\TX_AIM9X_Diffuse_mat.tif.xml")) { $materialRows.Add($row) }
foreach ($row in (Get-MaterialRows "F22-unused" "HwaSim_IR\Bin\Config\TargetLib\models\f22\f22_mat.tif.xml")) { $materialRows.Add($row) }

$checks.Add((Add-Check "Material XML rows parsed" ($materialRows.Count -ge 10) "F35/AIM120D/AIM9X/F22 XML"))

Write-Host "Stage 2 material/asset binding check"
Write-Host "Workspace: $rootPath"
Write-Host ""
$checks | Format-Table -AutoSize

Write-Host ""
Write-Host "Material ID rows detected:"
$materialRows | Sort-Object Asset, MaterialId | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Failed checks: $($failed.Count)"
    if ($Strict) {
        exit 1
    }
    exit 2
}

Write-Host ""
Write-Host "All Stage 2 checks passed."
