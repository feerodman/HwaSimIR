param(
    [ValidateSet(
        "v2_jpeg",
        "v2_h264",
        "v3_video_jpeg",
        "v3_video_h264",
        "v3_h264_annotation",
        "v3_h264_realtime",
        "v3_h264_all",
        "v3_annotation_realtime",
        "v3_all_disabled")]
    [string[]]$Cases = @(
        "v2_jpeg",
        "v2_h264",
        "v3_video_jpeg",
        "v3_video_h264",
        "v3_h264_annotation",
        "v3_h264_realtime",
        "v3_h264_all",
        "v3_annotation_realtime",
        "v3_all_disabled"),
    [int]$Seconds = 12,
    [string]$FFmpegRoot = $env:FFMPEG_ROOT
)

$ErrorActionPreference = "Stop"
$Seconds = [Math]::Max(8, $Seconds)
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$runner = Join-Path $root "tools\phase2a_sync60_save_smoke.ps1"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runDir = Join-Path $root "logs\v4-packet-v3-$stamp"
$utf8 = New-Object Text.UTF8Encoding($false)

if (-not (Test-Path -LiteralPath $runner)) {
    throw "Required runner not found: $runner"
}
if ([string]::IsNullOrWhiteSpace($FFmpegRoot)) {
    throw "FFMPEG_ROOT is required for the V4 Windows H.264 matrix"
}
foreach ($required in @(
    (Join-Path $FFmpegRoot "include\libavcodec\avcodec.h"),
    (Join-Path $FFmpegRoot "lib\avcodec.lib"))) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "FFmpeg SDK path missing: $required"
    }
}
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$catalog = @{
    v2_jpeg = [pscustomobject]@{
        Version=2; Video=$true; Annotation=$true; Realtime=$true; H264=$false; Flags="7"
    }
    v2_h264 = [pscustomobject]@{
        Version=2; Video=$true; Annotation=$true; Realtime=$true; H264=$true; Flags="7"
    }
    v3_video_jpeg = [pscustomobject]@{
        Version=3; Video=$true; Annotation=$false; Realtime=$false; H264=$false; Flags="4"
    }
    v3_video_h264 = [pscustomobject]@{
        Version=3; Video=$true; Annotation=$false; Realtime=$false; H264=$true; Flags="4"
    }
    v3_h264_annotation = [pscustomobject]@{
        Version=3; Video=$true; Annotation=$true; Realtime=$false; H264=$true; Flags="6"
    }
    v3_h264_realtime = [pscustomobject]@{
        Version=3; Video=$true; Annotation=$false; Realtime=$true; H264=$true; Flags="5"
    }
    v3_h264_all = [pscustomobject]@{
        Version=3; Video=$true; Annotation=$true; Realtime=$true; H264=$true; Flags="7"
    }
    v3_annotation_realtime = [pscustomobject]@{
        Version=3; Video=$false; Annotation=$true; Realtime=$true; H264=$true; Flags="3"
    }
    v3_all_disabled = [pscustomobject]@{
        Version=3; Video=$false; Annotation=$false; Realtime=$false; H264=$true; Flags="0"
    }
}

function Convert-BoolText([bool]$Value) {
    if ($Value) { return "1" }
    return "0"
}

function Read-Text([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return "" }
    return [IO.File]::ReadAllText($Path)
}

function Add-Failure([Collections.Generic.List[string]]$Failures, [bool]$Condition, [string]$Reason) {
    if (-not $Condition) { $Failures.Add($Reason) }
}

$environmentNames = @(
    "FFMPEG_ROOT",
    "TcpPacketVersion",
    "TcpSendVideo",
    "TcpSendAnnotation",
    "TcpSendRealtimeData",
    "TcpForwardInitControl",
    "EnableH264Experimental",
    "H264Encoder",
    "H264FallbackToJpeg")
$savedEnvironment = @{}
foreach ($name in $environmentNames) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
}

