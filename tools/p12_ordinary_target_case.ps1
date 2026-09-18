[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Name,
    [int]$Seconds = 8,
	[string]$OutputRoot = 'logs/p12/p12b/ordinary_assets',
	[switch]$SkipAnnexBDump,
	[switch]$SkipWidgetCapture
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$senderDir = Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release\release'
$receiverDir = Join-Path $repo 'HwaSim_IR_VideoDisplay\x64\Release'
$senderExe = Join-Path $senderDir 'DataDrivenTestQT.exe'
$receiverExe = Join-Path $receiverDir 'HwaSim_IR_VideoDisplay.exe'
$senderConfig = Join-Path $senderDir 'NetworkConfig.ini'
$receiverConfig = Join-Path $receiverDir 'NetworkConfig.ini'
$out = if ([IO.Path]::IsPathRooted($OutputRoot)) { Join-Path $OutputRoot $Name } else { Join-Path $repo (Join-Path $OutputRoot $Name) }

foreach ($path in @($senderExe,$receiverExe,$senderConfig,$receiverConfig)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing ordinary runtime file: $path" }
}
if (Test-Path -LiteralPath $out) { throw "Refusing to overwrite existing evidence directory: $out" }
New-Item -ItemType Directory -Path $out | Out-Null
$out = (Resolve-Path -LiteralPath $out).Path

$businessOverrides = @(
    'P5Scene','P5View','P5AssetBandCase','P6Scene','P6ExistingTargets','P6TestTargetType',
    'WeatherCameraInput','WeatherStartSec','SensorDisplayPreset','HwaSimIRDdsVideoTopic',
    'HwaSimIRProtocolPlatId','HwaSimIRProtocolSensorId','ZRDDS_QOS_FILE'
)
foreach ($nameToCheck in $businessOverrides) {
    if (-not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($nameToCheck,'Process'))) {
        throw "Unexpected inherited business override: $nameToCheck"
    }
}
$conflicts = @(Get-Process -Name DataDrivenTestQT,HwaSim_IR_VideoDisplay -ErrorAction SilentlyContinue)
if ($conflicts.Count -ne 0) { throw "Ordinary sender/receiver already running: $($conflicts.Id -join ',')" }

$senderConfigText = Get-Content -LiteralPath $senderConfig -Raw
$targetMatch = [regex]::Match($senderConfigText, '(?im)^TargetType\s*=\s*(\S+)\s*$')
$bandMatch = [regex]::Match($senderConfigText, '(?im)^trackerSensorBand\s*=\s*(\d+)\s*$')
if (-not $targetMatch.Success -or -not $bandMatch.Success) { throw 'Effective sender sidecar lacks TargetType or trackerSensorBand' }
$targetType = $targetMatch.Groups[1].Value
$band = [int]$bandMatch.Groups[1].Value
if ($band -notin @(0,2)) { throw "P12 asset case requires formal SWIR(0) or MWIR(2), got $band" }

Copy-Item -LiteralPath $senderConfig -Destination (Join-Path $out 'DataDrivenTestQT.NetworkConfig.ini')
Copy-Item -LiteralPath $receiverConfig -Destination (Join-Path $out 'VideoDisplay.NetworkConfig.ini')
$request = [ordered]@{
    schema = 1
    name = $Name
    purpose = 'P12B ordinary UI/INI target selection through DDS auto-discovery'
    targetType = $targetType
    protocolBand = $band
    band = if ($band -eq 0) { 'SWIR' } else { 'MWIR' }
    seconds = $Seconds
    sender = [ordered]@{ path=$senderExe; workingDirectory=$senderDir; sha256=(Get-FileHash -LiteralPath $senderExe -Algorithm SHA256).Hash.ToLowerInvariant(); args=@("--duration-sec=$Seconds") }
    receiver = [ordered]@{ path=$receiverExe; workingDirectory=$receiverDir; sha256=(Get-FileHash -LiteralPath $receiverExe -Algorithm SHA256).Hash.ToLowerInvariant(); args=@() }
    senderConfigSha256 = (Get-FileHash -LiteralPath $senderConfig -Algorithm SHA256).Hash.ToLowerInvariant()
    receiverConfigSha256 = (Get-FileHash -LiteralPath $receiverConfig -Algorithm SHA256).Hash.ToLowerInvariant()
    forcedIdentityTopicQos = $false
    businessOverrides = @()
    diagnostics = @(
        if(-not $SkipWidgetCapture){'actual receiver widget capture';'actual sender widget capture'}
        if(-not $SkipAnnexBDump){'received Annex-B copy'}
    )
    startedUtc = [DateTime]::UtcNow.ToString('o')
}
[IO.File]::WriteAllText((Join-Path $out 'request.json'), ($request | ConvertTo-Json -Depth 6) + "`n", [Text.UTF8Encoding]::new($false))

