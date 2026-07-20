param(
    [ValidateSet(
        "precise_visible_sync_jpeg",
        "precise_visible_async60_jpeg",
        "coarse_visible_async60_jpeg",
        "dual_visible_async60_jpeg",
        "dual_headless_async60_jpeg")]
    [string[]]$Cases = @(
        "precise_visible_sync_jpeg",
        "precise_visible_async60_jpeg",
        "coarse_visible_async60_jpeg",
        "dual_visible_async60_jpeg",
        "dual_headless_async60_jpeg"),
    [int]$WarmupSeconds = 5,
    [int]$MeasureSeconds = 30,
    [int]$HwaStartupSeconds = 8
)

$ErrorActionPreference = "Stop"
$WarmupSeconds = [Math]::Max(5, $WarmupSeconds)
$MeasureSeconds = [Math]::Max(30, $MeasureSeconds)
$HwaStartupSeconds = [Math]::Max(5, $HwaStartupSeconds)

$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$hwaExe = Join-Path $root "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $root "HwaSim_IR\Bin"
$stimExe = Join-Path $root "build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe"
$stimWorkDir = Join-Path $root "DataDrivenTestQT"
$videoExe = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe"
$videoWorkDir = Split-Path -Parent $videoExe
$gxx = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
$senderSource = Join-Path $root "tools\r1_protocol_sender.cpp"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runDir = Join-Path $root "logs\r2-60fps-$stamp"
$senderExe = Join-Path $runDir "r1_protocol_sender.exe"
$utf8 = New-Object Text.UTF8Encoding($false)
$invariant = [Globalization.CultureInfo]::InvariantCulture

# Codex-launched Windows shells can contain both Path and PATH. Windows
# PowerShell 5 Start-Process treats those case-insensitive keys as duplicates.
$processEnvironment = [Environment]::GetEnvironmentVariables("Process")
$pathEntries = @($processEnvironment.GetEnumerator() | Where-Object { $_.Key -imatch '^path$' })
if ($pathEntries.Count -gt 1) {
    $pathValue = ($pathEntries | Where-Object { $_.Key -ceq "Path" } | Select-Object -First 1).Value
    if ([string]::IsNullOrEmpty($pathValue)) { $pathValue = $pathEntries[0].Value }
    [Environment]::SetEnvironmentVariable("PATH", $null, "Process")
    [Environment]::SetEnvironmentVariable("Path", [string]$pathValue, "Process")
}

foreach ($required in @($hwaExe, $stimExe, $videoExe, $gxx, $senderSource)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required path not found: $required"
    }
}
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$hwaSha256 = (Get-FileHash -LiteralPath $hwaExe -Algorithm SHA256).Hash

