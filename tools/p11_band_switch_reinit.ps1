[CmdletBinding()]
param(
    [ValidateSet("Check", "Run")]
    [string]$Mode = "Check",

    [ValidateRange(4, 120)]
    [int]$Seconds = 8,

    [string]$OutputRoot = "",

    [string]$InputFile = ""
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
$hwaWork = Join-Path $repo "HwaSim_IR\Bin"
$videoWork = Join-Path $repo "HwaSim_IR_VideoDisplay\x64\Release"
$stimWork = Join-Path $repo "build-DataDrivenTestQT-codex-mingw73_64-Release\release"

$hwaCandidates = @(
    (Join-Path $repo "HwaSim_IR\Bin\HwaSim_IR.exe"),
    (Join-Path $repo "HwaSim_IR\HwaSim_IR\Bin\HwaSim_IR.exe")
)
# The runtime-root copy must win when present because the executable resolves
# Config relative to its own binary directory.  MSBuild also leaves an identical
# nested copy whose adjacent Config directory is intentionally incomplete.
$hwaExe = @($hwaCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1)
if ($hwaExe.Count -gt 0) { $hwaExe = [string]$hwaExe[0] } else { $hwaExe = $hwaCandidates[0] }
$videoExe = Join-Path $videoWork "HwaSim_IR_VideoDisplay.exe"
$stimExe = Join-Path $stimWork "DataDrivenTestQT.exe"

$runtimeIni = Join-Path $hwaWork "Config\HwaSimIRRuntime.ini"
$hwaNetworkIni = Join-Path $hwaWork "Config\NetworkConfig.ini"
$videoNetworkIni = Join-Path $videoWork "NetworkConfig.ini"
$stimNetworkIni = Join-Path $stimWork "NetworkConfig.ini"
$mp4Root = Join-Path $videoWork "MP4"
$swirProfile = Join-Path $hwaWork "Config\SensorWave\default_SWIR.json"
$mwirProfile = Join-Path $hwaWork "Config\SensorWave\default_MWIR.json"
$formalLut = Join-Path $hwaWork "Config\Atmosphere\MODTRAN\processed\band_lut_si.csv"
$bandSwitchFixture = Join-Path $repo "tools\p11_inputs\civil_van_band_switch_1km.json"
$resolvedInputFile = if ([string]::IsNullOrWhiteSpace($InputFile)) {
    # DataDriven still needs its normal row source; the controlled civil target
    # and camera state are injected through WeatherCameraInput below.
    Join-Path $stimWork "1.txt"
} elseif ([IO.Path]::IsPathRooted($InputFile)) {
    $InputFile
} else {
    Join-Path $repo $InputFile
}

$ffmpegCandidate = Join-Path $repo ".deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1\bin\ffmpeg.exe"
$ffprobeCandidate = Join-Path $repo ".deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1\bin\ffprobe.exe"

$roundPlan = @(
    [pscustomobject]@{ ordinal = 1; name = "01_swir"; band = 0; bandName = "SWIR"; previousBandName = ""; lowUm = 1.1; highUm = 2.5 },
    [pscustomobject]@{ ordinal = 2; name = "02_mwir"; band = 2; bandName = "MWIR"; previousBandName = "SWIR"; lowUm = 3.0; highUm = 5.0 },
    [pscustomobject]@{ ordinal = 3; name = "03_swir_reinit"; band = 0; bandName = "SWIR"; previousBandName = "MWIR"; lowUm = 1.1; highUm = 2.5 }
)

function Get-ToolPath {
    param([string]$Candidate, [string]$CommandName)
    if (Test-Path -LiteralPath $Candidate) { return (Get-Item -LiteralPath $Candidate).FullName }
    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if ($null -ne $command) { return $command.Source }
    return ""
}

function Get-Sha256OrMissing {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return "MISSING" }
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
}

function Get-NewestSourceUtc {
    param([string]$Root)
    if (-not (Test-Path -LiteralPath $Root)) { return [datetime]::MinValue }
    $latest = Get-ChildItem -LiteralPath $Root -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Extension -in @(".cpp", ".c", ".h", ".hpp", ".inl", ".ui", ".qrc") -and
            $_.FullName -notmatch "(?i)[\\/](Thirdparty library|opencv2-440|x64|build)[\\/]"
        } | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($null -eq $latest) { return [datetime]::MinValue }
    return $latest.LastWriteTimeUtc
}

function Test-SourceTokens {
    param([string]$Path, [string[]]$Tokens)
    if (-not (Test-Path -LiteralPath $Path)) { return $false }
    $text = [IO.File]::ReadAllText($Path)
    foreach ($token in $Tokens) {
        if (-not $text.Contains($token)) { return $false }
    }
    return $true
}

