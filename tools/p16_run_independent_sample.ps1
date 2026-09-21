[CmdletBinding()]
param(
    [string]$Root = '',
    [string]$OutputDirectory = '',
    [switch]$ReuseExistingMedia
)

$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $Root 'logs\p16\independent_sample' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path $OutputDirectory).Path
$exe = Join-Path $Root 'samples\P16IndependentGraphics\bin\P16IndependentGraphics.exe'
$config = Join-Path $Root 'samples\P16IndependentGraphics\config\sample.ini'
$script = Join-Path $Root 'samples\P16IndependentGraphics\app\p16_graphics_sample.py'
$analyzer = Join-Path $Root 'tools\p16_analyze_independent_sample.py'
foreach ($path in @($exe, $config, $script, $analyzer)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing independent sample input: $path" }
}

$cases = @(
    'effects_off','effects_glow','effects_smoke','effects_cloud','effects_rain','effects_snow',
    'effects_combo_glow_smoke','effects_combo_cloud_rain',
    'texture_static_nearest','texture_motion_nearest','texture_motion_linear',
    'texture_overlap_before','texture_overlap_after'
)
$entries = @()
foreach ($caseId in $cases) {
    $caseDirectory = Join-Path $OutputDirectory $caseId
    New-Item -ItemType Directory -Force -Path $caseDirectory | Out-Null
    if (-not $ReuseExistingMedia) {
        & $exe --case $caseId --output $caseDirectory --config $config
        if ($LASTEXITCODE -ne 0) { throw "Independent renderer failed for $caseId with $LASTEXITCODE" }
    }
    $manifest = Get-Content -LiteralPath (Join-Path $caseDirectory 'case_manifest.json') -Raw | ConvertFrom-Json
    $frames = Join-Path $caseDirectory 'frames\frame_%04d.png'
    $video = Join-Path $caseDirectory ($caseId + '.mp4')
    if (-not $ReuseExistingMedia) {
        & ffmpeg -v error -y -framerate $manifest.fps -start_number 0 -i $frames -frames:v $manifest.frames -c:v libx264 -preset medium -crf 17 -pix_fmt yuv420p -movflags +faststart $video
        if ($LASTEXITCODE -ne 0) { throw "MP4 encode failed for $caseId with $LASTEXITCODE" }
    }
    if (-not (Test-Path -LiteralPath $video -PathType Leaf)) { throw "Missing rendered MP4 for $caseId" }
    $probe = & ffprobe -v error -count_frames -select_streams v:0 -show_entries stream=codec_name,width,height,avg_frame_rate,nb_read_frames -show_entries format=duration -of json $video | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw "ffprobe failed for $caseId" }
    $stream = $probe.streams[0]
    $decodeSink = if ($IsWindows) { 'NUL' } else { 'NUL' }
    & ffmpeg -v error -i $video -map 0:v:0 -f null $decodeSink
    $decodePassed = $LASTEXITCODE -eq 0
    $keyframeDirectory = Join-Path $caseDirectory 'keyframes'
    New-Item -ItemType Directory -Force -Path $keyframeDirectory | Out-Null
    foreach ($frame in @(0, [int]($manifest.frames / 2), ([int]$manifest.frames - 1))) {
        Copy-Item -LiteralPath (Join-Path $caseDirectory ('frames\frame_{0:D4}.png' -f $frame)) -Destination (Join-Path $keyframeDirectory ('frame_{0:D4}.png' -f $frame)) -Force
    }
    $entries += [pscustomobject][ordered]@{
        caseId = $caseId
        classification = $manifest.classification
        program = $exe
        programSha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
        scriptSha256 = $manifest.programSha256
        config = $config
        configSha256 = $manifest.configSha256
        resolution = "$($stream.width)x$($stream.height)"
        frames = [int]$stream.nb_read_frames
        fps = $stream.avg_frame_rate
        durationSeconds = [double]$probe.format.duration
        timebase = $manifest.timebase
        seed = $manifest.seed
        effectToggles = $manifest.effectToggles
        eventTimeline = @([pscustomobject]@{ frame = 0; simulationTimeSeconds = 0; event = 'case switches fixed for complete run' })
        trace = (Join-Path $caseDirectory 'frame_trace.jsonl')
        traceSha256 = (Get-FileHash -LiteralPath (Join-Path $caseDirectory 'frame_trace.jsonl') -Algorithm SHA256).Hash.ToLowerInvariant()
        uniquePngHashes = $manifest.uniquePngHashes
        mp4 = $video
        mp4Sha256 = (Get-FileHash -LiteralPath $video -Algorithm SHA256).Hash.ToLowerInvariant()
        decodable = $decodePassed
        continuousRender = $true
        synthesizedFromFewStills = $false
        productionModulesImported = $false
        productionAssetsRead = $false
        businessTrajectoryRead = $false
    }
}