$productionConfigs = @(
    (Join-Path $root "HwaSim_IR\Bin\Config\NetworkConfig_precise.ini"),
    (Join-Path $root "HwaSim_IR\Bin\Config\NetworkConfig_coarse.ini")
)
$productionHashes = @{}
foreach ($path in $productionConfigs) {
    if (Test-Path -LiteralPath $path) {
        $productionHashes[$path] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
}

& $gxx -std=c++11 -O2 -I $root $senderSource -lws2_32 -o $senderExe
if ($LASTEXITCODE -ne 0) {
    throw "r1_protocol_sender build failed: $LASTEXITCODE"
}

function Get-ChannelSensorID {
    param([string]$Channel)
    if ($Channel -eq "precise") { return 2 }
    return 1
}

function Get-WrongSensorID {
    param([string]$Channel)
    if ($Channel -eq "precise") { return 1 }
    return 2
}

function Write-HwaConfig {
    param(
        [string]$Path,
        [string]$Channel,
        [int]$SensorID,
        [int]$UdpLocalPort,
        [int]$UdpRemotePort,
        [int]$TcpPort
    )
    $content = @"
[Identity]
channel=$Channel
platID=1001
sensorID=$SensorID
acceptSensorBroadcast=true
allowDynamicRemote=false

[UDP]
localIp=127.0.0.1
localPort=$UdpLocalPort
remoteIp=127.0.0.1
remotePort=$UdpRemotePort

[TCP]
serverIp=127.0.0.1
serverPort=$TcpPort
"@
    [IO.File]::WriteAllText($Path, $content, $utf8)
}

function Write-StimConfig {
    param(
        [string]$Path,
        [string]$Channel,
        [int]$SensorID,
        [int]$SimMode,
        [int]$UdpLocalPort,
        [int]$UdpRemotePort
    )
    $content = @"
[Identity]
channel=$Channel
platID=1001
sensorID=$SensorID

[RenderControl]
simMode=$SimMode
videoFps=60

[UDP]
localIp=127.0.0.1
localPort=$UdpLocalPort
remoteIp=127.0.0.1
remotePort=$UdpRemotePort
"@
    [IO.File]::WriteAllText($Path, $content, $utf8)
}

function Write-VideoConfig {
    param(
        [string]$Path,
        [string]$Channel,
        [int]$SensorID,
        [int]$TcpPort
    )
    $content = @"
[Identity]
channel=$Channel
platID=1001
sensorID=$SensorID

[Network]
ip=127.0.0.1
port=$TcpPort

[Recorder]
MaxRecordingQueueFrames=180
FlushTimeoutMs=10000
"@
    [IO.File]::WriteAllText($Path, $content, $utf8)
}

function Start-LoggedProcess {
    param(
        [string]$Component,
        [string]$Channel,
        [string]$FilePath,
        [string[]]$Arguments,
        [string]$WorkingDirectory,
        [string]$CaseDirectory
    )
    $prefix = "$Channel.$Component"
    $process = Start-Process -FilePath $FilePath `
        -ArgumentList $Arguments `
        -WorkingDirectory $WorkingDirectory `
        -WindowStyle Hidden `
        -PassThru `
        -RedirectStandardOutput (Join-Path $CaseDirectory "$prefix.out.log") `
        -RedirectStandardError (Join-Path $CaseDirectory "$prefix.err.log")
    return [pscustomobject]@{
        Component = $Component
        Channel = $Channel
        Process = $process
        Stdout = Join-Path $CaseDirectory "$prefix.out.log"
        Stderr = Join-Path $CaseDirectory "$prefix.err.log"
    }
}

function Stop-LoggedProcesses {
    param([object[]]$Entries)
    foreach ($entry in @($Entries)) {
        if ($null -eq $entry -or $null -eq $entry.Process) { continue }
        $process = $entry.Process
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
        }
        $process.WaitForExit()
    }
}

function Assert-ProcessesAlive {
    param([object[]]$Entries, [string]$Stage)
    foreach ($entry in @($Entries)) {
        $entry.Process.Refresh()
        if ($entry.Process.HasExited) {
            throw "$($entry.Component)/$($entry.Channel) exited during $Stage with code $($entry.Process.ExitCode)"
        }
    }
}

function Get-LogOffsets {
    param([object[]]$Entries)
    $offsets = @{}
    foreach ($entry in @($Entries)) {
        foreach ($path in @($entry.Stdout, $entry.Stderr)) {
            $offsets[$path] = if (Test-Path -LiteralPath $path) {
                (Get-Item -LiteralPath $path).Length
            }
            else { 0L }
        }
    }
    return $offsets
}

function Write-FormalTails {
    param([hashtable]$Offsets)
    $formalPaths = @{}
    foreach ($path in $Offsets.Keys) {
        $formalPath = "$path.formal.log"
        $stream = New-Object IO.FileStream(
            $path,
            [IO.FileMode]::Open,
            [IO.FileAccess]::Read,
            [IO.FileShare]::ReadWrite)
        try {
            [void]$stream.Seek([int64]$Offsets[$path], [IO.SeekOrigin]::Begin)
            $reader = New-Object IO.StreamReader($stream, [Text.Encoding]::UTF8, $true, 4096, $true)
            try { $tail = $reader.ReadToEnd() }
            finally { $reader.Dispose() }
        }
        finally { $stream.Dispose() }
        [IO.File]::WriteAllText($formalPath, $tail, $utf8)
        $formalPaths[$path] = $formalPath
    }
    return $formalPaths
}