function Get-Preflight {
    $ffmpeg = Get-ToolPath $ffmpegCandidate "ffmpeg.exe"
    $ffprobe = Get-ToolPath $ffprobeCandidate "ffprobe.exe"
    $required = @($hwaExe, $videoExe, $stimExe, $runtimeIni, $swirProfile, $mwirProfile, $formalLut, $resolvedInputFile, $bandSwitchFixture, $ffmpeg, $ffprobe)
    $missing = @($required | Where-Object { [string]::IsNullOrWhiteSpace($_) -or -not (Test-Path -LiteralPath $_) })

    $hwaSourceUtc = Get-NewestSourceUtc (Join-Path $repo "HwaSim_IR\HwaSim_IR")
    $videoSourceUtc = Get-NewestSourceUtc (Join-Path $repo "HwaSim_IR_VideoDisplay\HwaSim_IR_VideoDisplay")
    $stimSourceUtc = Get-NewestSourceUtc (Join-Path $repo "DataDrivenTestQT")
    $freshness = [ordered]@{}
    foreach ($item in @(
        [pscustomobject]@{ name = "HwaSim_IR"; exe = $hwaExe; sourceUtc = $hwaSourceUtc },
        [pscustomobject]@{ name = "VideoDisplay"; exe = $videoExe; sourceUtc = $videoSourceUtc },
        [pscustomobject]@{ name = "DataDrivenTestQT"; exe = $stimExe; sourceUtc = $stimSourceUtc }
    )) {
        $exists = Test-Path -LiteralPath $item.exe
        $exeUtc = if ($exists) { (Get-Item -LiteralPath $item.exe).LastWriteTimeUtc } else { [datetime]::MinValue }
        $freshness[$item.name] = [pscustomobject]@{
            exe = $item.exe
            sha256 = Get-Sha256OrMissing $item.exe
            exeUtc = $exeUtc.ToString("o")
            newestSourceUtc = $item.sourceUtc.ToString("o")
            fresh = $exists -and $exeUtc -ge $item.sourceUtc
        }
    }

    $hwaSource = Join-Path $repo "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
    $videoSource = Join-Path $repo "HwaSim_IR_VideoDisplay\HwaSim_IR_VideoDisplay\HwaSim_IR_VideoDisplay.cpp"
    $stimSource = Join-Path $repo "DataDrivenTestQT\main.cpp"
    $sourceContracts = [ordered]@{
        stimulusAckGatesStart = Test-SourceTokens $stimSource @("initAckReceived", "onStartButtonClicked", "--sensor-band", "requested < 0 || requested > 2")
        initRejectsUnsupported = Test-SourceTokens $hwaSource @("supportsProductionProtocolBand", "action=reject_init", "formal_atmosphere_band_missing")
        initClearsBandState = Test-SourceTokens $hwaSource @("m_lastStage5RadianceComponentLogState.clear()", "m_stage5ModtranRadianceCache.clear()", "m_lastCapturedSourceSeq = 0")
        shaderConsumesBandAndM1 = Test-SourceTokens $hwaSource @("u_ir_band_index", "u_m1_physics_runtime_en", "u_m1_display_scale")
        receiverResetsOnInit = Test-SourceTokens $videoSource @("resetVideoPerfStats()", "m_recorder->configure", "startPending")
    }

    $profileContracts = [ordered]@{ SWIR = $false; MWIR = $false }
    if (Test-Path -LiteralPath $swirProfile) {
        $profile = Get-Content -LiteralPath $swirProfile -Raw | ConvertFrom-Json
        $cfg = $profile.Systems.SensorConfigurationSystem
        $profileContracts.SWIR = $profile.HwaSimIR.Band -eq "SWIR" -and
            [double]$cfg.SpectralResponseRangeLow -eq 1.1 -and [double]$cfg.SpectralResponseRangeHigh -eq 2.5
    }
    if (Test-Path -LiteralPath $mwirProfile) {
        $profile = Get-Content -LiteralPath $mwirProfile -Raw | ConvertFrom-Json
        $cfg = $profile.Systems.SensorConfigurationSystem
        $profileContracts.MWIR = $profile.HwaSimIR.Band -eq "MWIR" -and
            [double]$cfg.SpectralResponseRangeLow -eq 3.0 -and [double]$cfg.SpectralResponseRangeHigh -eq 5.0
    }

    $lutContracts = [ordered]@{ SWIR = $false; MWIR = $false; rectangularBand = $false }
    if (Test-Path -LiteralPath $formalLut) {
        $lutText = [IO.File]::ReadAllText($formalLut)
        $lutContracts.SWIR = [regex]::IsMatch($lutText, "(?m)^[^,]+,[^,]+,SWIR,")
        $lutContracts.MWIR = [regex]::IsMatch($lutText, "(?m)^[^,]+,[^,]+,MWIR,")
        $lutContracts.rectangularBand = $lutText.Contains("RectangularBand")
    }

    $allContracts = @($sourceContracts.Values) + @($profileContracts.Values) + @($lutContracts.Values)
    $allFresh = @($freshness.Values | Where-Object { -not $_.fresh }).Count -eq 0
    $ready = $missing.Count -eq 0 -and @($allContracts | Where-Object { -not $_ }).Count -eq 0 -and $allFresh
    return [pscustomobject]@{
        mode = $Mode
        readOnly = ($Mode -eq "Check")
        readyForRun = $ready
        processLaunches = 0
        configWrites = 0
        plan = @($roundPlan | Select-Object ordinal, name, band, bandName, previousBandName)
        invalidBandEvidencePlan = @("3", "4", "VIS-SWIR")
        requiredMissing = $missing
        binaryFreshness = $freshness
        sourceContracts = $sourceContracts
        profileContracts = $profileContracts
        lutContracts = $lutContracts
        ffmpeg = $ffmpeg
        ffprobe = $ffprobe
        inputFile = $resolvedInputFile
        inputFileSha256 = Get-Sha256OrMissing $resolvedInputFile
    }
}

function Set-IniSectionValue {
    param([string]$Text, [string]$Section, [string]$Key, [string]$Value)
    $sectionPattern = "(?ms)(^\[" + [regex]::Escape($Section) + "\]\s*.*?)(?=^\[|\z)"
    $sectionMatch = [regex]::Match($Text, $sectionPattern)
    if (-not $sectionMatch.Success) {
        return $Text.TrimEnd() + "`r`n`r`n[$Section]`r`n$Key=$Value`r`n"
    }
    $block = $sectionMatch.Groups[1].Value
    $keyPattern = "(?m)^" + [regex]::Escape($Key) + "=.*$"
    if ([regex]::IsMatch($block, $keyPattern)) {
        $replacement = [regex]::Replace($block, $keyPattern, "$Key=$Value")
    } else {
        $replacement = $block.TrimEnd() + "`r`n$Key=$Value`r`n"
    }
    return $Text.Substring(0, $sectionMatch.Index) + $replacement +
        $Text.Substring($sectionMatch.Index + $sectionMatch.Length)
}

function Get-SharedFileLength {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) { return [long]0 }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try { return $stream.Length } finally { $stream.Dispose() }
}

function Read-SharedFileSlice {
    param([string]$Path, [long]$Offset)
    if (-not (Test-Path -LiteralPath $Path)) { return ,([byte[]]@()) }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        if ($Offset -gt $stream.Length) { throw "Log was truncated while process was alive: $Path" }
        [void]$stream.Seek($Offset, [IO.SeekOrigin]::Begin)
        $remaining = [int]($stream.Length - $Offset)
        $bytes = New-Object byte[] $remaining
        $read = 0
        while ($read -lt $remaining) {
            $count = $stream.Read($bytes, $read, $remaining - $read)
            if ($count -le 0) { break }
            $read += $count
        }
        if ($read -eq $remaining) { return ,$bytes }
        $trimmed = New-Object byte[] $read
        [Array]::Copy($bytes, $trimmed, $read)
        return ,$trimmed
    } finally { $stream.Dispose() }
}

function Get-SliceText {
    param([string]$Path, [long]$Offset)
    $bytes = Read-SharedFileSlice $Path $Offset
    return [Text.Encoding]::UTF8.GetString($bytes)
}

function Wait-SliceContains {
    param([string]$Path, [long]$Offset, [string]$Needle, [int]$TimeoutSeconds = 25)
    $deadline = [datetime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if ((Get-SliceText $Path $Offset).Contains($Needle)) { return }
        Start-Sleep -Milliseconds 250
    } while ([datetime]::UtcNow -lt $deadline)
    throw "Timed out waiting for '$Needle' in $Path"
}

function Get-TagLines {
    param([string]$Text, [string]$Tag)
    return @($Text -split "`r?`n" | Where-Object { $_.StartsWith("[$Tag]") })
}

