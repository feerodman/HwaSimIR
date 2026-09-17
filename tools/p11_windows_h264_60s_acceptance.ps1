param(
    [ValidateSet('Both','SWIR','NIR','MWIR')][string]$Band = 'Both',
    [ValidateRange(3, 120)][int]$Seconds = 60,
    [string]$OutputRoot = '',
    [switch]$Run
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$phase2a = Join-Path $PSScriptRoot 'phase2a_sync60_save_smoke.ps1'
$analyzer = Join-Path $PSScriptRoot 'p11_windows_h264_acceptance_analyze.py'
$ffmpegRoot = Join-Path $repo '.deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1'
$ffmpeg = Join-Path $ffmpegRoot 'bin\ffmpeg.exe'
$ffprobe = Join-Path $ffmpegRoot 'bin\ffprobe.exe'
$dumpbin = 'C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\dumpbin.exe'
$hwaExe = Join-Path $repo 'HwaSim_IR\Bin\HwaSim_IR.exe'
$videoExe = Join-Path $repo 'HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe'
$stimulusExe = Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe'
$fixture = Join-Path $PSScriptRoot 'p11_inputs\civil_van_close_lookat.json'
$python = 'F:\Programs\anaconda3\python.exe'
if (!(Test-Path -LiteralPath $python)) { $python = (Get-Command python -ErrorAction Stop).Source }
if (!$OutputRoot) {
    $OutputRoot = Join-Path $repo ("logs\p11\windows\h264-60s-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Get-Sha256OrNull {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Get-BytesOrNull {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (Get-Item -LiteralPath $Path).Length
}

function Get-PeDependencies {
    param([string]$Path)
    if (!(Test-Path -LiteralPath $dumpbin) -or !(Test-Path -LiteralPath $Path)) { return @() }
    $text = (& $dumpbin /dependents $Path 2>&1 | Out-String)
    return @([regex]::Matches($text, '(?im)^\s+([A-Za-z0-9_.-]+\.dll)\s*$') |
        ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)
}

function Get-ConfigHashes {
    $paths = [ordered]@{
        runtime = Join-Path $repo 'HwaSim_IR\Bin\Config\HwaSimIRRuntime.ini'
        hwaNetwork = Join-Path $repo 'HwaSim_IR\Bin\Config\NetworkConfig.ini'
        videoNetwork = Join-Path $repo 'HwaSim_IR_VideoDisplay\x64\Release\NetworkConfig.ini'
        stimulusNetwork = Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\NetworkConfig.ini'
    }
    $hashes = [ordered]@{}
    foreach ($name in $paths.Keys) { $hashes[$name] = Get-Sha256OrNull $paths[$name] }
    return $hashes
}

$senderImports = @(Get-PeDependencies $hwaExe)
$receiverImports = @(Get-PeDependencies $videoExe)
$requiredImports = @('avcodec-62.dll','avformat-62.dll','avutil-60.dll','swscale-9.dll')
$preflightErrors = New-Object System.Collections.Generic.List[string]
foreach ($path in @($phase2a,$analyzer,$ffmpeg,$ffprobe,$dumpbin,$hwaExe,$videoExe,$stimulusExe,$fixture,
        (Join-Path $ffmpegRoot 'include\libavcodec\avcodec.h'),
        (Join-Path $ffmpegRoot 'lib\avcodec.lib'))) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { $preflightErrors.Add("missing:$path") }
}
foreach ($dependency in $requiredImports) {
    if ($senderImports -notcontains $dependency) { $preflightErrors.Add("sender_import_missing:$dependency") }
    if ($receiverImports -notcontains $dependency) { $preflightErrors.Add("receiver_import_missing:$dependency") }
    foreach ($directory in @((Split-Path -Parent $hwaExe),(Split-Path -Parent $videoExe))) {
        if (!(Test-Path -LiteralPath (Join-Path $directory $dependency))) {
            $preflightErrors.Add("runtime_dll_missing:$directory\$dependency")
        }
    }
}

$preflight = [ordered]@{
    schema = 'hwasimir_p11_windows_h264_binary_preflight_1'
    result = $(if ($preflightErrors.Count -eq 0) { 'PASS' } else { 'FAIL' })
    hwaExe = [ordered]@{path=$hwaExe;bytes=(Get-BytesOrNull $hwaExe);sha256=(Get-Sha256OrNull $hwaExe);imports=$senderImports}
    videoDisplayExe = [ordered]@{path=$videoExe;bytes=(Get-BytesOrNull $videoExe);sha256=(Get-Sha256OrNull $videoExe);imports=$receiverImports}
    stimulusExe = [ordered]@{path=$stimulusExe;bytes=(Get-BytesOrNull $stimulusExe);sha256=(Get-Sha256OrNull $stimulusExe)}
    ffmpegRoot = $ffmpegRoot
    requiredImports = $requiredImports
    errors = @($preflightErrors)
    buildCommands = [ordered]@{
        hwa = "`$env:FFMPEG_ROOT='$ffmpegRoot'; & 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:FFMPEG_ROOT=`$env:FFMPEG_ROOT /verbosity:minimal /nologo"
        videoDisplay = "`$env:FFMPEG_ROOT='$ffmpegRoot'; & 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe' .\HwaSim_IR_VideoDisplay\HwaSim_IR_VideoDisplay.sln /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:QtInstall='D:\Qt\Qt5.12.12\5.12.12\msvc2015_64' /p:FFMPEG_ROOT=`$env:FFMPEG_ROOT /verbosity:minimal /nologo"
    }
}
$preflightPath = Join-Path $OutputRoot 'binary_preflight.json'
$preflight | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $preflightPath -Encoding UTF8
Write-Output "[P11WindowsH264] preflight=$($preflight.result) evidence=$preflightPath"
if ($preflight.result -ne 'PASS') {
    $preflight.errors | ForEach-Object { Write-Error $_ -ErrorAction Continue }
    exit 2
}
if (!$Run) {
    Write-Output '[P11WindowsH264] preflight-only; no process launched and no configuration modified. Add -Run after final EXEs are frozen.'
    exit 0
}

$active = @(Get-Process HwaSim_IR,HwaSim_IR_VideoDisplay,DataDrivenTestQT -ErrorAction SilentlyContinue)
if ($active.Count -gt 0) {
    throw "Acceptance requires exclusive local processes; active=$($active.ProcessName -join ',')"
}

$bands = if ($Band -eq 'Both') {
    @([ordered]@{name='SWIR';protocol=0}, [ordered]@{name='MWIR';protocol=2})
} elseif ($Band -eq 'SWIR') {
    @([ordered]@{name='SWIR';protocol=0})
} elseif ($Band -eq 'NIR') {
    @([ordered]@{name='NIR';protocol=1})
} else {
    @([ordered]@{name='MWIR';protocol=2})
}

$environmentNames = @('WeatherCameraInput','WeatherStartSec','P6TestTargetType','RenderPresentationMode','HwaInputAuditDirectory')
$environmentBefore = @{}
foreach ($name in $environmentNames) { $environmentBefore[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
$configBefore = Get-ConfigHashes
$bandResults = New-Object System.Collections.Generic.List[object]
try {
    foreach ($spec in $bands) {
        $bandDir = Join-Path $OutputRoot $spec.name
        New-Item -ItemType Directory -Force -Path $bandDir | Out-Null
        $env:WeatherCameraInput = $fixture
        $env:WeatherStartSec = '0'
        $env:P6TestTargetType = '0x55'
        $env:RenderPresentationMode = 'HeadlessOffscreen'
        $env:HwaInputAuditDirectory = Join-Path $bandDir 'input_audit'
        New-Item -ItemType Directory -Force -Path $env:HwaInputAuditDirectory | Out-Null
        $started = [DateTime]::UtcNow
        $invoke = @{
            Seconds=$Seconds;PostStimulusWaitSeconds=3;InitAckTimeoutSeconds=30;StopCompletionTimeoutSeconds=60
            StimSensorBand=[int]$spec.protocol;StimH264En='1';EnableH264Experimental='true'
            H264Encoder='ffmpeg';H264FallbackToJpeg='false';H264ForceKeyFrameOnStart='true'
            H264BitrateKbps=4000;H264GopFrames=30;H264LowLatency='true';EnablePerfLog='true'
            Stage5LogComponents='true';Stage5ComponentLogEveryFrames=120
            M1CompareOnly='false';M1EnableRuntime='true';M1EnableNIRRuntime='true'
            M1EnableSWIRRuntime='true';M1EnableMWIRRuntime='true'
            NaturalSolarEnable='true';NaturalSolarEnableOpticalShadow='true';NaturalSolarEnableSolarThermal='true'
            EnableAGC='true';AGCDebugLog='false';AnnotationOverlayInSensorImage='true';PostprocessAA='Off'
        }
        & $phase2a @invoke *> (Join-Path $bandDir 'phase2a_runner.log')
        $formal = Get-ChildItem -LiteralPath (Join-Path $repo 'logs') -Directory -Filter 'phase2a-final-*' |
            Where-Object { $_.LastWriteTimeUtc -ge $started.AddSeconds(-2) } |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        if (!$formal) { throw "phase2a output not found for $($spec.name)" }
        $formalCopy = Join-Path $bandDir 'formal_tcp_run'
        Copy-Item -LiteralPath $formal.FullName -Destination $formalCopy -Recurse -Force
        $summary = Get-Content -LiteralPath (Join-Path $formal.FullName 'phase2a_sync60_save_summary.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        if (!$summary.roundDir -or !(Test-Path -LiteralPath $summary.roundDir -PathType Container)) {
            throw "receiver recording directory missing for $($spec.name)"
        }
        Copy-Item -LiteralPath $summary.roundDir -Destination (Join-Path $bandDir 'receiver_recording') -Recurse -Force
        & $python $analyzer $bandDir --band $spec.name --expected-seconds $Seconds --expected-fps 60 `
            --latency-threshold-ms 80 --ffmpeg $ffmpeg --ffprobe $ffprobe *> (Join-Path $bandDir 'analyzer.log')
        $analysisExit = $LASTEXITCODE
        $analysisPath = Join-Path $bandDir 'p11_windows_h264_acceptance_summary.json'
        $analysis = if (Test-Path -LiteralPath $analysisPath) {
            Get-Content -LiteralPath $analysisPath -Raw -Encoding UTF8 | ConvertFrom-Json
        } else { $null }
        $bandResults.Add([ordered]@{
            band=$spec.name;result=$(if ($analysis) {$analysis.result} else {'FAIL'})
            analyzerExitCode=$analysisExit;summary=$analysisPath
        })
    }
} finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable($name, $environmentBefore[$name], 'Process')
    }
}

$configAfter = Get-ConfigHashes
$restoreErrors = New-Object System.Collections.Generic.List[string]
foreach ($name in $configBefore.Keys) {
    if ($configBefore[$name] -ne $configAfter[$name]) {
        $restoreErrors.Add("config_not_restored:${name}:$($configBefore[$name])!=$($configAfter[$name])")
    }
}
$overallPass = $restoreErrors.Count -eq 0 -and @($bandResults | Where-Object {$_.result -ne 'PASS'}).Count -eq 0
$restoreErrorArray = @($restoreErrors | ForEach-Object { [string]$_ })
$bandResultArray = @($bandResults | ForEach-Object { $_ })
$delivery = [ordered]@{
    schema='hwasimir_p11_windows_h264_dual_band_acceptance_1'
    result=$(if ($overallPass) { 'PASS' } else { 'FAIL' });requestedBand=$Band;seconds=$Seconds
    preflight=$preflight;configHashesBefore=$configBefore;configHashesAfter=$configAfter
    configurationRestored=($restoreErrors.Count -eq 0);restoreErrors=$restoreErrorArray;bands=$bandResultArray
}
$deliveryPath = Join-Path $OutputRoot 'p11_windows_h264_dual_band_summary.json'
$delivery | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $deliveryPath -Encoding UTF8
Write-Output "[P11WindowsH264] result=$($delivery.result) summary=$deliveryPath"
if (!$overallPass) { exit 1 }