function Convert-TaggedRows {
    param([string]$Path, [string]$Tag)
    $rows = @()
    if (-not (Test-Path -LiteralPath $Path)) { return $rows }
    foreach ($line in [IO.File]::ReadAllLines($Path)) {
        if (-not $line.Contains("[$Tag]")) { continue }
        if ($Tag -eq "Perf" -and $line.Contains("[Perf][WARN]")) { continue }
        $fields = @{}
        foreach ($match in [regex]::Matches($line, '(?<key>[A-Za-z][A-Za-z0-9]*)=(?<value>[^\s]+)')) {
            $fields[$match.Groups['key'].Value] = $match.Groups['value'].Value
        }
        if ($fields.Count -gt 0) { $rows += ,$fields }
    }
    return $rows
}

function Convert-ToDouble {
    param([object]$Value)
    $parsed = 0.0
    if ($null -ne $Value -and [double]::TryParse(
        [string]$Value,
        [Globalization.NumberStyles]::Float,
        $invariant,
        [ref]$parsed)) {
        return $parsed
    }
    return $null
}

function Get-Values {
    param([object[]]$Rows, [string]$Key)
    $values = @()
    foreach ($row in @($Rows)) {
        if ($row.ContainsKey($Key)) {
            $value = Convert-ToDouble $row[$Key]
            if ($null -ne $value) { $values += $value }
        }
    }
    return $values
}

function Get-Average {
    param([double[]]$Values)
    if (@($Values).Count -eq 0) { return $null }
    return ($Values | Measure-Object -Average).Average
}

function Get-Maximum {
    param([double[]]$Values)
    if (@($Values).Count -eq 0) { return $null }
    return ($Values | Measure-Object -Maximum).Maximum
}

function Get-LastValue {
    param([double[]]$Values)
    if (@($Values).Count -eq 0) { return $null }
    return $Values[@($Values).Count - 1]
}

function Test-SustainedGrowth {
    param([double[]]$Values)
    $count = @($Values).Count
    if ($count -lt 6) { return $false }
    $span = [Math]::Max(2, [Math]::Floor($count / 3))
    $first = Get-Average @($Values[0..($span - 1)])
    $last = Get-Average @($Values[($count - $span)..($count - 1)])
    return $last -gt 2.0 -and $last -gt ($first + 2.0)
}

function Test-Context {
    param([object[]]$Rows, [string]$Channel, [int]$SensorID)
    if (@($Rows).Count -eq 0) { return $false }
    foreach ($row in @($Rows)) {
        if (-not $row.ContainsKey("channel") -or $row["channel"] -ne $Channel) { return $false }
        if (-not $row.ContainsKey("platID") -or $row["platID"] -ne "1001") { return $false }
        if (-not $row.ContainsKey("sensorID") -or $row["sensorID"] -ne [string]$SensorID) { return $false }
        if (-not $row.ContainsKey("pid") -or [int64]$row["pid"] -le 0) { return $false }
    }
    return $true
}

function Format-Number {
    param([object]$Value)
    if ($null -eq $Value) { return "n/a" }
    return ([double]$Value).ToString("0.000", $invariant)
}

$caseCatalog = @{
    precise_visible_sync_jpeg = [pscustomobject]@{
        Name = "precise_visible_sync_jpeg"; Channels = @("precise"); SimMode = 1; Presentation = "VisibleWindow"
    }
    precise_visible_async60_jpeg = [pscustomobject]@{
        Name = "precise_visible_async60_jpeg"; Channels = @("precise"); SimMode = 2; Presentation = "VisibleWindow"
    }
    coarse_visible_async60_jpeg = [pscustomobject]@{
        Name = "coarse_visible_async60_jpeg"; Channels = @("coarse"); SimMode = 2; Presentation = "VisibleWindow"
    }
    dual_visible_async60_jpeg = [pscustomobject]@{
        Name = "dual_visible_async60_jpeg"; Channels = @("precise", "coarse"); SimMode = 2; Presentation = "VisibleWindow"
    }
    dual_headless_async60_jpeg = [pscustomobject]@{
        Name = "dual_headless_async60_jpeg"; Channels = @("precise", "coarse"); SimMode = 2; Presentation = "HeadlessOffscreen"
    }
}