function Get-Field {
    param([string]$Line, [string]$Name)
    # Concurrent producer diagnostics can occasionally meet at a redirected
    # stream boundary without an intervening newline.  A new '[' starts the
    # next structured tag and is never part of a scalar key/value field.
    $match = [regex]::Match($Line, "(?:^|\s)" + [regex]::Escape($Name) + "=([^\s\[]+)")
    if (-not $match.Success) { return "" }
    return $match.Groups[1].Value.Trim('"')
}

function Get-IntField {
    param([string]$Line, [string]$Name)
    $value = Get-Field $Line $Name
    if ($value -notmatch "^-?[0-9]+$") { throw "Missing/non-integer field $Name in: $Line" }
    return [int64]$value
}

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw "P11 band-switch gate failed: $Message" }
}

function Assert-AllFieldEquals {
    param([string[]]$Lines, [string]$Field, [string]$Expected, [string]$Context)
    Assert-True ($Lines.Count -gt 0) "$Context has no log rows"
    foreach ($line in $Lines) {
        Assert-True ((Get-Field $line $Field) -eq $Expected) "$Context field $Field expected ${Expected}: $line"
    }
}

function Get-MaxIntField {
    param([string[]]$Lines, [string]$Field)
    Assert-True ($Lines.Count -gt 0) "no rows while reading maximum $Field"
    $values = @($Lines | ForEach-Object { Get-IntField $_ $Field })
    return [int64](($values | Measure-Object -Maximum).Maximum)
}

function Wait-NewRecordingDirectory {
    param([string[]]$Before, [int]$TimeoutSeconds = 25)
    $deadline = [datetime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $now = if (Test-Path -LiteralPath $mp4Root) {
            @(Get-ChildItem -LiteralPath $mp4Root -Directory -ErrorAction SilentlyContinue)
        } else { @() }
        $newDirs = @($now | Where-Object { $Before -notcontains $_.FullName })
        $finalized = @($newDirs | Where-Object {
            (Test-Path -LiteralPath (Join-Path $_.FullName "recording_status.json")) -and
            (Test-Path -LiteralPath (Join-Path $_.FullName "output.mp4"))
        })
        if ($finalized.Count -eq 1) { return $finalized[0] }
        if ($finalized.Count -gt 1) { throw "More than one new recording directory appeared" }
        Start-Sleep -Milliseconds 250
    } while ([datetime]::UtcNow -lt $deadline)
    throw "Timed out waiting for finalized receiver recording"
}

function Copy-RecordingDirectory {
    param([string]$Source, [string]$Destination)
    [void](New-Item -ItemType Directory -Path $Destination)
    foreach ($item in Get-ChildItem -LiteralPath $Source -Force) {
        Copy-Item -LiteralPath $item.FullName -Destination $Destination -Recurse
    }
}

function Invoke-ExternalChecked {
    param([string]$FilePath, [string[]]$Arguments, [string]$Purpose)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Purpose failed with exit code $LASTEXITCODE" }
}

