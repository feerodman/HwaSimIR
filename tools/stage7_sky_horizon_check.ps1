param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path

$appHeader = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\HwaSimIR.h"
$appSource = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\HwaSimIR.cpp"
$stage3AtmosphereSource = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\IRSimulation.cpp"
$stage4Check = Join-Path $rootPath "tools\stage4_hotspot_check.ps1"
$stage7Smoke = Join-Path $rootPath "tools\stage7_sky_horizon_smoke.ps1"

function Read-Text {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }
    $encoding = [System.Text.Encoding]::Default
    return $encoding.GetString([System.IO.File]::ReadAllBytes($Path))
}

function Remove-DisabledIf0 {
    param([string]$Text)
    return [System.Text.RegularExpressions.Regex]::Replace($Text, "(?s)#if\s+0.*?#endif[^\r\n]*", "")
}

function Remove-CppComments {
    param([string]$Text)
    $withoutBlocks = [System.Text.RegularExpressions.Regex]::Replace($Text, "(?s)/\*.*?\*/", "")
    return [System.Text.RegularExpressions.Regex]::Replace($withoutBlocks, "(?m)//.*$", "")
}

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

$appHeaderText = Read-Text $appHeader
$appSourceText = Read-Text $appSource
$appActiveText = Remove-CppComments (Remove-DisabledIf0 $appSourceText)
$stage3AtmosphereText = Read-Text $stage3AtmosphereSource
$stage4CheckText = Read-Text $stage4Check
$stage7SmokeText = Read-Text $stage7Smoke

$checks = New-Object System.Collections.Generic.List[object]

$sourceConsistencyOk = ($appSourceText -match "SetupStage6FinalPipeline") -and
    ($appSourceText -match "\[Stage6 FinalPipeline\]") -and
    ($appSourceText -match "\[Stage6 ViewportDiag\]") -and
    ($appHeaderText -match "m_stage6FinalRegion") -and
    ($appHeaderText -match "m_stage6RawSceneBuffer") -and
    ($appSourceText -match "u_stage6_final_uv_scale") -and
    ($appSourceText -match "CreateStage7SkyDomeNode") -and
    ($appSourceText -match "CreateStage7LowerHemisphereShellNode") -and
    ($appSourceText -match "\[Stage7 3DSkyGround\]")
if (-not $sourceConsistencyOk) {
    Write-Host "STAGE6_STAGE7_PATCH_NOT_PRESENT_IN_CURRENT_SOURCE" -ForegroundColor Red
}
$checks.Add((Add-Check "current source contains Stage6/Stage7 consistency patch" $sourceConsistencyOk "$appHeader; $appSource")) | Out-Null

$buildStampOk = $appSourceText -match "\[BuildStamp\] stage6b4_stage7a2_source_active=1"
$checks.Add((Add-Check "startup BuildStamp identifies active Stage6B.4/Stage7A.2 source" $buildStampOk $appSource)) | Out-Null

$checks.Add((Add-Check "EnableStage7SkyHorizon exists and defaults on" (($appHeaderText -match "m_enableStage7SkyHorizon = true") -and ($appSourceText -match 'getBool\("Stage7Background",\s*"EnableSkyHorizon",\s*"EnableStage7SkyHorizon",\s*true')) "$appHeader; $appSource")) | Out-Null
$checks.Add((Add-Check "[Stage7 3DSkyGround] log exists" (($appSourceText -match "\[Stage7 3DSkyGround\]") -and ($appSourceText -match "skyGray=") -and ($appSourceText -match "groundGray=") -and ($appSourceText -match "skyDome=") -and ($appSourceText -match "lowerShell=")) $appSource)) | Out-Null

