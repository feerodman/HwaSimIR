param(
    [string]$DeliveryRoot = 'D:\HwaSimIR\logs\p14\deliverables\HwaSimIR_P14_Delivery',
    [string]$OutputDirectory = 'D:\HwaSimIR\logs\p15\p14_media_audit',
    [string]$Ffmpeg = 'D:\HwaSimIR\.deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1\bin\ffmpeg.exe',
    [string]$Ffprobe = 'D:\HwaSimIR\.deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1\bin\ffprobe.exe'
)

$ErrorActionPreference = 'Stop'
$delivery = (Resolve-Path -LiteralPath $DeliveryRoot).Path
$manifestPath = Join-Path $delivery 'manifest.json'
$manifest = Get-Content -Raw -Encoding UTF8 -LiteralPath $manifestPath | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$decodeSlugs = @(
    'original_swir',
    'original_mwir',
    'ground_swir_cloud_visibility',
    'ground_mwir_rain',
    'ground_swir_snow',
    'plume_mwir_on',
    'plume_mwir_off',
    'first_valid_swir'
)

function Get-SourceClass([string]$Slug) {
    if ($Slug -like 'original_*') { return 'controlled_original_1txt_dds_replay_not_user_ordinary_entry' }
    if ($Slug -like 'ground_*') { return 'controlled_synthetic_civil_ground_fixture_not_user_ordinary_entry' }
    if ($Slug -like 'plume_*') { return 'controlled_generic_heat_source_fixture_not_user_ordinary_entry' }
    if ($Slug -like 'first_valid_*') { return 'independent_generated_dds_fixture_not_user_ordinary_entry' }
    if ($Slug -eq 'mwir_snow_300s') { return 'controlled_complex_weather_long_test_not_user_ordinary_entry' }
    return 'historical_controlled_case_not_user_ordinary_entry'
}