function Test-RoundEvidence {
    param(
        [pscustomobject]$Spec,
        [string]$StimText,
        [string]$HwaText,
        [string]$VideoText,
        [string]$RecordingDirectory,
        [string]$FfprobePath
    )

    $stimProtocol = @(Get-TagLines $StimText "StimProtocol")
    $stimWeather = @(Get-TagLines $StimText "StimWeather")
    $stimInit = @(Get-TagLines $StimText "StimInit")
    $stimAck = @(Get-TagLines $StimText "StimInitAck")
    $stimFirstFrame = @(Get-TagLines $StimText "AeroSpeedSend" | Where-Object { (Get-Field $_ "sourceSeq") -eq "1" })
    $stimFinal = @(Get-TagLines $StimText "StimFinal")
    Assert-True ($stimProtocol.Count -eq 1) "$($Spec.name): exactly one StimProtocol required"
    Assert-True ((Get-Field $stimProtocol[0] "simMode") -eq "1") "$($Spec.name): StimSimMode must be 1"
    Assert-True ((Get-Field $stimProtocol[0] "saveMP4En") -eq "1") "$($Spec.name): saveMP4En must be 1"
    Assert-True ($stimWeather.Count -eq 1 -and (Get-Field $stimWeather[0] "sensorBand") -eq [string]$Spec.band) "$($Spec.name): StimWeather band mismatch"
    Assert-True ($stimInit.Count -eq 1 -and (Get-Field $stimInit[0] "sensorBand") -eq [string]$Spec.band) "$($Spec.name): StimInit band mismatch"
    Assert-True ($stimAck.Count -eq 1 -and (Get-Field $stimAck[0] "received") -eq "1") "$($Spec.name): one successful INIT ACK required"
    Assert-True ($stimFirstFrame.Count -ge 1) "$($Spec.name): no realtime sourceSeq=1 after ACK"
    Assert-True ($StimText.IndexOf($stimAck[0]) -lt $StimText.IndexOf($stimFirstFrame[0])) "$($Spec.name): START/realtime preceded INIT ACK"
    Assert-True ($stimFinal.Count -eq 1) "$($Spec.name): exactly one StimFinal required"
    $sent = Get-IntField $stimFinal[0] "successfulRealtimeWrites"
    Assert-True ($sent -gt 0) "$($Spec.name): stimulus sent no frames"

    $renderControl = @(Get-TagLines $HwaText "RenderControl")
    Assert-True ($renderControl.Count -ge 1) "$($Spec.name): missing RenderControl"
    Assert-AllFieldEquals $renderControl "effectiveSimMode" "1" "$($Spec.name) RenderControl"
    Assert-AllFieldEquals $renderControl "asyncInputPolicy" "OrderedQueue" "$($Spec.name) RenderControl"

    $profileLines = @($HwaText -split "`r?`n" | Where-Object { $_.StartsWith("[Stage1] Sensor profile (init-command):") })
    Assert-True ($profileLines.Count -eq 1) "$($Spec.name): exactly one init-command profile refresh required"
    $profileLine = $profileLines[0]
    Assert-True ($profileLine.Contains("protocolBand=$($Spec.band), band=$($Spec.bandName)")) "$($Spec.name): active profile band mismatch"
    Assert-True ($profileLine.Contains("sensorRange=$([string]::Format([Globalization.CultureInfo]::InvariantCulture, '{0:F3}', $Spec.lowUm))-$([string]::Format([Globalization.CultureInfo]::InvariantCulture, '{0:F3}', $Spec.highUm))um")) "$($Spec.name): active profile spectral range mismatch"
    Assert-True ($profileLine.Contains("default_$($Spec.bandName).json")) "$($Spec.name): active profile source mismatch"

    # Multiple producer threads share stdout, so a small number of diagnostics
    # may be byte-interleaved.  Retain only rows carrying a complete band token;
    # the gate still requires valid first-frame and later evidence for every tag.
    $validBands = @("SWIR", "MWIR")
    $m1Rows = @(Get-TagLines $HwaText "M1 Compare" | Where-Object { $validBands -contains (Get-Field $_ "band") })
    $componentRows = @(Get-TagLines $HwaText "Stage5 RadianceComponents" | Where-Object { $validBands -contains (Get-Field $_ "band") })
    $modtranRows = @(Get-TagLines $HwaText "Stage5 ModtranRadianceCompare" | Where-Object { $validBands -contains (Get-Field $_ "band") })
    $plumeRows = @(Get-TagLines $HwaText "Stage5 Plume" | Where-Object {
        -not $_.StartsWith("[Stage5 Plume][WARN]") -and $validBands -contains (Get-Field $_ "band")
    })
    Assert-AllFieldEquals $m1Rows "band" $Spec.bandName "$($Spec.name) M1 band refresh"
    Assert-AllFieldEquals $componentRows "band" $Spec.bandName "$($Spec.name) shader band refresh"
    Assert-AllFieldEquals $modtranRows "band" $Spec.bandName "$($Spec.name) MODTRAN band refresh"
    Assert-AllFieldEquals $plumeRows "band" $Spec.bandName "$($Spec.name) plume band refresh"
    $formalPlumeRows = @($plumeRows | Where-Object { -not [string]::IsNullOrEmpty((Get-Field $_ "formalTauReady")) })
    Assert-AllFieldEquals $formalPlumeRows "formalTauReady" "1" "$($Spec.name) formal plume tau"
    $m1First = @($m1Rows | Where-Object { (Get-Field $_ "sourceSeq") -eq "1" })
    $componentFirst = @($componentRows | Where-Object { (Get-Field $_ "sourceSeq") -eq "1" })
    $modtranFirst = @($modtranRows | Where-Object { (Get-Field $_ "sourceSeq") -eq "1" })
    Assert-AllFieldEquals $m1First "band" $Spec.bandName "$($Spec.name) first-frame M1"
    Assert-AllFieldEquals $m1First "finalOutput" "M1" "$($Spec.name) first-frame M1"
    Assert-AllFieldEquals $m1First "compareOnly" "0" "$($Spec.name) first-frame M1"
    Assert-AllFieldEquals $m1First "valid" "1" "$($Spec.name) first-frame M1"
    Assert-AllFieldEquals $m1First "fallbackReason" "none" "$($Spec.name) first-frame M1"
    Assert-AllFieldEquals $m1First "responseMode" "RectangularBand" "$($Spec.name) first-frame M1"
    Assert-AllFieldEquals $componentFirst "band" $Spec.bandName "$($Spec.name) first-frame shader input"
    Assert-AllFieldEquals $componentFirst "modtranRadianceValid" "1" "$($Spec.name) first-frame shader input"
    Assert-AllFieldEquals $componentFirst "modtranFallbackReason" "none" "$($Spec.name) first-frame shader input"
    Assert-AllFieldEquals $modtranFirst "band" $Spec.bandName "$($Spec.name) first-frame MODTRAN"
    Assert-AllFieldEquals $modtranFirst "valid" "1" "$($Spec.name) first-frame MODTRAN"
    Assert-AllFieldEquals $modtranFirst "fallbackReason" "none" "$($Spec.name) first-frame MODTRAN"
    Assert-AllFieldEquals $modtranFirst "responseMode" "RectangularBand" "$($Spec.name) first-frame MODTRAN"
    if (-not [string]::IsNullOrEmpty($Spec.previousBandName)) {
        $firstPhysics = @($m1First + $componentFirst + $modtranFirst)
        foreach ($line in $firstPhysics) {
            Assert-True (-not [regex]::IsMatch($line, "(?:^|\s)band=" + [regex]::Escape($Spec.previousBandName) + "(?:\s|$)")) "$($Spec.name): previous-band cache marker in first new frame"
        }
    }

    $syncRows = @(Get-TagLines $HwaText "SyncRoundConservation")
    Assert-True ($syncRows.Count -eq 1) "$($Spec.name): exactly one SyncRoundConservation required"
    $accepted = Get-IntField $syncRows[0] "acceptedRealtime"
    $captured = Get-IntField $syncRows[0] "lastCapturedSourceSeq"
    Assert-True ((Get-IntField $syncRows[0] "inputMinusCaptured") -eq 0) "$($Spec.name): inputMinusCaptured is nonzero"
    Assert-True ((Get-IntField $syncRows[0] "queueDepth") -eq 0) "$($Spec.name): queue did not drain"
    Assert-True ((Get-IntField $syncRows[0] "staleFramePublished") -eq 0) "$($Spec.name): stale frame published"

    $syncFrames = @(Get-TagLines $HwaText "SyncFrame" | Where-Object { -not $_.StartsWith("[SyncFrame][WARN]") })
    # The sourceSeq=1 diagnostic may be prefixed by a concurrent component log;
    # frame_index is the authoritative first-frame identity ledger below.
    Assert-True ($syncFrames.Count -gt 0) "$($Spec.name): no complete SyncFrame diagnostics"
    Assert-AllFieldEquals $syncFrames "sourceSeqContinuous" "1" "$($Spec.name) SyncFrame"
    Assert-AllFieldEquals $syncFrames "overwritten" "0" "$($Spec.name) SyncFrame"

    $perfRows = @(Get-TagLines $HwaText "Perf")
    $sampledExecuted = Get-MaxIntField $perfRows "renderFrames"
    # Perf/SyncFrame stdout is sampled before the final worker/stream flush and
    # can be one frame behind.  Exact execution follows from accepted==captured;
    # exact publication is independently proven by the receiver source ledger.
    $sampledOutput = Get-MaxIntField $perfRows "outputFrames"
    $sampledUdpFrames = Get-MaxIntField $perfRows "udpFrames"
    Assert-True ((Get-MaxIntField $perfRows "shaderInputSetCount") -gt 0) "$($Spec.name): shader inputs were not refreshed"
    Assert-True ((Get-MaxIntField $perfRows "stage5ModtranCacheMissCount") -gt 0) "$($Spec.name): no formal MODTRAN query observed"
    foreach ($field in @("inputQueueOverflowCount", "dropped", "inputOverwritten", "outputOverwritten", "overwritten")) {
        Assert-AllFieldEquals $perfRows $field "0" "$($Spec.name) Perf"
    }

    $rxRows = @(Get-TagLines $VideoText "TcpFramePacketRx")
    # TcpFramePacketRx is intentionally rate-limited (normally every 120th
    # frame), so the first-frame identity is checked against frame_index below.
    Assert-True ($rxRows.Count -gt 0) "$($Spec.name): no receiver packet diagnostics"
    foreach ($row in $rxRows) {
        Assert-True ((Get-Field $row "frameSeq") -eq (Get-Field $row "outputOrdinal")) "$($Spec.name): sampled receiver frame/output identity mismatch"
    }
    $videoPerf = @(Get-TagLines $VideoText "VideoPerf")
    Assert-AllFieldEquals $videoPerf "sourceSeqContinuous" "1" "$($Spec.name) VideoPerf"
    Assert-AllFieldEquals $videoPerf "discontinuities" "0" "$($Spec.name) VideoPerf"
    Assert-AllFieldEquals $videoPerf "h264DecodeErrors" "0" "$($Spec.name) VideoPerf"
    $recorderPerf = @(Get-TagLines $VideoText "RecorderPerf" | Where-Object { -not $_.StartsWith("[RecorderPerf][WARN]") })
    Assert-AllFieldEquals $recorderPerf "droppedFrames" "0" "$($Spec.name) RecorderPerf"
    Assert-AllFieldEquals $recorderPerf "sourceSeqContinuousWritten" "1" "$($Spec.name) RecorderPerf"

    $indexPath = Join-Path $RecordingDirectory "frame_index.jsonl"
    $statusPath = Join-Path $RecordingDirectory "recording_status.json"
    $mp4Path = Join-Path $RecordingDirectory "output.mp4"
    foreach ($path in @($indexPath, $statusPath, $mp4Path)) { Assert-True (Test-Path -LiteralPath $path) "$($Spec.name): missing recording artifact $path" }
    $index = @((Get-Content -LiteralPath $indexPath) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | ForEach-Object { $_ | ConvertFrom-Json })
    Assert-True ($index.Count -gt 0) "$($Spec.name): empty frame index"
    for ($i = 0; $i -lt $index.Count; ++$i) {
        $expected = $i + 1
        Assert-True ([int64]$index[$i].sourceSeq -eq $expected) "$($Spec.name): sourceSeq discontinuity at storage row $expected"
        Assert-True ([int64]$index[$i].frameSeq -eq $expected) "$($Spec.name): frameSeq discontinuity at storage row $expected"
        Assert-True ([int64]$index[$i].storageIndex -eq $expected) "$($Spec.name): storageIndex discontinuity at row $expected"
    }
    $status = Get-Content -LiteralPath $statusPath -Raw | ConvertFrom-Json
    Assert-True (-not [bool]$status.fileError) "$($Spec.name): recorder fileError"
    Assert-True ([bool]$status.muxerFinalized) "$($Spec.name): muxer not finalized"
    Assert-True ([int64]$status.completeProducts -eq $index.Count) "$($Spec.name): recording status/index count mismatch"
    Assert-True ([int64]$status.lastSourceSeq -eq $index.Count) "$($Spec.name): recording status lastSourceSeq mismatch"
    $probeArgs = @("-v", "error", "-count_frames", "-select_streams", "v:0", "-show_entries", "stream=nb_read_frames", "-of", "default=noprint_wrappers=1:nokey=1", $mp4Path)
    $probeText = (& $FfprobePath @probeArgs | Out-String).Trim()
    Assert-True ($LASTEXITCODE -eq 0 -and $probeText -match "^[0-9]+$") "$($Spec.name): ffprobe frame count failed"
    $mp4Frames = [int64]$probeText
    $received = [int64]$index.Count
    $executed = $captured
    $output = $received
    $udpFrames = $accepted
    $writtenMax = Get-MaxIntField $recorderPerf "sourceSeqWritten"
    Assert-True ($sent -eq $accepted -and $accepted -eq $captured -and $captured -eq $executed -and $executed -eq $output -and $output -eq $udpFrames -and $udpFrames -eq $received -and $received -eq $writtenMax -and $writtenMax -eq $mp4Frames) "$($Spec.name): sent/accepted/captured/executed/output/received/written/mp4 conservation failed ($sent/$accepted/$captured/$executed/$output/$udpFrames/$received/$writtenMax/$mp4Frames)"

    return [pscustomobject]@{
        ordinal = $Spec.ordinal; name = $Spec.name; protocolBand = $Spec.band; band = $Spec.bandName
        sent = $sent; accepted = $accepted; captured = $captured; executed = $executed; output = $output
        received = $received; written = $writtenMax; mp4Frames = $mp4Frames
        sampledPerfUdpFrames = $sampledUdpFrames; sampledPerfRenderFrames = $sampledExecuted; sampledPerfOutputFrames = $sampledOutput
        firstSourceSeq = [int64]$index[0].sourceSeq; lastSourceSeq = [int64]$index[-1].sourceSeq
        profileRefresh = $true; m1Refresh = $true; modtranRefresh = $true; shaderRefresh = $true
        previousBandCacheMarker = $false; orderedQueue = $true; syncMode = $true
        overflow = 0; overwrite = 0; drop = 0
    }
}

