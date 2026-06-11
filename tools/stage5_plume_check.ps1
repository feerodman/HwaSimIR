param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$modelHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IREnginePlumeModel.h"
$modelSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IREnginePlumeModel.cpp"
$appHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.h"
$appSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$commonDefine = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\Common\CommonDefine.h"
$runtimeIni = Join-Path $rootPath "HwaSim_IR\Bin\Config\HwaSimIRRuntime.ini"
$profileJson = Join-Path $rootPath "HwaSim_IR\Bin\Config\IRPlume\engine_plume_profiles.json"
$cmakePath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\CMakeLists.txt"
$vcxprojPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj"
$filtersPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj.filters"
$stage6DisplayCheck = Join-Path $rootPath "tools\stage6_sensor_display_check.ps1"

function Read-Text {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return "" }
    return Get-Content -LiteralPath $Path -Raw -Encoding UTF8
}

function Add-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail)
    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Get-FunctionBody {
    param([string]$Text, [string]$Signature)
    $start = $Text.IndexOf($Signature)
    if ($start -lt 0) { return "" }
    $brace = $Text.IndexOf("{", $start)
    if ($brace -lt 0) { return "" }
    $depth = 0
    for ($i = $brace; $i -lt $Text.Length; $i++) {
        $c = $Text[$i]
        if ($c -eq "{") { $depth++ }
        elseif ($c -eq "}") {
            $depth--
            if ($depth -eq 0) {
                return $Text.Substring($brace, $i - $brace + 1)
            }
        }
    }
    return ""
}

$modelHeaderText = Read-Text $modelHeader
$modelSourceText = Read-Text $modelSource
$appHeaderText = Read-Text $appHeader
$appSourceText = Read-Text $appSource
$commonText = Read-Text $commonDefine
$runtimeText = Read-Text $runtimeIni
$profileText = Read-Text $profileJson
$cmakeText = Read-Text $cmakePath
$vcxprojText = Read-Text $vcxprojPath
$filtersText = Read-Text $filtersPath
$plumeUpdateBody = Get-FunctionBody -Text $appSourceText -Signature "UpdateEnginePlumeForTarget"
$plumeCreateBody = Get-FunctionBody -Text $appSourceText -Signature "CreateEnginePlumeForTarget"
$updateIrBody = Get-FunctionBody -Text $appSourceText -Signature "void HwaSimIR::UpdatePlatformIRStatus"