$records = New-Object System.Collections.Generic.List[object]
foreach ($video in $manifest.videos) {
    $videoPath = Join-Path $delivery ($video.path -replace '/', '\')
    $caseResultPath = Join-Path $delivery ($video.caseResult -replace '/', '\')
    $mediaQcPath = Join-Path $delivery ($video.mediaQc -replace '/', '\')
    $caseResult = Get-Content -Raw -Encoding UTF8 -LiteralPath $caseResultPath | ConvertFrom-Json
    $mediaQc = Get-Content -Raw -Encoding UTF8 -LiteralPath $mediaQcPath | ConvertFrom-Json
    $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $videoPath).Hash.ToLowerInvariant()
    $actualBytes = (Get-Item -LiteralPath $videoPath).Length
    $probeText = & $Ffprobe -v error -count_frames -select_streams v:0 `
        -show_entries stream=codec_name,width,height,avg_frame_rate,nb_frames,nb_read_frames:format=duration,size `
        -of json $videoPath | Out-String
    if ($LASTEXITCODE -ne 0) { throw "ffprobe failed for $videoPath" }
    $probe = $probeText | ConvertFrom-Json

    $decodeStatus = 'NOT_RERUN_IN_P15'
    $decodeError = $null
    if ($decodeSlugs -contains $video.slug) {
        $decodeLog = Join-Path $OutputDirectory ($video.slug + '.decode.stderr.log')
        & $Ffmpeg -hide_banner -loglevel error -i $videoPath -map 0:v:0 -pix_fmt gray -f rawvideo NUL `
            2> $decodeLog
        if ($LASTEXITCODE -eq 0) {
            $decodeStatus = 'PASS'
        } else {
            $decodeStatus = 'FAIL'
            $decodeError = (Get-Content -Raw -ErrorAction SilentlyContinue -LiteralPath $decodeLog)
        }
    }

    $keyframes = @()
    if ($mediaQc.products -and $mediaQc.products.keyframes) {
        foreach ($keyframe in $mediaQc.products.keyframes) {
            $packagedPath = Join-Path (Split-Path -Parent $caseResultPath) `
                ('keyframes\' + $keyframe.label + '.png')
            $keyframes += [ordered]@{
                label = $keyframe.label
                seconds = $keyframe.seconds
                packagedPath = $packagedPath
                packagedExists = Test-Path -LiteralPath $packagedPath
                sha256 = $keyframe.sha256
            }
        }
    } elseif ($caseResult.products.receivedKeyframe) {
        $packagedPath = Join-Path (Split-Path -Parent $caseResultPath) 'received_keyframe.png'
        $keyframes += [ordered]@{
            label = 'received_keyframe'
            seconds = $null
            packagedPath = $packagedPath
            packagedExists = Test-Path -LiteralPath $packagedPath
            sha256 = if (Test-Path -LiteralPath $packagedPath) {
                (Get-FileHash -Algorithm SHA256 -LiteralPath $packagedPath).Hash.ToLowerInvariant()
            } else { $null }
        }
    }

    $boardIdentity = @($caseResult.boardPreflight)
    $record = [ordered]@{
        slug = $video.slug
        title = $video.title
        actualFile = $videoPath
        actualBytes = $actualBytes
        manifestBytes = $video.bytes
        actualSha256 = $actualHash
        manifestSha256 = $video.sha256
        fileHashMatchesManifest = ($actualHash -eq $video.sha256 -and $actualBytes -eq $video.bytes)
        caseResultFile = $caseResultPath
        mediaQcFile = $mediaQcPath
        caseName = $caseResult.name
        sourceClass = Get-SourceClass $video.slug
        fromUserOrdinaryEntry = $false
        historicalVersion = $true
        controlledFixture = $caseResult.controlledFixture
        inputMode = $caseResult.inputMode
        input = $caseResult.input
        inputSha256 = $caseResult.inputSha256
        startedUtc = $caseResult.startedUtc
        finishedUtc = $caseResult.finishedUtc
        band = $caseResult.band
        weather = $caseResult.weather
        environment = $caseResult.environment
        materialView = $caseResult.materialView
        resolution = if ($caseResult.resolution) { $caseResult.resolution } else { $caseResult.identity.resolution }
        programAndConfigIdentity = [ordered]@{
            finalElfSha256 = if ($caseResult.finalElfSha256) { $caseResult.finalElfSha256 } else { $manifest.finalElfSha256 }
            senderExeSha256 = $caseResult.senderExeSha256
            receiverExeSha256 = $caseResult.receiverExeSha256
            senderConfigSha256 = $caseResult.senderConfigSha256
            receiverConfigSha256 = $caseResult.receiverConfigSha256
            boardPreflight = $boardIdentity
        }
        currentProbe = [ordered]@{
            codec = $probe.streams[0].codec_name
            width = $probe.streams[0].width
            height = $probe.streams[0].height
            avgFrameRate = $probe.streams[0].avg_frame_rate
            frames = if ($probe.streams[0].nb_read_frames) { $probe.streams[0].nb_read_frames } else { $probe.streams[0].nb_frames }
            durationSeconds = $probe.format.duration
            bytes = $probe.format.size
        }
        p15CurrentFullDecode = $decodeStatus
        p15CurrentDecodeError = $decodeError
        p14HistoricalFullDecode = $video.fullDecode
        p14PacketTimestamps = $video.packetTimestamps
        keyframes = $keyframes
        clockRelationship = $mediaQc.timeline
    }
    $records.Add([pscustomobject]$record)
}

$allHashMatch = (@($records | Where-Object { -not $_.fileHashMatchesManifest }).Count -eq 0)
$currentDecoded = @($records | Where-Object { $_.p15CurrentFullDecode -ne 'NOT_RERUN_IN_P15' })
$currentDecodePass = (@($currentDecoded | Where-Object { $_.p15CurrentFullDecode -ne 'PASS' }).Count -eq 0)
$report = [ordered]@{
    schema = 'hwasimir.p15.p14-media-readonly-audit.v1'
    createdUtc = [DateTime]::UtcNow.ToString('o')
    mode = 'read_only_existing_p14_media'
    p14Manifest = $manifestPath
    p14ManifestSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $manifestPath).Hash.ToLowerInvariant()
    advertisedRootDeliveryExists = Test-Path -LiteralPath 'D:\HwaSimIR\deliverables\HwaSimIR_P14_Delivery'
    actualDeliveryRoot = $delivery
    actualVideoCount = $records.Count
    allVideoFilesAndHashesMatchManifest = $allHashMatch
    p14HistoricalFullDecodeClaim = 'PASS_15_OF_15'
    p15CurrentFullDecodeSelected = $currentDecoded.Count
    p15CurrentFullDecodePassed = @($currentDecoded | Where-Object { $_.p15CurrentFullDecode -eq 'PASS' }).Count
    p15CurrentFullDecodeResult = if ($currentDecodePass) { 'PASS' } else { 'FAIL' }
    acceptanceBoundary = 'Decode, frame count, packet timestamps, and signed raw differences do not prove correct visibility in the user ordinary window.'
    feedbackStatus = [ordered]@{
        ordinaryBlackPlume = 'REOPENED'
        ordinaryWeatherNotVisible = 'REOPENED'
    }
    records = @($records | ForEach-Object { $_ })
}

$reportPath = Join-Path $OutputDirectory 'p14_media_readonly_audit.json'
$report | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $reportPath -Encoding UTF8
$report | ConvertTo-Json -Depth 6