$skyGround3dOk = ($appSourceText -match "CreateStage7SkyDomeNode") -and
    ($appSourceText -match "CreateStage7LowerHemisphereShellNode") -and
    ($appSourceText -match "Stage7_SkyDome") -and
    ($appSourceText -match "Stage7_LowerHemisphereShell") -and
    ($appSourceText -match "m_renderRoot\.attach_new_node\(CreateStage7SkyDomeNode\(\)\)") -and
    ($appSourceText -match "m_renderRoot\.attach_new_node\(CreateStage7LowerHemisphereShellNode\(\)\)") -and
    ($appSourceText -match "m_skyNode\.set_pos\(m_renderRoot,\s*cameraPos\)") -and
    ($appSourceText -match "m_skyNode\.set_hpr\(m_renderRoot,\s*0\.0f,\s*0\.0f,\s*0\.0f\)") -and
    ($appSourceText -match "m_stage7LowerShellNode\.set_pos\(m_renderRoot,\s*cameraPos\)") -and
    ($appSourceText -match "m_stage7LowerShellNode\.set_hpr\(m_renderRoot,\s*0\.0f,\s*0\.0f,\s*0\.0f\)") -and
    ($appSourceText -match 'set_bin\("background",\s*0\)') -and
    ($appSourceText -match 'set_bin\("background",\s*1\)') -and
    ($appSourceText -match "set_depth_write\(false\)") -and
    ($appSourceText -match "set_depth_test\(false\)") -and
    ($appSourceText -match "u_stage7_sky_horizon_en") -and
    ($appSourceText -match "u_stage7_background_kind")
$checks.Add((Add-Check "Stage7B uses 3D sky dome and lower hemisphere shell" $skyGround3dOk $appSource)) | Out-Null

$real3dBackgroundOk = ($appSourceText -match "\[Stage7 Real3DBackground\]") -and
    ($appSourceText -match "\[Stage7 GroundReference\]") -and
    ($appSourceText -match "backgroundMode=real_3d") -and
    ($appSourceText -match "flatGroundPlane=0") -and
    ($appSourceText -match "lowerShell=1") -and
    ($appSourceText -match "mode=reference_zero") -and
    ($appHeaderText -match "m_stage7LowerShellNode") -and
    ($appHeaderText -match "m_stage7GroundReferenceZ")
$checks.Add((Add-Check "Stage7B logs real 3D background and reference-zero ground" $real3dBackgroundOk $appSource)) | Out-Null

$noFlatGroundPrimaryOk = ($appSourceText -notmatch "CreateStage7GroundPlaneNode") -and
    ($appSourceText -notmatch "Stage7_GroundPlane") -and
    ($appSourceText -notmatch "m_stage7GroundPlaneNode") -and
    ($appSourceText -notmatch "m_renderRoot\.attach_new_node\(CreateStage7GroundPlaneNode\(\)\)")
$checks.Add((Add-Check "default Stage7B background is not a flat ground plane" $noFlatGroundPrimaryOk $appSource)) | Out-Null

$stage7DebugOk = ($appSourceText -match 'getString\("Stage7Background",\s*"DebugMode",\s*"Stage7DebugMode",\s*"Off"') -and
    ($appSourceText -match "SkyOnly") -and
    ($appSourceText -match "GroundOnly") -and
    ($appSourceText -match "SkyGroundColor") -and
    ($appSourceText -match "\[Stage7 Debug\]") -and
    ($appSourceText -match "\[Stage7 SkyDomeDiag\]") -and
    ($appSourceText -match "\[Stage7 GroundDiag\]") -and
    ($appSourceText -match 'getDouble\("Stage7Background",\s*"GroundZOffset",\s*"Stage7GroundZOffset",\s*0\.0') -and
    ($appSourceText -match "set_two_sided\(true\)") -and
    ($appSourceText -match "m_skyNode\.set_pos\(m_renderRoot,\s*cameraPos\)")
$checks.Add((Add-Check "Stage7A.3 debug modes and 3D geometry diagnostics exist" $stage7DebugOk $appSource)) | Out-Null