$oldQt = $env:QT_FORCE_STDERR_LOGGING
$oldReceiverDump = $env:P6ReceiverUiDump
$oldResponsive = $env:P6ReceiverUiResponsiveCapture
$oldSenderDump = $env:P7SenderUiDump
$oldVideoPath = $env:P5DdsVideoPath
$oldVideoSamples = $env:P5DdsVideoSamples
$receiver = $null
$sender = $null
try {
    $taskPath = [Environment]::GetEnvironmentVariable('Path','Process')
    [Environment]::SetEnvironmentVariable('PATH',$null,'Process')
    [Environment]::SetEnvironmentVariable('Path',$taskPath,'Process')
    $env:QT_FORCE_STDERR_LOGGING = '1'
	if($SkipWidgetCapture){$env:P6ReceiverUiDump=$null;$env:P6ReceiverUiResponsiveCapture=$null}
	else{$env:P6ReceiverUiDump = Join-Path $out 'receiver_actual_widget.png';$env:P6ReceiverUiResponsiveCapture = '1'}
	if($SkipAnnexBDump){$env:P5DdsVideoPath=$null;$env:P5DdsVideoSamples=$null}
	else{
		$env:P5DdsVideoPath = Join-Path $out 'received.h264'
		$env:P5DdsVideoSamples = [string]([Math]::Max(120,$Seconds*60))
	}
    $receiver = Start-Process -FilePath $receiverExe -WorkingDirectory $receiverDir -WindowStyle Normal -PassThru `
        -RedirectStandardOutput (Join-Path $out 'receiver.out.log') -RedirectStandardError (Join-Path $out 'receiver.err.log')
    Start-Sleep -Seconds 2
	if($SkipWidgetCapture){$env:P7SenderUiDump=$null}else{$env:P7SenderUiDump = Join-Path $out 'sender_actual_widget.png'}
    $sender = Start-Process -FilePath $senderExe -WorkingDirectory $senderDir -WindowStyle Normal -PassThru `
        -ArgumentList @("--duration-sec=$Seconds") `
        -RedirectStandardOutput (Join-Path $out 'sender.out.log') -RedirectStandardError (Join-Path $out 'sender.err.log')
    if (-not $sender.WaitForExit(($Seconds + 45) * 1000)) { throw 'Ordinary sender did not finish its requested round' }
    Start-Sleep -Seconds 3
    if (-not $receiver.HasExited) {
        [void]$receiver.CloseMainWindow()
        if (-not $receiver.WaitForExit(10000)) { Stop-Process -Id $receiver.Id -Force }
    }
}
finally {
    foreach ($process in @($sender,$receiver)) {
        if ($process) { $process.Refresh(); if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force } }
    }
    $env:QT_FORCE_STDERR_LOGGING = $oldQt
    $env:P6ReceiverUiDump = $oldReceiverDump
    $env:P6ReceiverUiResponsiveCapture = $oldResponsive
    $env:P7SenderUiDump = $oldSenderDump
    $env:P5DdsVideoPath = $oldVideoPath
    $env:P5DdsVideoSamples = $oldVideoSamples
}

$requiredFiles=@()
if(-not $SkipWidgetCapture){$requiredFiles+=@('receiver_actual_widget.png','sender_actual_widget.png')}
if(-not $SkipAnnexBDump){$requiredFiles+='received.h264'}
foreach ($required in $requiredFiles) {
    $path = Join-Path $out $required
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-Item -LiteralPath $path).Length -eq 0) {
        throw "Ordinary target case missing evidence: $required"
    }
}
$senderLog = Get-Content -LiteralPath (Join-Path $out 'sender.err.log') -Raw
$receiverLog = (Get-Content -LiteralPath (Join-Path $out 'receiver.out.log') -Raw) + "`n" +
    (Get-Content -LiteralPath (Join-Path $out 'receiver.err.log') -Raw)
if ($senderLog -notmatch ('\[StimTargetSelection\].*targetType=' + [regex]::Escape($targetType.ToUpperInvariant()) + '.*source=ordinary_ui_ini')) {
    throw "Sender did not publish the ordinary UI/INI target type $targetType"
}
if ($receiverLog -notmatch '(decodedFrames=|"decodedFrames"\s*:\s*")[1-9][0-9]*' -or
    $receiverLog -match '(decodeErrors=|"decodeErrors"\s*:\s*")[1-9][0-9]*') {
    throw 'Receiver did not prove successful DDS H.264 decode'
}
$request.finishedUtc = [DateTime]::UtcNow.ToString('o')
$request.result = 'PASS'
$request.evidence = [ordered]@{
    senderTargetSelection = ([regex]::Matches($senderLog,'(?m)^.*\[StimTargetSelection\].*$') | Select-Object -First 4 | ForEach-Object Value)
    receiverFinal = ([regex]::Matches($receiverLog,'(?m)^.*\[RuntimeMetricsV2\].*$') | Select-Object -Last 1 | ForEach-Object Value)
	widgetSha256 = if($SkipWidgetCapture){$null}else{(Get-FileHash -LiteralPath (Join-Path $out 'receiver_actual_widget.png') -Algorithm SHA256).Hash.ToLowerInvariant()}
	annexBSha256 = if($SkipAnnexBDump){$null}else{(Get-FileHash -LiteralPath (Join-Path $out 'received.h264') -Algorithm SHA256).Hash.ToLowerInvariant()}
}
[IO.File]::WriteAllText((Join-Path $out 'result.json'), ($request | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
Write-Output "[P12OrdinaryTargetCase] result=PASS name=$Name targetType=$targetType band=$band path=$out"