$analysisDirectory = Join-Path $OutputDirectory 'analysis'
& python $analyzer --root $OutputDirectory --output $analysisDirectory
if ($LASTEXITCODE -ne 0) { throw "Independent sample pixel analysis failed with $LASTEXITCODE" }
$analysis = Get-Content -LiteralPath (Join-Path $analysisDirectory 'pixel_evidence.json') -Raw | ConvertFrom-Json
foreach ($entry in $entries) {
    $effect = @($analysis.effects | Where-Object caseId -eq $entry.caseId)
    $texture = @($analysis.textureCases | Where-Object caseId -eq $entry.caseId)
    if ($effect.Count) {
        $entry | Add-Member -NotePropertyName effectDiscernible -NotePropertyValue ([bool]$effect[0].effectDiscernible)
        $entry | Add-Member -NotePropertyName observation -NotePropertyValue 'Pixel contribution differs from effects_off outside labels.'
    } elseif ($texture.Count) {
        $entry | Add-Member -NotePropertyName effectDiscernible -NotePropertyValue $null
        $entry | Add-Member -NotePropertyName observation -NotePropertyValue ("Texture temporal mean delta={0:N4}; unique board frames={1}" -f $texture[0].meanConsecutiveDelta,$texture[0].uniqueRegionFrameHashes)
    } else {
        $entry | Add-Member -NotePropertyName effectDiscernible -NotePropertyValue $null
        $entry | Add-Member -NotePropertyName observation -NotePropertyValue 'All requested effects off baseline.'
    }
}
$index = [ordered]@{
    schema = 'P16.IndependentOrdinaryGraphicsMediaIndex.1'
    createdUtc = [DateTime]::UtcNow.ToString('o')
    scope = 'Independent ordinary RGB graphics only; no production inference and no IR calibration.'
    sdk = 'Panda3D 1.10.15 installed at F:\Programs\Panda3D-1.10.15-x64'
    program = [ordered]@{
        executable = $exe
        executableSha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
        script = $script
        scriptSha256 = (Get-FileHash -LiteralPath $script -Algorithm SHA256).Hash.ToLowerInvariant()
        config = $config
        configSha256 = (Get-FileHash -LiteralPath $config -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    statuses = [ordered]@{
        allVideosDecodable = (@($entries | Where-Object { -not $_.decodable }).Count -eq 0)
        allSingleEffectsDiscernible = [bool]$analysis.allRequestedSingleEffectsDiscernible
        independentRootCauseLocated = [bool]$analysis.independentRootCauseAndFix.rootCauseLocated
        independentProblemFixed = [bool]$analysis.independentRootCauseAndFix.problemFixed
        productionWeatherClosed = $false
        productionPlumeClosed = $false
    }
    cases = $entries
    analysis = (Join-Path $analysisDirectory 'pixel_evidence.json')
}
$utf8 = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'media_index.json'), ($index | ConvertTo-Json -Depth 12) + "`n", $utf8)
Write-Output ($index.statuses | ConvertTo-Json -Compress)