$dynamicGeometryOk = ($appActiveText -notmatch "m_skyNode\.set_scale\(80000\.0f\)") -and
    ($appSourceText -match "farClipM\s*\*\s*0\.85") -and
    ($appSourceText -match "ClampStage5Double\(farClipM\s*\*\s*0\.85,\s*1000\.0,\s*200000\.0\)") -and
    ($appSourceText -match "m_stage7SkyDomeRadius\s*>=\s*farClipM") -and
    ($appSourceText -match "m_stage7SkyDomeRadius\s*<\s*farClipM") -and
    ($appSourceText -match "radiusSource=farClip_scaled") -and
    ($appSourceText -match "radiusLessThanFarClip=") -and
    ($appSourceText -match "m_stage7LowerShellRadius\s*=\s*m_stage7SkyDomeRadius")
$checks.Add((Add-Check "Stage7B sky/lower-shell radius scales from farClip" $dynamicGeometryOk $appSource)) | Out-Null

$stage7VisibilityGuardOk = ($appSourceText -match "ClampStage5Double\(skyGrayRaw,\s*0\.12,\s*0\.92\)") -and
    ($appSourceText -match "ClampStage5Double\(groundGrayRaw,\s*0\.18,\s*0\.88\)") -and
    ($appSourceText -match "STAGE7_SKY_DOME_NOT_VISIBLE_AFTER_RADIUS_FIX") -and
    ($appSourceText -match "STAGE7_TARGET_NEAR_FAR_CLIP") -and
    ($appSourceText -match "STAGE6_TARGET_BEYOND_FAR_CLIP")
$checks.Add((Add-Check "Stage7 visibility floors and near/far warnings exist" $stage7VisibilityGuardOk $appSource)) | Out-Null

$no2dOnlyBackgroundOk = ($appSourceText -notmatch "IR_Stage7_SkyHorizon_Fullscreen") -and
    ($appSourceText -notmatch "Stage6RawBackgroundRoot") -and
    ($appSourceText -notmatch "m_stage6RawBackgroundRegion") -and
    ($appSourceText -notmatch "skyMaker\.set_frame_fullscreen_quad\(\)") -and
    ($appSourceText -match "IR_Sky_Background") -and
    ($appSourceText -match "if \(m_enableStage7SkyHorizon && m_stage7UseReal3DBackground\)") -and
    ($appSourceText -match "else\s*\{[\s\S]*set_frame\(-1\.0f,\s*1\.0f,\s*-1\.0f,\s*1\.0f\)")
$checks.Add((Add-Check "2D fullscreen card is not the Stage7 enabled background" $no2dOnlyBackgroundOk $appSource)) | Out-Null

$usesEnvOk = ($appSourceText -match "envTerrain") -and
    ($appSourceText -match "envSky") -and
    ($appSourceText -match "envRadScaleTerrain") -and
    ($appSourceText -match "envRadScaleSky") -and
    ($appSourceText -match "source=env\+band_default")
$checks.Add((Add-Check "Stage7A uses env terrain, sky, and radiance scale fields" $usesEnvOk $appSource)) | Out-Null

$stage6FrameDiagOk = ($appSourceText -match "\[Stage6 FrameDiag\]") -and
    ($appSourceText -match "targetVisibleCount=") -and
    ($appSourceText -match "targetMappedCount=") -and
    ($appSourceText -match "hiddenByTargetNum=") -and
    ($appSourceText -match "hiddenByTargetViewValid=") -and
    ($appSourceText -match "hiddenByWeaponViewValid=") -and
    ($appSourceText -match "beyondFarClip=") -and
    ($appSourceText -match "NO_VISIBLE_TARGETS") -and
    ($appSourceText -match "BACKGROUND_ONLY_FRAME")
$checks.Add((Add-Check "Stage6B.2 frame diagnostics exist" $stage6FrameDiagOk $appSource)) | Out-Null

$backgroundToggleOk = ($appSourceText -match 'getBool\("Stage6Display",\s*"BackgroundDisplayEnable",\s*"Stage6BackgroundDisplayEnable",\s*true') -and
    ($appSourceText -match "u_stage6_background_display_en") -and
    ($appSourceText -match "backgroundDisplay=")
$checks.Add((Add-Check "Stage6 background display safety toggle exists" $backgroundToggleOk $appSource)) | Out-Null