$results = @()
try {
    $env:FFMPEG_ROOT = $FFmpegRoot
    $env:TcpForwardInitControl = "1"
    $env:EnableH264Experimental = "1"
    $env:H264Encoder = "ffmpeg"
    $env:H264FallbackToJpeg = "1"

    foreach ($caseName in $Cases) {
        $case = $catalog[$caseName]
        $env:TcpPacketVersion = [string]$case.Version
        $env:TcpSendVideo = Convert-BoolText $case.Video
        $env:TcpSendAnnotation = Convert-BoolText $case.Annotation
        $env:TcpSendRealtimeData = Convert-BoolText $case.Realtime

        $caseDir = Join-Path $runDir $caseName
        New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
        $caseStart = Get-Date
        $runnerOutput = @(& $runner `
            -Seconds $Seconds `
            -EnableH264Experimental "true" `
            -H264Encoder "ffmpeg" `
            -H264FallbackToJpeg "true" `
            -StimH264En $(if ($case.H264) { "1" } else { "0" }) `
            -StimExtraArgs @("--save-mp4=0") 2>&1)
        [IO.File]::WriteAllLines(
            (Join-Path $caseDir "runner.log"),
            [string[]]($runnerOutput | ForEach-Object { [string]$_ }),
            $utf8)

        $latestResult = Get-ChildItem -LiteralPath (Join-Path $root "logs") -Directory |
            Where-Object {
                $_.Name -like "phase2a-final-*" -and
                $_.LastWriteTime -ge $caseStart.AddSeconds(-2)
            } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if (-not $latestResult) {
            throw "Could not locate phase2a logs for $caseName"
        }

        $hwaText = Read-Text (Join-Path $latestResult.FullName "hwa.out.log")
        $hwaErrorText = Read-Text (Join-Path $latestResult.FullName "hwa.err.log")
        $videoText = Read-Text (Join-Path $latestResult.FullName "video.err.log")
        $stimText = Read-Text (Join-Path $latestResult.FullName "stim.err.log")
        Copy-Item -LiteralPath (Join-Path $latestResult.FullName "hwa.out.log") -Destination $caseDir
        Copy-Item -LiteralPath (Join-Path $latestResult.FullName "hwa.err.log") -Destination $caseDir
        Copy-Item -LiteralPath (Join-Path $latestResult.FullName "video.err.log") -Destination $caseDir
        Copy-Item -LiteralPath (Join-Path $latestResult.FullName "stim.err.log") -Destination $caseDir

        $failures = [Collections.Generic.List[string]]::new()
        $configPattern = "\[TcpPayloadConfig\].*PacketVersion=$($case.Version).*SendVideo=$(Convert-BoolText $case.Video).*SendAnnotation=$(Convert-BoolText $case.Annotation).*SendRealtimeData=$(Convert-BoolText $case.Realtime).*ForwardInitControl=1"
        Add-Failure $failures ($hwaText -match $configPattern) "effective_payload_config_missing"
        Add-Failure $failures ($hwaText -match "\[TcpInitForward\]") "init_forward_missing"

        if ($caseName -eq "v3_all_disabled") {
            $warningCount = [regex]::Matches(
                $hwaText,
                "\[TcpPayloadConfig\]\[WARN\].*reason=all_frame_sections_disabled").Count
            Add-Failure $failures ($warningCount -eq 1) "all_disabled_warning_not_exactly_once"
            Add-Failure $failures ($hwaText -notmatch "\[TcpFramePacket\]") "frame_packet_sent_while_all_disabled"
            Add-Failure $failures ($videoText -notmatch "\[TcpFramePacketRx\]") "receiver_got_frame_while_all_disabled"
        }
        else {
            $senderPacket = "\[TcpFramePacket\].*packetVersion=$($case.Version).*flags=0x$($case.Flags)"
            $receiverPacket = "\[TcpFramePacketRx\].*packetVersion=$($case.Version).*flags=0x$($case.Flags)"
            Add-Failure $failures ($hwaText -match $senderPacket) "sender_packet_shape_missing"
            Add-Failure $failures ($videoText -match $receiverPacket) "receiver_packet_shape_missing"

            $expectedRealtime = if ($case.Version -eq 2 -or $case.Realtime) { "[1-9][0-9]*" } else { "0" }
            $expectedAnnotation = if ($case.Version -eq 2 -or $case.Annotation) { "[1-9][0-9]*" } else { "0" }
            $expectedVideo = if ($case.Version -eq 2 -or $case.Video) { "[1-9][0-9]*" } else { "0" }
            $lengthPattern = "\[TcpFramePacket\].*realtimeBytes=$expectedRealtime.*annotationBytes=$expectedAnnotation.*videoBytes=$expectedVideo"
            Add-Failure $failures ($hwaText -match $lengthPattern) "sender_section_lengths_wrong"
        }

        $effectiveVideo = $case.Version -eq 2 -or $case.Video
        $effectiveAnnotation = $case.Version -eq 2 -or $case.Annotation
        $effectiveRealtime = $case.Version -eq 2 -or $case.Realtime
        if ($effectiveVideo) {
            $codec = if ($case.H264) { "h264_annexb" } else { "jpeg" }
            Add-Failure $failures ($hwaText -match "\[TcpPerf\].*activeCodec=$codec") "sender_codec_not_active"
            Add-Failure $failures ($videoText -match "\[VideoPerf\].*activeCodec=$codec") "receiver_codec_not_active"
            if ($case.H264) {
                # Runtime diagnostics are emitted by several threads.  A
                # concurrent Stage0 line can split the VideoEncoder prefix
                # from the configured/backend fields, so accept the complete
                # configured record even when the tag is interleaved.
                Add-Failure $failures ($hwaText -match "(?:\[VideoEncoder\].*)?configured=1.*activeBackend=ffmpeg") "ffmpeg_backend_not_active"
                Add-Failure $failures ($hwaText -match "\[TcpFramePacket\].*keyFrame=1") "sender_idr_not_seen"
                Add-Failure $failures ($videoText -match "\[TcpFramePacketRx\].*keyFrame=1") "receiver_idr_not_seen"
            }
        }
        else {
            Add-Failure $failures ($hwaText -notmatch "\[VideoEncoder\].*configured=1") "encoder_ran_without_video_section"
            Add-Failure $failures ($hwaText -notmatch "jpegBytes=[1-9][0-9]*") "jpeg_ran_without_video_section"
            Add-Failure $failures ($videoText -notmatch "(decodeFailed=1|JPEG.*decode.*failed|H264.*decode.*failed|解码失败)") "decoder_error_on_metadata_only_packet"
            if ($caseName -ne "v3_all_disabled") {
                Add-Failure $failures ($videoText -match "\[TcpFramePacketRx\].*hasVideo=0") "metadata_only_packet_not_observed"
            }
        }
        if (-not $effectiveAnnotation -and $caseName -ne "v3_all_disabled") {
            Add-Failure $failures ($hwaText -match "\[TcpFramePacket\].*annotationBytes=0") "annotation_generated_when_disabled"
        }
        if (-not $effectiveRealtime -and $caseName -ne "v3_all_disabled") {
            Add-Failure $failures ($hwaText -match "\[TcpFramePacket\].*realtimeBytes=0") "realtime_sent_when_disabled"
        }
        Add-Failure $failures ($hwaErrorText -notmatch "(?i)(access violation|assertion failed|fatal error)") "hwa_runtime_error"
        Add-Failure $failures ($stimText -match "\[StimPerf\]") "stimulus_perf_missing"

        $pass = $failures.Count -eq 0
        $result = [pscustomobject]@{
            case = $caseName
            packetVersion = $case.Version
            flags = "0x$($case.Flags)"
            sendVideo = $case.Video
            sendAnnotation = $case.Annotation
            sendRealtimeData = $case.Realtime
            requestedCodec = $(if ($case.H264) { "h264" } else { "jpeg" })
            sourceLogDir = $latestResult.FullName
            pass = $pass
            failureReasons = @($failures)
        }
        $results += $result
        $result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $caseDir "summary.json") -Encoding UTF8
        Write-Output "[V4PacketCase] case=$caseName packetVersion=$($case.Version) flags=0x$($case.Flags) status=$(if ($pass) { 'PASS' } else { 'FAIL' }) failures=$($failures -join ',')"
    }
}
finally {
    foreach ($name in $savedEnvironment.Keys) {
        [Environment]::SetEnvironmentVariable(
            $name,
            $savedEnvironment[$name],
            "Process")
    }
}

$allPass = @($results).Count -eq @($Cases).Count -and
    @($results | Where-Object { -not $_.pass }).Count -eq 0
$summary = [pscustomobject]@{
    generatedAt = (Get-Date).ToString("o")
    ffmpegRoot = $FFmpegRoot
    secondsPerCase = $Seconds
    pass = $allPass
    cases = $results
}
$summary | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath (Join-Path $runDir "summary.json") -Encoding UTF8
$results | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath (Join-Path $runDir "summary.csv")
Write-Output "summary=$(Join-Path $runDir 'summary.json')"
Write-Output "V4_PACKET_ACCEPTANCE=$(if ($allPass) { 'PASS' } else { 'FAIL' })"
if (-not $allPass) {
    throw "V4 Packet v3 acceptance failed; see $runDir"
}