$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "IREnginePlumeModel header/source exist" ((Test-Path -LiteralPath $modelHeader -PathType Leaf) -and (Test-Path -LiteralPath $modelSource -PathType Leaf)) "$modelHeader; $modelSource")) | Out-Null
$checks.Add((Add-Check "IREnginePlumeModel supports core/halo profiles" (($modelHeaderText -match "IREnginePlumeLayerProfile") -and ($modelHeaderText -match "IREnginePlumeProfile[\s\S]*core") -and ($modelHeaderText -match "IREnginePlumeProfile[\s\S]*halo") -and ($modelHeaderText -match "coreGray") -and ($modelHeaderText -match "haloGray") -and ($modelSourceText -match "deriveHaloLayer") -and ($modelSourceText -match "computeLayerGray")) "$modelHeader; $modelSource")) | Out-Null
$checks.Add((Add-Check "engine plume profile exists and contains core/halo" ((Test-Path -LiteralPath $profileJson -PathType Leaf) -and ($profileText -match '"defaults"') -and ($profileText -match '"platforms"') -and ($profileText -match '"F35"') -and ($profileText -match '"AIM120D"') -and ($profileText -match '"AIM9X"') -and ($profileText -match '"core"') -and ($profileText -match '"halo"')) $profileJson)) | Out-Null
$checks.Add((Add-Check "profile exposes two-layer plume fields" (($profileText -match '"temperatureK"') -and ($profileText -match '"opacity"') -and ($profileText -match '"lengthM"') -and ($profileText -match '"radiusRootM"') -and ($profileText -match '"radiusTailM"') -and ($profileText -match '"axialDecay"') -and ($profileText -match '"radialDecay"') -and ($profileText -match '"noiseScale"') -and ($profileText -match '"noiseStrength"') -and ($profileText -match '"bandGain"') -and ($profileText -match '"MWIR"') -and ($profileText -match '"LWIR"')) $profileJson)) | Out-Null
$checks.Add((Add-Check "HwaSimIRRuntime.ini has Stage5Plume core/halo display controls" (($runtimeText -match "\[Stage5Plume\]") -and ($runtimeText -match "EnableEnginePlume=1") -and ($runtimeText -match "EnginePlumeProfilePath=Config/IRPlume/engine_plume_profiles\.json") -and ($runtimeText -match "MaxPlumeNodes=16") -and ($runtimeText -match "UseEngineState=1") -and ($runtimeText -match "UseProceduralNoise=1") -and ($runtimeText -match "ForcePlumeVisible=0") -and ($runtimeText -match "PlumeDisplayGain=1\.0") -and ($runtimeText -match "PlumeCoreDisplayGain=1\.2") -and ($runtimeText -match "PlumeHaloDisplayGain=0\.8") -and ($runtimeText -match "PlumeOpacityScale=1\.0") -and ($runtimeText -match "PlumeCoreOpacityScale=1\.0") -and ($runtimeText -match "PlumeHaloOpacityScale=1\.0")) $runtimeIni)) | Out-Null
$checks.Add((Add-Check "IREnginePlumeModel in CMake" ($cmakeText -match "IR/IREnginePlumeModel\.cpp") $cmakePath)) | Out-Null
$checks.Add((Add-Check "IREnginePlumeModel in VS project" (($vcxprojText -match "IR\\IREnginePlumeModel\.cpp") -and ($vcxprojText -match "IR\\IREnginePlumeModel\.h")) $vcxprojPath)) | Out-Null
$checks.Add((Add-Check "IREnginePlumeModel in VS filters" (($filtersText -match "IR\\IREnginePlumeModel\.cpp") -and ($filtersText -match "IR\\IREnginePlumeModel\.h")) $filtersPath)) | Out-Null
$checks.Add((Add-Check "TargetPlatformData owns core/halo plume nodes" (($commonText -match "enginePlumeCoreNodePath") -and ($commonText -match "enginePlumeHaloNodePath") -and ($appSourceText -match "CreateEnginePlumeForTarget\(newTargetPlat\)") -and ($plumeCreateBody -match "enginePlumeCoreNodePath") -and ($plumeCreateBody -match "enginePlumeHaloNodePath") -and ($plumeCreateBody -match "maxPlumeNodes")) "$commonDefine; $appSource")) | Out-Null
$checks.Add((Add-Check "u_object_kind=4 and u_plume_layer shader path exists" (($appSourceText -match "u_object_kind.*4:EnginePlume") -or ($appSourceText -match "u_object_kind == 4")) $appSource)) | Out-Null
$checks.Add((Add-Check "plume shader uniforms include layer and shape controls" (($appSourceText -match "u_plume_layer") -and ($appSourceText -match "u_plume_temperature_K") -and ($appSourceText -match "u_plume_gray") -and ($appSourceText -match "u_plume_opacity") -and ($appSourceText -match "u_plume_length") -and ($appSourceText -match "u_plume_radius_root") -and ($appSourceText -match "u_plume_radius_tail") -and ($appSourceText -match "u_plume_axial_decay") -and ($appSourceText -match "u_plume_radial_decay") -and ($appSourceText -match "u_plume_noise_scale") -and ($appSourceText -match "u_plume_noise_strength") -and ($appSourceText -match "u_plume_band_gain")) $appSource)) | Out-Null
$checks.Add((Add-Check "plume is updated after Stage4 target state" (($updateIrBody -match "ApplyStage4TargetState") -and ($updateIrBody -match "UpdateEnginePlumeForTarget") -and ($updateIrBody.IndexOf("ApplyStage4TargetState") -lt $updateIrBody.IndexOf("UpdateEnginePlumeForTarget"))) $appSource)) | Out-Null
$checks.Add((Add-Check "plume update is controlled by engineState" (($modelHeaderText -match "engineState") -and ($modelSourceText -match "input\.engineState") -and ($modelSourceText -match "useEngineState") -and ($plumeUpdateBody -match "targetPlat\.targetState\.engineState")) "$modelHeader; $modelSource; $appSource")) | Out-Null
$checks.Add((Add-Check "plume does not read strikeFlag or strikePart" (($modelHeaderText -notmatch "strikeFlag|strikePart") -and ($modelSourceText -notmatch "strikeFlag|strikePart") -and ($plumeUpdateBody -notmatch "strikeFlag|strikePart")) "$modelHeader; $modelSource; $appSource")) | Out-Null
$checks.Add((Add-Check "plume hides with target visibility and far-clip hiding" (($plumeUpdateBody -match "targetRenderable") -and ($plumeUpdateBody -match "targetState\.viewValid") -and ($appSourceText -match "HideEnginePlume\(targetPlat\)") -and ($appSourceText -match "STAGE6_TARGET_BEYOND_FAR_CLIP")) $appSource)) | Out-Null
$checks.Add((Add-Check "plume nodes are not created in per-frame UpdatePlatformIRStatus" (($updateIrBody -notmatch "attach_new_node|CreateStage5EnginePlumeBillboardNode|TexturePool::load_texture") -and ($plumeUpdateBody -notmatch "attach_new_node|TexturePool::load_texture")) $appSource)) | Out-Null
$checks.Add((Add-Check "plume uses low-cost geometry and transparency" (($appSourceText -match "CreateStage5EnginePlumeBillboardNode") -and ($appSourceText -match "set_transparency") -and ($appSourceText -match "set_depth_write\(false\)") -and ($appSourceText -match "set_depth_test\(true\)") -and ($appSourceText -match 'set_bin\("transparent"')) $appSource)) | Out-Null
$checks.Add((Add-Check "plume logs core/halo diagnostics" (($appSourceText -match "\[Stage5 Plume\]") -and ($appSourceText -match "coreEnabled=") -and ($appSourceText -match "haloEnabled=") -and ($appSourceText -match "coreTempK=") -and ($appSourceText -match "haloTempK=") -and ($appSourceText -match "coreGray=") -and ($appSourceText -match "haloGray=") -and ($appSourceText -match "coreOpacity=") -and ($appSourceText -match "haloOpacity=") -and ($appSourceText -match "coreVisible=") -and ($appSourceText -match "haloVisible=") -and ($appSourceText -match "\[Stage5 PlumePerf\]") -and ($appSourceText -match "textureLoadCountThisFrame=0")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage6 final sensor pipeline remains same-output" ((Test-Path -LiteralPath $stage6DisplayCheck -PathType Leaf) -and ($appSourceText -match "\[Stage6 FinalPipeline\]") -and ($appSourceText -match "windowSource=final_sensor") -and ($appSourceText -match "tcpSource=final_sensor") -and ($appSourceText -match "sameOutput=1")) "$appSource; $stage6DisplayCheck")) | Out-Null
$checks.Add((Add-Check "Stage5E keeps forbidden boundaries" (($appSourceText -cnotmatch "path_radiance_band|sky_radiance_band|solar_irradiance_band|pathRadianceBand|skyRadianceBand|solarIrradianceBand|AGC|MTF|H264|UDP video|wholeTargetHotspot|whole-target hotspot") -and ($modelSourceText -cnotmatch "path_radiance|sky_radiance|solar_irradiance|AGC|MTF|H264|UDP video")) "$appSource; $modelSource")) | Out-Null

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage5 plume check failed:" -ForegroundColor Red
    $failed | Format-List
    if ($Strict) { exit 1 }
}
else {
    Write-Host ""
    Write-Host "Stage5 plume check passed." -ForegroundColor Green
}