$noTerrainOk = ($appActiveText -notmatch "heightmap|terrainMesh|GeoMipTerrain|real terrain|真实地形|地形网格")
$checks.Add((Add-Check "Stage7A does not add real terrain" $noTerrainOk $appSource)) | Out-Null

$noRadianceTablesOk = ($appSourceText -notmatch "Stage7[^\r\n]*(path_radiance|sky_radiance|solar_irradiance|solarIrradiance)")
$checks.Add((Add-Check "Stage7A does not connect path/sky/solar radiance tables" $noRadianceTablesOk $appSource)) | Out-Null

$noImagingExpansionOk = ($appSourceText -notmatch "Stage7[^\r\n]*(\bAGC\b|\bMTF\b|blur|H264|H\.264|UDP video)")
$checks.Add((Add-Check "Stage7A does not add complex post imaging or video protocols" $noImagingExpansionOk $appSource)) | Out-Null

$stage6DisplayStillOk = ($appSourceText -match "u_stage6_display_en") -and
    ($appSourceText -match "u_stage6_final_white_hot") -and
    ($appSourceText -match "u_stage6_final_display_gain") -and
    ($appSourceText -match "u_stage6_final_noise_enable") -and
    ($appSourceText -match "\[Stage6 FinalPipeline\]") -and
    ($appSourceText -match "windowSource=final_sensor") -and
    ($appSourceText -match "tcpSource=final_sensor")
$checks.Add((Add-Check "Stage6B final display route remains intact" $stage6DisplayStillOk $appSource)) | Out-Null

$stage4Stage5BoundaryOk = ($appSourceText -match 'getBool\("Stage5",\s*"EnableRadianceDebug",\s*"EnableStage5RadianceDebug",\s*false') -and
    ($appHeaderText -match "m_enableStage5RadianceDebug = false") -and
    ($stage3AtmosphereText -match "m_useModtranTauForAtmosphere\(false\)") -and
    ($appSourceText -match 'getBool\("Stage3",\s*"UseModtranTauForAtmosphere",\s*"UseModtranTauForAtmosphere",\s*false') -and
    ($stage4CheckText -match "engineState") -and
    ($stage4CheckText -match "strikeFlag")
$checks.Add((Add-Check "Stage4/Stage5 defaults and semantic checks remain present" $stage4Stage5BoundaryOk "$appHeader; $appSource; $stage4Check")) | Out-Null

$stage7SmokeGracefulExitOk = (Test-Path -LiteralPath $stage7Smoke -PathType Leaf) -and
    ($stage7SmokeText -match "HwaSimIRExitOnStop") -and
    ($stage7SmokeText -match "New-ControlStopPacket") -and
    ($stage7SmokeText -match "Stop-HwaProcessGracefully")
$checks.Add((Add-Check "stage7 sky horizon smoke exits HwaSimIR gracefully" $stage7SmokeGracefulExitOk $stage7Smoke)) | Out-Null

$stage7SmokeCoverageOk = ($stage7SmokeText -match "Stage7DebugMode") -and
    ($stage7SmokeText -match "SkyOnly") -and
    ($stage7SmokeText -match "GroundOnly") -and
    ($stage7SmokeText -match "SkyGroundColor") -and
    ($stage7SmokeText -match "30000") -and
    ($stage7SmokeText -match "80000") -and
    ($stage7SmokeText -match "radiusLessThanFarClip=1") -and
    ($stage7SmokeText -match "Stage7 Real3DBackground") -and
    ($stage7SmokeText -match "flatGroundPlane=0") -and
    ($stage7SmokeText -match "Stage7 GroundReference") -and
    ($stage7SmokeText -match "mode=reference_zero")
$checks.Add((Add-Check "stage7 smoke covers debug modes and 30km/80km far clips" $stage7SmokeCoverageOk $stage7Smoke)) | Out-Null

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage7 sky horizon check failed:" -ForegroundColor Red
    $failed | Format-List
    if ($Strict) {
        exit 1
    }
}
else {
    Write-Host ""
    Write-Host "Stage7 sky horizon check passed." -ForegroundColor Green
}