function Stop-OwnedProcess {
    param([System.Diagnostics.Process]$Process)
    if ($null -eq $Process) { return }
    try { $Process.Refresh() } catch { return }
    if ($Process.HasExited) { return }
    Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
    try { [void]$Process.WaitForExit(5000) } catch { }
}

$preflight = Get-Preflight
if ($Mode -eq "Check") {
    $preflight | ConvertTo-Json -Depth 8
    return
}

Assert-True $preflight.readyForRun "preflight is not ready; run -Mode Check and rebuild stale binaries"
$conflicts = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.ProcessName -in @("HwaSim_IR", "HwaSim_IR_VideoDisplay", "DataDrivenTestQT")
})
Assert-True ($conflicts.Count -eq 0) "existing Hwa/Video/Stim process would conflict with UDP/TCP ports"

if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $repo ("logs\p11-band-switch-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
} elseif (-not [IO.Path]::IsPathRooted($OutputRoot)) {
    $OutputRoot = Join-Path $repo $OutputRoot
}
Assert-True (-not (Test-Path -LiteralPath $OutputRoot)) "output directory already exists: $OutputRoot"
[void](New-Item -ItemType Directory -Path $OutputRoot)
$backupRoot = Join-Path $OutputRoot "config_backup"
[void](New-Item -ItemType Directory -Path $backupRoot)

$configPaths = @($runtimeIni, $hwaNetworkIni, $videoNetworkIni, $stimNetworkIni)
$configBackup = @{}
$configBefore = @()
foreach ($path in $configPaths) {
    $exists = Test-Path -LiteralPath $path
    $bytes = if ($exists) { [IO.File]::ReadAllBytes($path) } else { $null }
    $configBackup[$path] = $bytes
    $configBefore += [pscustomobject]@{ path = $path; existed = $exists; sha256 = Get-Sha256OrMissing $path }
    if ($exists) { [IO.File]::WriteAllBytes((Join-Path $backupRoot ((Split-Path -Leaf $path) + "." + $configBefore.Count + ".bak")), $bytes) }
}