$oldEnvironment = @{}
foreach ($name in @("RenderPresentationMode", "RenderWindowPreview", "EnablePerfLog", "RenderPerfProbe", "QT_FORCE_STDERR_LOGGING")) {
    $oldEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
}

$allResults = @()
$caseResults = @()
$caseIndex = 0
try {
    $env:EnablePerfLog = "1"
    $env:RenderPerfProbe = "1"
    $env:QT_FORCE_STDERR_LOGGING = "1"

    foreach ($caseName in $Cases) {
        ++$caseIndex
        $case = $caseCatalog[$caseName]
        $caseDir = Join-Path $runDir $case.Name
        New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
        $env:RenderPresentationMode = $case.Presentation
        $env:RenderWindowPreview = if ($case.Presentation -eq "VisibleWindow") { "1" } else { "0" }
        $basePort = 30000 + (($caseIndex - 1) * 500) + (Get-Random -Minimum 0 -Maximum 100)
        $entries = @()
        $channelConfigs = @{}
        $caseError = $null
        try {
            $channelIndex = 0
            foreach ($channel in @($case.Channels)) {
                $sensorID = Get-ChannelSensorID $channel
                $udpPort = $basePort + $channelIndex
                $stimPort = $basePort + 100 + $channelIndex
                $tcpPort = $basePort + 200 + $channelIndex
                $hwaConfig = Join-Path $caseDir "$channel.hwa.ini"
                $stimConfig = Join-Path $caseDir "$channel.stim.ini"
                $videoConfig = Join-Path $caseDir "$channel.video.ini"
                Write-HwaConfig $hwaConfig $channel $sensorID $udpPort $stimPort $tcpPort
                Write-StimConfig $stimConfig $channel $sensorID $case.SimMode $stimPort $udpPort
                Write-VideoConfig $videoConfig $channel $sensorID $tcpPort
                $channelConfigs[$channel] = [pscustomobject]@{
                    SensorID = $sensorID; UdpPort = $udpPort; StimPort = $stimPort; TcpPort = $tcpPort
                    Hwa = $hwaConfig; Stim = $stimConfig; Video = $videoConfig
                }
                ++$channelIndex
            }

            foreach ($channel in @($case.Channels)) {
                $cfg = $channelConfigs[$channel]
                $entries += Start-LoggedProcess "video" $channel $videoExe @(
                    "--network-config=$($cfg.Video)",
                    "--channel=$channel",
                    "--plat-id=1001",
                    "--sensor-id=$($cfg.SensorID)") $videoWorkDir $caseDir
            }
            Start-Sleep -Seconds 2
            foreach ($channel in @($case.Channels)) {
                $cfg = $channelConfigs[$channel]
                $entries += Start-LoggedProcess "hwa" $channel $hwaExe @(
                    "--network-config", $cfg.Hwa) $hwaWorkDir $caseDir
            }
            Start-Sleep -Seconds $HwaStartupSeconds
            Assert-ProcessesAlive $entries "Hwa startup"

            $stimRunSeconds = 6 + $WarmupSeconds + $MeasureSeconds + 20
            foreach ($channel in @($case.Channels)) {
                $cfg = $channelConfigs[$channel]
                $entries += Start-LoggedProcess "stim" $channel $stimExe @(
                    "--network-config=$($cfg.Stim)",
                    "--channel=$channel",
                    "--plat-id=1001",
                    "--sensor-id=$($cfg.SensorID)",
                    "--sim-mode=$($case.SimMode)",
                    "--video-fps=60",
                    "--phase1d-h264=0",
                    "--save-mp4=0",
                    "--phase1b-auto-seconds=$stimRunSeconds") $stimWorkDir $caseDir
            }

            # DataDrivenTestQT sends Init after 0.5 s and starts its 60 Hz stream after 6 s.
            Start-Sleep -Milliseconds 6500
            Assert-ProcessesAlive $entries "stimulus startup"
            Start-Sleep -Seconds $WarmupSeconds
            # Allow one complete two-second metrics interval after warmup before taking byte offsets.
            Start-Sleep -Seconds 3
            $offsets = Get-LogOffsets $entries

            foreach ($channel in @($case.Channels)) {
                $cfg = $channelConfigs[$channel]
                $wrongSensorID = Get-WrongSensorID $channel
                $routeProbe = & $senderExe `
                    "--type=display" `
                    "--port=$($cfg.UdpPort)" `
                    "--plat-id=1001" `
                    "--sensor-id=$wrongSensorID" 2>&1
                if ($LASTEXITCODE -ne 0) {
                    throw "Wrong-sensor route probe failed for $channel`: $routeProbe"
                }
                [IO.File]::AppendAllLines(
                    (Join-Path $caseDir "$channel.route-probe.log"),
                    [string[]]$routeProbe,
                    $utf8)
            }

            Start-Sleep -Seconds $MeasureSeconds
            Assert-ProcessesAlive $entries "formal measurement"
            Stop-LoggedProcesses $entries
            $formalPaths = Write-FormalTails $offsets

            $channelSummaries = @()
            $casePass = $true
            foreach ($channel in @($case.Channels)) {
                $cfg = $channelConfigs[$channel]
                $hwaEntry = $entries | Where-Object { $_.Channel -eq $channel -and $_.Component -eq "hwa" }
                $videoEntry = $entries | Where-Object { $_.Channel -eq $channel -and $_.Component -eq "video" }
                $stimEntry = $entries | Where-Object { $_.Channel -eq $channel -and $_.Component -eq "stim" }
                $perfRows = Convert-TaggedRows $formalPaths[$hwaEntry.Stdout] "Perf"
                $probeRows = Convert-TaggedRows $formalPaths[$hwaEntry.Stdout] "RenderPerfProbe"
                $videoRows = Convert-TaggedRows $formalPaths[$videoEntry.Stderr] "VideoPerf"
                $stimRows = Convert-TaggedRows $formalPaths[$stimEntry.Stderr] "StimPerf"

                $udpValues = Get-Values $perfRows "udpFps"
                $renderValues = Get-Values $perfRows "renderFps"
                $outputValues = Get-Values $perfRows "outputFps"
                $displayValues = Get-Values $videoRows "displayFps"
                $pandaValues = Get-Values $probeRows "pandaDoFrameMs"
                $readbackValues = Get-Values $perfRows "readbackMs"
                $jpegValues = Get-Values $perfRows "jpegMs"
                $tcpValues = Get-Values $perfRows "tcpSendMs"
                $inputDepthValues = Get-Values $perfRows "inputQueueDepth"
                $outputDepthValues = Get-Values $perfRows "outputQueueDepth"
                $lagValues = Get-Values $perfRows "sourceSeqLag"
                $latencyAvgValues = Get-Values $videoRows "latencyAvgMs"
                $latencyP95Values = Get-Values $videoRows "latencyP95Ms"
                $droppedValues = Get-Values $perfRows "dropped"
                $inputOverwrittenValues = Get-Values $perfRows "inputOverwritten"
                $outputOverwrittenValues = Get-Values $perfRows "outputOverwritten"
                $overwrittenValues = Get-Values $perfRows "overwritten"
                $stimFpsValues = Get-Values $stimRows "sentFpsInstant"

                $outputFps = Get-Average $outputValues
                $displayFps = Get-Average $displayValues
                $latencyAvgMs = Get-Average $latencyAvgValues
                $failureReasons = @()
                if ($null -eq $outputFps -or $outputFps -lt 59.0) {
                    $failureReasons += "outputFps_lt_59_or_missing"
                }
                if ($null -eq $displayFps -or $displayFps -lt 59.0) {
                    $failureReasons += "displayFps_lt_59_or_missing"
                }
                if ($null -eq $latencyAvgMs -or $latencyAvgMs -lt 0.0 -or $latencyAvgMs -gt 80.0) {
                    $failureReasons += "latencyAvgMs_gt_80_or_missing"
                }
                if (Test-SustainedGrowth $inputDepthValues) { $failureReasons += "inputQueueDepth_sustained_growth" }
                if (Test-SustainedGrowth $outputDepthValues) { $failureReasons += "outputQueueDepth_sustained_growth" }
                if (Test-SustainedGrowth $lagValues) { $failureReasons += "sourceSeqLag_sustained_growth" }
                if ($case.SimMode -eq 1 -and (
                    (Get-LastValue $inputOverwrittenValues) -gt 0 -or
                    (Get-LastValue $outputOverwrittenValues) -gt 0)) {
                    $failureReasons += "sync_effective_input_or_output_overwritten"
                }
                if (-not (Test-Context $perfRows $channel $cfg.SensorID)) {
                    $failureReasons += "hwa_perf_context_missing_or_mixed"
                }
                if (-not (Test-Context $videoRows $channel $cfg.SensorID)) {
                    $failureReasons += "video_perf_context_missing_or_mixed"
                }
                if (-not (Test-Context $stimRows $channel $cfg.SensorID)) {
                    $failureReasons += "stim_perf_context_missing_or_mixed"
                }

                $hwaFormalText = [IO.File]::ReadAllText($formalPaths[$hwaEntry.Stdout])
                $wrongSensorID = Get-WrongSensorID $channel
                $rejectPattern = "\[PacketRouteReject\].*flag=0x38.*packetPlatID=1001.*packetSensorID=$wrongSensorID.*reason=sensor_mismatch"
                $wrongAcceptPattern = "\[PacketRoute\].*flag=0x38.*accepted=1.*packetPlatID=1001.*packetSensorID=$wrongSensorID"
                if ($hwaFormalText -notmatch $rejectPattern) { $failureReasons += "wrong_sensor_reject_not_observed" }
                if ($hwaFormalText -match $wrongAcceptPattern) { $failureReasons += "wrong_sensor_was_accepted" }

                $channelPass = $failureReasons.Count -eq 0
                if (-not $channelPass) { $casePass = $false }
                $summary = [pscustomobject]@{
                    case = $case.Name
                    channel = $channel
                    presentation = $case.Presentation
                    simMode = $case.SimMode
                    codec = "jpeg"
                    platID = 1001
                    sensorID = $cfg.SensorID
                    hwaPid = $hwaEntry.Process.Id
                    stimPid = $stimEntry.Process.Id
                    videoPid = $videoEntry.Process.Id
                    warmupSeconds = $WarmupSeconds
                    measureSeconds = $MeasureSeconds
                    sampleCount = @($perfRows).Count
                    videoSampleCount = @($videoRows).Count
                    udpFps = Get-Average $udpValues
                    renderFps = Get-Average $renderValues
                    outputFps = $outputFps
                    displayFps = $displayFps
                    stimulusFps = Get-Average $stimFpsValues
                    pandaDoFrameMs = Get-Average $pandaValues
                    readbackMs = Get-Average $readbackValues
                    jpegMs = Get-Average $jpegValues
                    tcpSendMs = Get-Average $tcpValues
                    inputQueueDepthMax = Get-Maximum $inputDepthValues
                    outputQueueDepthMax = Get-Maximum $outputDepthValues
                    sourceSeqLagMax = Get-Maximum $lagValues
                    latencyAvgMs = $latencyAvgMs
                    latencyP95Ms = Get-Average $latencyP95Values
                    dropped = Get-LastValue $droppedValues
                    inputOverwritten = Get-LastValue $inputOverwrittenValues
                    outputOverwritten = Get-LastValue $outputOverwrittenValues
                    overwritten = Get-LastValue $overwrittenValues
                    inputQueueSustainedGrowth = Test-SustainedGrowth $inputDepthValues
                    outputQueueSustainedGrowth = Test-SustainedGrowth $outputDepthValues
                    sourceSeqLagSustainedGrowth = Test-SustainedGrowth $lagValues
                    crossSensorReject = ($hwaFormalText -match $rejectPattern) -and ($hwaFormalText -notmatch $wrongAcceptPattern)
                    pass = $channelPass
                    failureReasons = @($failureReasons)
                }
                $channelSummaries += $summary
                $allResults += $summary
                Write-Output ("[R2Channel] case={0} channel={1} outputFps={2} displayFps={3} latencyAvgMs={4} status={5}" -f `
                    $case.Name, $channel, (Format-Number $outputFps), (Format-Number $displayFps),
                    (Format-Number $latencyAvgMs), $(if ($channelPass) { "PASS" } else { "FAIL" }))
            }
            $caseResult = [pscustomobject]@{
                case = $case.Name
                pass = $casePass
                channels = $channelSummaries
                error = $null
            }
            $caseResults += $caseResult
            [IO.File]::WriteAllText(
                (Join-Path $caseDir "summary.json"),
                ($caseResult | ConvertTo-Json -Depth 8),
                $utf8)
        }
        catch {
            $caseError = $_.Exception.Message
            $caseResults += [pscustomobject]@{
                case = $case.Name
                pass = $false
                channels = @()
                error = $caseError
            }
            Write-Warning "[R2Case] case=$($case.Name) status=FAIL error=$caseError"
        }
        finally {
            Stop-LoggedProcesses $entries
        }
    }
}
finally {
    foreach ($name in $oldEnvironment.Keys) {
        if ($null -eq $oldEnvironment[$name]) {
            [Environment]::SetEnvironmentVariable($name, $null, "Process")
        }
        else {
            [Environment]::SetEnvironmentVariable($name, [string]$oldEnvironment[$name], "Process")
        }
    }
}

$productionConfigUnchanged = $true
foreach ($path in $productionHashes.Keys) {
    if (-not (Test-Path -LiteralPath $path) -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $productionHashes[$path]) {
        $productionConfigUnchanged = $false
    }
}
if (-not $productionConfigUnchanged) {
    $caseResults += [pscustomobject]@{
        case = "production_config_integrity"
        pass = $false
        channels = @()
        error = "Production precise/coarse network config changed"
    }
}

$overallPass = $productionConfigUnchanged -and
    @($caseResults | Where-Object { -not $_.pass }).Count -eq 0 -and
    $caseResults.Count -eq $Cases.Count
$runSummary = [pscustomobject]@{
    status = if ($overallPass) { "PASS" } else { "FAIL" }
    generatedAt = (Get-Date).ToString("o")
    hwaBinary = $hwaExe
    hwaSha256 = $hwaSha256
    warmupSeconds = $WarmupSeconds
    measureSeconds = $MeasureSeconds
    productionConfigUnchanged = $productionConfigUnchanged
    cases = $caseResults
}
[IO.File]::WriteAllText(
    (Join-Path $runDir "summary.json"),
    ($runSummary | ConvertTo-Json -Depth 10),
    $utf8)
$allResults | Export-Csv -LiteralPath (Join-Path $runDir "summary.csv") -NoTypeInformation -Encoding UTF8

$markdown = New-Object Text.StringBuilder
[void]$markdown.AppendLine("# R2 dual-process 60 FPS acceptance")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("- Status: **$($runSummary.status)**")
[void]$markdown.AppendLine("- Warmup: $WarmupSeconds s")
[void]$markdown.AppendLine("- Formal measurement: $MeasureSeconds s")
[void]$markdown.AppendLine("- Production precise/coarse configs unchanged: $productionConfigUnchanged")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("| Case | Channel | outputFps | displayFps | latencyAvgMs | latencyP95Ms | inputQ max | outputQ max | lag max | dropped | overwritten | Cross sensor | Result |")
[void]$markdown.AppendLine("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
foreach ($row in $allResults) {
    [void]$markdown.AppendLine(("| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8} | {9} | {10} | {11} | {12} |" -f `
        $row.case, $row.channel, (Format-Number $row.outputFps), (Format-Number $row.displayFps),
        (Format-Number $row.latencyAvgMs), (Format-Number $row.latencyP95Ms),
        (Format-Number $row.inputQueueDepthMax), (Format-Number $row.outputQueueDepthMax),
        (Format-Number $row.sourceSeqLagMax), (Format-Number $row.dropped),
        (Format-Number $row.overwritten), $row.crossSensorReject,
        $(if ($row.pass) { "PASS" } else { "FAIL: " + ($row.failureReasons -join ',') })))
}
[IO.File]::WriteAllText((Join-Path $runDir "summary.md"), $markdown.ToString(), $utf8)

Write-Output "[R2Acceptance] status=$($runSummary.status)"
Write-Output "[R2Acceptance] hwaSha256=$hwaSha256"
Write-Output "[R2Acceptance] productionConfigUnchanged=$productionConfigUnchanged"
Write-Output "LOGDIR=$runDir"
if (-not $overallPass) { exit 1 }