$utf8NoBom = New-Object Text.UTF8Encoding($false)
$oldQtForceStderr = $env:QT_FORCE_STDERR_LOGGING
$oldWeatherCameraInput = $env:WeatherCameraInput
$oldWeatherStartSec = $env:WeatherStartSec
$oldP6TestTargetType = $env:P6TestTargetType
$oldRenderPresentationMode = $env:RenderPresentationMode
$oldPathValue = [Environment]::GetEnvironmentVariable("Path", "Process")
$oldPathUpperValue = [Environment]::GetEnvironmentVariable("PATH", "Process")
$hwa = $null
$video = $null
$activeStim = $null
$runError = $null
$roundResults = @()
$aggregateHwaOut = Join-Path $OutputRoot "hwa.aggregate.out.log"
$aggregateHwaErr = Join-Path $OutputRoot "hwa.aggregate.err.log"
$aggregateVideoOut = Join-Path $OutputRoot "video.aggregate.out.log"
$aggregateVideoErr = Join-Path $OutputRoot "video.aggregate.err.log"

try {
    # Windows environment keys are case-insensitive, while Start-Process builds
    # a case-sensitive dictionary.  Codex sessions may expose both PATH and Path;
    # normalize them before launching the three production processes.
    $pathValue = $oldPathValue
    if ([string]::IsNullOrEmpty($pathValue)) { $pathValue = $oldPathUpperValue }
    if (-not [string]::IsNullOrEmpty($pathValue)) {
        [Environment]::SetEnvironmentVariable("PATH", $null, "Process")
        [Environment]::SetEnvironmentVariable("Path", $pathValue, "Process")
    }
    [IO.File]::WriteAllText($hwaNetworkIni,
        "[Identity]`r`nchannel=precise`r`nplatID=1001`r`nsensorID=2`r`nacceptSensorBroadcast=true`r`nallowDynamicRemote=false`r`n`r`n[UDP]`r`nlocalIp=127.0.0.1`r`nlocalPort=8888`r`nremoteIp=127.0.0.1`r`nremotePort=9999`r`n`r`n[TCP]`r`nserverIp=127.0.0.1`r`nserverPort=5555`r`n", $utf8NoBom)
    [IO.File]::WriteAllText($videoNetworkIni,
        "[Identity]`r`nchannel=precise`r`nplatID=1001`r`nsensorID=2`r`n`r`n[VideoInput]`r`nTransport=tcp`r`n`r`n[Network]`r`nip=127.0.0.1`r`nport=5555`r`n`r`n[Recorder]`r`nMaxRecordingQueueFrames=256`r`nFlushTimeoutMs=15000`r`n", $utf8NoBom)
    [IO.File]::WriteAllText($stimNetworkIni,
        "[Identity]`r`nchannel=precise`r`nplatID=1001`r`nsensorID=2`r`n`r`n[RenderControl]`r`nsimMode=1`r`nvideoFps=60`r`nsendStepMs=16.666666666666668`r`n`r`n[UDP]`r`nlocalIp=127.0.0.1`r`nlocalPort=9999`r`nremoteIp=127.0.0.1`r`nremotePort=8888`r`n", $utf8NoBom)

    $runtimeText = [IO.File]::ReadAllText($runtimeIni)
    foreach ($setting in @(
        @("RenderControl", "ModePolicy", "ExternalPreferred"), @("RenderControl", "ConfiguredSimMode", "1"),
        @("RenderControl", "ConfiguredVideoFps", "60"), @("RenderControl", "AsyncInputPolicy", "OrderedQueue"),
        @("RenderControl", "AsyncInputQueueMaxFrames", "256"), @("RenderControl", "AsyncInputBackpressureMaxWaitMs", "1000"),
        @("Performance", "EnablePerfLog", "true"), @("Performance", "EnableIRVerboseLog", "false"),
        @("Performance", "QuietPerfMode", "false"), @("Stage5Radiance", "EnableIRPhysicalPipeline", "true"),
        @("Stage5Radiance", "DebugView", "Off"), @("Stage5Radiance", "LogComponents", "true"),
        @("Stage5Radiance", "ComponentLogEveryFrames", "1"), @("Stage5ModtranRadiance", "EnableModtranRadianceDebug", "true"),
        @("Stage5ModtranRadiance", "PreferredSource", "band_lut"), @("Stage5ModtranRadiance", "CompareLegacy", "true"),
        @("Stage5ModtranRadiance", "LogEveryFrames", "1"), @("M1NirMwirPhysics", "CompareOnly", "false"),
        @("M1NirMwirPhysics", "EnableRuntime", "true"), @("M1NirMwirPhysics", "EnableNIRRuntime", "true"),
        @("M1NirMwirPhysics", "EnableSWIRRuntime", "true"), @("M1NirMwirPhysics", "EnableMWIRRuntime", "true"),
        @("M1NirMwirPhysics", "ResponseMode", "RectangularBand"), @("CommandTransport", "Input", "udp"),
        @("CommandTransport", "Ack", "match_input"), @("DdsProtocol", "Enable", "false"),
        @("DdsVideo", "Enable", "false"), @("TcpOutput", "Codec", "jpeg"),
        @("TcpOutput", "JpegQuality", "100"), @("TcpPayload", "PacketVersion", "3"),
        @("TcpPayload", "SendVideo", "true"), @("TcpPayload", "SendAnnotation", "true"),
        @("TcpPayload", "SendRealtimeData", "true"), @("TcpPayload", "ForwardInitControl", "true"),
        @("Debug", "ExitOnStop", "0")
    )) { $runtimeText = Set-IniSectionValue $runtimeText $setting[0] $setting[1] $setting[2] }
    [IO.File]::WriteAllText($runtimeIni, $runtimeText, $utf8NoBom)

    $env:QT_FORCE_STDERR_LOGGING = "1"
    # The legacy row stream alone describes an F35 at 10 km, outside the P11
    # SWIR near-ground grid.  These documented, process-scoped hooks select the
    # CC0 civil van and its controlled 1 km atmosphere state without changing
    # any wire-layout field.
    $env:WeatherCameraInput = $bandSwitchFixture
    $env:WeatherStartSec = "0"
    $env:P6TestTargetType = "0x55"
    $env:RenderPresentationMode = "HeadlessOffscreen"
    $video = Start-Process -FilePath $videoExe -ArgumentList @("--receive-transport=tcp") -WorkingDirectory $videoWork -WindowStyle Hidden -PassThru -RedirectStandardOutput $aggregateVideoOut -RedirectStandardError $aggregateVideoErr
    Start-Sleep -Seconds 2
    Assert-True (-not $video.HasExited) "VideoDisplay exited during startup"
    $hwa = Start-Process -FilePath $hwaExe -ArgumentList @("--network-config", $hwaNetworkIni) -WorkingDirectory $hwaWork -WindowStyle Hidden -PassThru -RedirectStandardOutput $aggregateHwaOut -RedirectStandardError $aggregateHwaErr
    Start-Sleep -Seconds 5
    Assert-True (-not $hwa.HasExited) "HwaSim_IR exited during startup"
    $sharedPids = [pscustomobject]@{ hwa = $hwa.Id; video = $video.Id }

    foreach ($spec in $roundPlan) {
        $roundDir = Join-Path $OutputRoot $spec.name
        [void](New-Item -ItemType Directory -Path $roundDir)
        $hwaOutOffset = Get-SharedFileLength $aggregateHwaOut
        $hwaErrOffset = Get-SharedFileLength $aggregateHwaErr
        $videoOutOffset = Get-SharedFileLength $aggregateVideoOut
        $videoErrOffset = Get-SharedFileLength $aggregateVideoErr
        $recordingsBefore = if (Test-Path -LiteralPath $mp4Root) { @(Get-ChildItem -LiteralPath $mp4Root -Directory | ForEach-Object { $_.FullName }) } else { @() }

        $stimOut = Join-Path $roundDir "stim.out.log"
        $stimErr = Join-Path $roundDir "stim.err.log"
        $stimArgs = @("--phase1b-auto-seconds=$Seconds", "--control-transport=udp", "--phase1d-h264=0", "--save-mp4=1", "--sim-mode=1", "--video-fps=60", "--sensor-band=$($spec.band)", "--freeze-geometry")
        $stimArgs += "--input-file=$resolvedInputFile"
        $activeStim = Start-Process -FilePath $stimExe -ArgumentList $stimArgs -WorkingDirectory $stimWork -WindowStyle Hidden -PassThru -RedirectStandardOutput $stimOut -RedirectStandardError $stimErr
        Assert-True ($activeStim.WaitForExit(($Seconds + 35) * 1000)) "$($spec.name): stimulus timeout"
        # Complete the redirected-stream drain before reading ExitCode.  Without
        # the parameterless wait PowerShell can expose a null ExitCode even
        # though the child has already written StimFinal and exited normally.
        $activeStim.WaitForExit()
        $activeStim.Refresh()
        $stimExitCode = $activeStim.ExitCode
        if ($null -ne $stimExitCode) {
            Assert-True ($stimExitCode -eq 0) "$($spec.name): stimulus exit code $stimExitCode"
        } else {
            # Windows PowerShell 5.1 can leave ExitCode unset for a redirected
            # Qt child.  In that case require the child's own terminal marker;
            # a timeout/crash cannot emit this after the timed send loop.
            $stimCompletionText = [IO.File]::ReadAllText($stimOut) + "`n" + [IO.File]::ReadAllText($stimErr)
            Assert-True ($stimCompletionText.Contains("[StimFinal]")) "$($spec.name): stimulus ExitCode unavailable and StimFinal missing"
        }
        $activeStim = $null
        Wait-SliceContains $aggregateHwaOut $hwaOutOffset "[SyncRoundConservation]" 25
        $recordingSource = Wait-NewRecordingDirectory $recordingsBefore 25
        Start-Sleep -Seconds 1
        Assert-True (-not $hwa.HasExited -and $hwa.Id -eq $sharedPids.hwa) "$($spec.name): Hwa process was not preserved"
        Assert-True (-not $video.HasExited -and $video.Id -eq $sharedPids.video) "$($spec.name): Video process was not preserved"

        $hwaOutBytes = Read-SharedFileSlice $aggregateHwaOut $hwaOutOffset
        $hwaErrBytes = Read-SharedFileSlice $aggregateHwaErr $hwaErrOffset
        $videoOutBytes = Read-SharedFileSlice $aggregateVideoOut $videoOutOffset
        $videoErrBytes = Read-SharedFileSlice $aggregateVideoErr $videoErrOffset
        [IO.File]::WriteAllBytes((Join-Path $roundDir "hwa.out.log"), $hwaOutBytes)
        [IO.File]::WriteAllBytes((Join-Path $roundDir "hwa.err.log"), $hwaErrBytes)
        [IO.File]::WriteAllBytes((Join-Path $roundDir "video.out.log"), $videoOutBytes)
        [IO.File]::WriteAllBytes((Join-Path $roundDir "video.err.log"), $videoErrBytes)
        $recordingCopy = Join-Path $roundDir "receiver_recording"
        Copy-RecordingDirectory $recordingSource.FullName $recordingCopy

        $mp4 = Join-Path $recordingCopy "output.mp4"
        $firstPng = Join-Path $roundDir "received_first.png"
        $middlePng = Join-Path $roundDir "received_middle.png"
        Invoke-ExternalChecked $preflight.ffmpeg @("-hide_banner", "-loglevel", "error", "-y", "-i", $mp4, "-frames:v", "1", $firstPng) "$($spec.name) first PNG decode"
        $seek = [string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:R}", $Seconds / 2.0)
        Invoke-ExternalChecked $preflight.ffmpeg @("-hide_banner", "-loglevel", "error", "-y", "-ss", $seek, "-i", $mp4, "-frames:v", "1", $middlePng) "$($spec.name) middle PNG decode"
        Assert-True ((Test-Path -LiteralPath $firstPng) -and (Get-Item -LiteralPath $firstPng).Length -gt 0) "$($spec.name): first PNG missing"
        Assert-True ((Test-Path -LiteralPath $middlePng) -and (Get-Item -LiteralPath $middlePng).Length -gt 0) "$($spec.name): middle PNG missing"

        $stimText = [IO.File]::ReadAllText($stimErr)
        $hwaText = [Text.Encoding]::UTF8.GetString($hwaOutBytes)
        $videoText = [Text.Encoding]::UTF8.GetString($videoErrBytes)
        $roundResult = Test-RoundEvidence $spec $stimText $hwaText $videoText $recordingCopy $preflight.ffprobe
        $roundResult | Add-Member -NotePropertyName hwaPid -NotePropertyValue $sharedPids.hwa
        $roundResult | Add-Member -NotePropertyName videoPid -NotePropertyValue $sharedPids.video
        $roundResult | Add-Member -NotePropertyName recordingSource -NotePropertyValue $recordingSource.FullName
        $roundResult | Add-Member -NotePropertyName artifacts -NotePropertyValue ([pscustomobject]@{
            recording = $recordingCopy
            mp4 = $mp4; mp4Sha256 = Get-Sha256OrMissing $mp4
            firstPng = $firstPng; firstPngSha256 = Get-Sha256OrMissing $firstPng
            middlePng = $middlePng; middlePngSha256 = Get-Sha256OrMissing $middlePng
            stimLogSha256 = Get-Sha256OrMissing $stimErr
            hwaLogSha256 = Get-Sha256OrMissing (Join-Path $roundDir "hwa.out.log")
            videoLogSha256 = Get-Sha256OrMissing (Join-Path $roundDir "video.err.log")
        })
        $roundResult | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $roundDir "round_gate.json") -Encoding UTF8
        $roundResults += $roundResult
    }

    Assert-True ((@($roundResults | Select-Object -ExpandProperty protocolBand) -join ",") -eq "0,2,0") "cross-round band order is not 0,2,0"
    Assert-True (@($roundResults | Select-Object -ExpandProperty hwaPid -Unique).Count -eq 1) "Hwa PID changed across rounds"
    Assert-True (@($roundResults | Select-Object -ExpandProperty videoPid -Unique).Count -eq 1) "Video PID changed across rounds"
    Assert-True ($roundResults[0].artifacts.firstPngSha256 -ne $roundResults[1].artifacts.firstPngSha256) "SWIR-to-MWIR first received frame did not change"
    Assert-True ($roundResults[1].artifacts.firstPngSha256 -ne $roundResults[2].artifacts.firstPngSha256) "MWIR-to-SWIR first received frame did not change"

    $rejectRoot = Join-Path $OutputRoot "unsupported_band_rejections"
    [void](New-Item -ItemType Directory -Path $rejectRoot)
    $rejectResults = @()
    foreach ($value in @("3", "4", "VIS-SWIR")) {
        $safeName = $value.Replace("-", "_")
        $outPath = Join-Path $rejectRoot ("sensor_band_" + $safeName + ".out.log")
        $errPath = Join-Path $rejectRoot ("sensor_band_" + $safeName + ".err.log")
        # PowerShell 5.1 Start-Process may leave ExitCode null for a redirected
        # GUI-subsystem Qt executable.  Use System.Diagnostics.Process directly
        # so the explicit EX_USAGE=64 contract is actually measured.
        $rejectInfo = New-Object System.Diagnostics.ProcessStartInfo
        $rejectInfo.FileName = $stimExe
        $rejectInfo.Arguments = "--sensor-band=$value"
        $rejectInfo.WorkingDirectory = $stimWork
        $rejectInfo.UseShellExecute = $false
        $rejectInfo.CreateNoWindow = $true
        $rejectInfo.RedirectStandardOutput = $true
        $rejectInfo.RedirectStandardError = $true
        $reject = New-Object System.Diagnostics.Process
        $reject.StartInfo = $rejectInfo
        Assert-True ($reject.Start()) "invalid band $value process did not start"
        $rejectStdout = $reject.StandardOutput.ReadToEndAsync()
        $rejectStderr = $reject.StandardError.ReadToEndAsync()
        Assert-True ($reject.WaitForExit(10000)) "invalid band $value did not exit promptly"
        $reject.WaitForExit()
        [IO.File]::WriteAllText($outPath, $rejectStdout.Result, $utf8NoBom)
        [IO.File]::WriteAllText($errPath, $rejectStderr.Result, $utf8NoBom)
        $rejectExitCode = $reject.ExitCode
        Assert-True ($rejectExitCode -eq 64) "invalid band $value exit code was $rejectExitCode, expected 64"
        $rejectText = ([IO.File]::ReadAllText($outPath) + "`n" + [IO.File]::ReadAllText($errPath))
        Assert-True ($rejectText.Contains("production supports only 0=SWIR, 1=NIR, 2=MWIR")) "invalid band $value lacks explicit support-set evidence"
        Assert-True ($rejectText.Contains("VIS-SWIR are unsupported")) "invalid band $value lacks VIS-SWIR rejection evidence"
        $rejectResults += [pscustomobject]@{ value = $value; exitCode = $rejectExitCode; rejected = $true; stdout = $outPath; stderr = $errPath }
    }

    $manifest = [pscustomobject]@{
        schema = "HwaSimIR.P11.BandSwitchReinit.v1"
        generatedUtc = [datetime]::UtcNow.ToString("o")
        outputRoot = $OutputRoot
        lifecycle = "same HwaSim_IR + same VideoDisplay; DataDriven auto RESET/INIT/ACK/START/STOP per segment"
        processIdentity = $sharedPids
        forcedTransport = [pscustomobject]@{ command = "udp"; video = "tcp"; simMode = 1; inputPolicy = "OrderedQueue" }
        inputFixture = [pscustomobject]@{
            rowSource = $resolvedInputFile; rowSourceSha256 = Get-Sha256OrMissing $resolvedInputFile
            controlledState = $bandSwitchFixture; controlledStateSha256 = Get-Sha256OrMissing $bandSwitchFixture
        }
        binaries = $preflight.binaryFreshness
        configBefore = $configBefore
        rounds = $roundResults
        unsupportedBandRejections = $rejectResults
        pass = $true
    }
    $manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $OutputRoot "p11_band_switch_reinit_summary.json") -Encoding UTF8
} catch {
    $runError = $_
    throw
} finally {
    Stop-OwnedProcess $activeStim
    Stop-OwnedProcess $hwa
    Stop-OwnedProcess $video
    $restoreRows = @()
    foreach ($path in $configPaths) {
        $before = @($configBefore | Where-Object { $_.path -eq $path })[0]
        if ($before.existed) {
            [IO.File]::WriteAllBytes($path, [byte[]]($configBackup[$path]))
        } elseif (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Force
        }
        $afterHash = Get-Sha256OrMissing $path
        $restoreRows += [pscustomobject]@{ path = $path; beforeSha256 = $before.sha256; afterSha256 = $afterHash; restored = ($before.sha256 -eq $afterHash) }
    }
    $env:QT_FORCE_STDERR_LOGGING = $oldQtForceStderr
    $env:WeatherCameraInput = $oldWeatherCameraInput
    $env:WeatherStartSec = $oldWeatherStartSec
    $env:P6TestTargetType = $oldP6TestTargetType
    $env:RenderPresentationMode = $oldRenderPresentationMode
    [Environment]::SetEnvironmentVariable("Path", $oldPathValue, "Process")
    if ($oldPathUpperValue -ne $oldPathValue) {
        [Environment]::SetEnvironmentVariable("PATH", $oldPathUpperValue, "Process")
    }
    $restoreEvidence = [pscustomobject]@{
        restoredUtc = [datetime]::UtcNow.ToString("o")
        allRestored = @($restoreRows | Where-Object { -not $_.restored }).Count -eq 0
        files = $restoreRows
        runFailed = ($null -ne $runError)
        failure = if ($null -ne $runError) { $runError.Exception.Message } else { "" }
    }
    $restoreEvidence | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputRoot "config_restore_evidence.json") -Encoding UTF8
}

Write-Output ("PASS summary=" + (Join-Path $OutputRoot "p11_band_switch_reinit_summary.json"))
