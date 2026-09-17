[CmdletBinding()]
param(
    [ValidateSet('Plan', 'QuickCheck', 'Run')]
    [string]$Mode = 'Plan',
    [ValidateSet('All', 'SWIR', 'MWIR')]
    [string]$Band = 'All',
    [string]$Case = 'Representative',
    [ValidateSet('All', 'fixed', 'agc', 'annotated')]
    [string]$Variant = 'All',
    [ValidateRange(10, 3600)]
    [int]$Seconds = 10,
    [string]$MatrixPath = '',
    [string]$DeploymentReceipt = '',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:P11_BOARD_PASSWORD,
    [string]$SshKey = 'C:\Users\kahn1\.ssh\codex_hwasimir_ed25519',
    [ValidateSet('HeadlessOffscreen', 'VisibleWindow')]
    [string]$PresentationMode = 'HeadlessOffscreen',
    [double]$MinFps = 59.0,
    [double]$MaxFps = 61.5,
    [string]$OutputRoot = '',
    [switch]$ConfirmDdsImageRun
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$repo = (Resolve-Path -LiteralPath $repo).Path
if (-not $MatrixPath) { $MatrixPath = Join-Path $PSScriptRoot 'p11_windows_evidence_matrix.json' }
$MatrixPath = (Resolve-Path -LiteralPath $MatrixPath).Path
$pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if (-not $pythonCommand) { $pythonCommand = Get-Command python -ErrorAction Stop }
$python = $pythonCommand.Source
$checker = Join-Path $PSScriptRoot 'p11_rk3588_dds_image_matrix_check.py'
$acceptance = Join-Path $PSScriptRoot 'p11_rk3588_band_acceptance.ps1'
$finalizer = Join-Path $PSScriptRoot 'p11_rk3588_dds_image_finalize.py'
$boardRunner = Join-Path $PSScriptRoot 'p11_rk3588_band_board_run.sh'
$analyzer = Join-Path $PSScriptRoot 'p11_rk3588_acceptance_analyze.py'
$receiverExe = Join-Path $repo 'HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe'
$stimulusExe = Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe'
foreach ($path in @($checker, $acceptance, $finalizer, $boardRunner, $analyzer)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing DDS image matrix dependency: $path" }
}

$checkerVariant = if ($Variant -eq 'All') { 'ALL' } else { $Variant }
& $python $checker --matrix $MatrixPath --band $Band.ToUpperInvariant() --case $Case --variant $checkerVariant
if ($LASTEXITCODE -ne 0) { throw 'P11 RK3588 DDS image matrix quick-check failed' }
if ($Mode -eq 'QuickCheck') {
    Write-Host '[P11 RK3588 DDS Image Matrix] mode=QuickCheck result=PASS mutation=none processLaunch=none networkAccess=none transport=DDS-only'
    return
}

$matrix = Get-Content -Raw -LiteralPath $MatrixPath -Encoding UTF8 | ConvertFrom-Json
$representativeCases = @(
    'target_near_100m', 'exhaust_plume_on', 'cloud', 'rain', 'snow',
    'visibility_6km', 'active_in_band_on', 'solar_az_180_el_45',
    'target_altitude_high', 'speed_static_15mps'
)
$caseSelectors = if ($Case -ieq 'Representative') {
    $representativeCases
}
else {
    @($Case -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
}
$selectedCases = New-Object System.Collections.Generic.List[object]
$selectedCaseIds = @{}
foreach ($selector in $caseSelectors) {
    $matches = @($matrix.scenarios | Where-Object { [string]$_.id -like $selector })
    if (-not $matches) { throw "Case selector matched nothing: $selector" }
    foreach ($scenario in $matches) {
        if (-not $selectedCaseIds.ContainsKey([string]$scenario.id)) {
            $selectedCases.Add($scenario)
            $selectedCaseIds[[string]$scenario.id] = $true
        }
    }
}
$selectedBands = @($matrix.bands | Where-Object { $Band -eq 'All' -or $_.name -eq $Band })
$selectedVariants = @($matrix.captureVariants | Where-Object { $Variant -eq 'All' -or $_.name -eq $Variant })
if (-not $selectedCases.Count -or -not $selectedBands.Count -or -not $selectedVariants.Count) {
    throw 'DDS image matrix selection expanded to zero runs'
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $OutputRoot) { $OutputRoot = Join-Path $repo "logs\p11\rk3588\dds-image-$stamp" }
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$utf8NoBom = New-Object Text.UTF8Encoding($false)
$newline = [Environment]::NewLine

function Write-JsonFile {
    param([string]$Path, [object]$Value)
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 24) + $newline, $utf8NoBom)
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function ConvertTo-Invariant([double]$Value) {
    return [string]::Format([Globalization.CultureInfo]::InvariantCulture, '{0:R}', $Value)
}

function New-P11DdsFixture([object]$Scenario, [string]$SourceFixture, [string]$Destination) {
    $fixture = Get-Content -LiteralPath $SourceFixture -Raw -Encoding UTF8 | ConvertFrom-Json
    if (-not $fixture.SyntheticTelemetryTargets -or @($fixture.SyntheticTelemetryTargets).Count -ne 1) {
        throw 'P11 DDS image fixture must carry exactly one target'
    }
    $target = $fixture.SyntheticTelemetryTargets[0]
    $targetLat = [double]$target[3]
    $targetLon = [double]$target[4]
    $target[5] = [double]$Scenario.targetAltM
    if ($null -ne $Scenario.targetYawDeg) { $target[6] = [double]$Scenario.targetYawDeg }
    # Protocol targetLoc.speed is km/h; the matrix remains SI m/s.
    $target[9] = [double]$Scenario.targetSpeedMps * 3.6
    $bearingDeg = if ($null -ne $Scenario.cameraBearingDeg) { [double]$Scenario.cameraBearingDeg } else { 180.0 }
    $bearingRad = $bearingDeg * [Math]::PI / 180.0
    $cameraLat = $targetLat + ([double]$Scenario.rangeKm * [Math]::Cos($bearingRad) / 111.32)
    $cosLat = [Math]::Max(0.01, [Math]::Cos($targetLat * [Math]::PI / 180.0))
    $cameraLon = $targetLon + ([double]$Scenario.rangeKm * [Math]::Sin($bearingRad) / (111.32 * $cosLat))
    $sensorAltM = if ($null -ne $Scenario.sensorAltM) { [double]$Scenario.sensorAltM } else { [double]$Scenario.targetAltM }
    foreach ($frame in $fixture.Keyframes) {
        $frame[1] = $cameraLat
        $frame[2] = $cameraLon
        $frame[3] = $sensorAltM
    }
    $visibilityKm = if ($null -ne $Scenario.expectedVisibilityKm) { [double]$Scenario.expectedVisibilityKm } else { 6.0 }
    $weather = [ordered]@{}
    if ($fixture.InitializationWeather) {
        foreach ($property in $fixture.InitializationWeather.PSObject.Properties) {
            $weather[$property.Name] = $property.Value
        }
    }
    $weather['envVisibility'] = $visibilityKm * 1000.0
    if ($null -ne $Scenario.relativeHumidityPercent) {
        $weather['envHumidity'] = [double]$Scenario.relativeHumidityPercent
    }
    foreach ($mapping in @(
        @('rainMaxHeightM', 'envMaxHeightRain'),
        @('rainTransitionHeightM', 'envTransHeightRain'),
        @('snowMaxHeightM', 'envMaxHeightSnow'),
        @('snowTransitionHeightM', 'envTransHeightSnow'),
        @('rainSnowSpeedScale', 'envRainSnowSpeedScale')
    )) {
        $sourceName = $mapping[0]
        if ($null -ne $Scenario.$sourceName) { $weather[$mapping[1]] = [double]$Scenario.$sourceName }
    }
    $fixture | Add-Member -NotePropertyName InitializationWeather -NotePropertyValue ([pscustomobject]$weather) -Force
    $baseUtc = [DateTimeOffset]::FromUnixTimeMilliseconds([int64]$fixture.SimulationEpochMs).UtcDateTime
    if ($Scenario.solarDateUtc) {
        $baseUtc = [DateTime]::ParseExact([string]$Scenario.solarDateUtc, 'yyyy-MM-dd',
            [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal)
    }
    $scenarioUtc = [DateTime]::SpecifyKind($baseUtc.Date.AddHours([double]$Scenario.utcHour), [DateTimeKind]::Utc)
    $fixture.SimulationEpochMs = ([DateTimeOffset]$scenarioUtc).ToUnixTimeMilliseconds()
    $fixture.Description = "P11 formal DDS-only RK3588 fixture derived from $SourceFixture; case=$($Scenario.id); range=$($Scenario.rangeKm) km; bearing=$bearingDeg deg; sensorAlt=$sensorAltM m; targetAlt=$($Scenario.targetAltM) m; full target key preserved."
    [IO.File]::WriteAllText($Destination, ($fixture | ConvertTo-Json -Depth 16) + $newline, $utf8NoBom)
}

function Get-ActiveParameters([object]$Scenario, [string]$BandName) {
    $source = [string]$Scenario.illuminatorBand
    if (-not $source) { $source = 'FollowSensor' }
    if ($source -eq 'OppositeSensor') { $source = if ($BandName -eq 'SWIR') { 'MWIR' } else { 'SWIR' } }
    if ($source -eq 'MWIR') { return @{ Band = 'MWIR'; Center = 4.0; Width = 0.5 } }
    if ($source -eq 'SWIR') { return @{ Band = 'SWIR'; Center = 1.55; Width = 0.10 } }
    if ($BandName -eq 'MWIR') { return @{ Band = 'FollowSensor'; Center = 4.0; Width = 0.5 } }
    return @{ Band = 'FollowSensor'; Center = 1.55; Width = 0.10 }
}

$receipt = $null
if ($DeploymentReceipt) {
    $DeploymentReceipt = (Resolve-Path -LiteralPath $DeploymentReceipt).Path
    $receipt = Get-Content -Raw -LiteralPath $DeploymentReceipt -Encoding UTF8 | ConvertFrom-Json
    if ($receipt.schema -ne 'hwasimir.p11.rk3588.deployment-receipt.v1') {
        throw 'Unexpected deployment receipt schema'
    }
}
if ($Mode -eq 'Run') {
    if (-not $ConfirmDdsImageRun) { throw 'Run mode requires -ConfirmDdsImageRun' }
    if ($null -eq $receipt) { throw 'Run mode requires -DeploymentReceipt' }
}

$toolPaths = @($MyInvocation.MyCommand.Path, $MatrixPath, $checker, $acceptance, $finalizer, $boardRunner, $analyzer)
$toolIdentities = @($toolPaths | ForEach-Object {
    $item = Get-Item -LiteralPath $_
    [ordered]@{ path = $item.FullName; bytes = $item.Length; sha256 = Get-Sha256 $item.FullName }
})
$programIdentities = @(
    [ordered]@{
        role = 'DDS H.264 receiver/FFmpeg decoder'; path = $receiverExe
        bytes = (Get-Item -LiteralPath $receiverExe).Length; sha256 = Get-Sha256 $receiverExe
    },
    [ordered]@{
        role = 'DDS control stimulus'; path = $stimulusExe
        bytes = (Get-Item -LiteralPath $stimulusExe).Length; sha256 = Get-Sha256 $stimulusExe
    }
)
$plan = [ordered]@{
    schema = 'hwasimir.p11.rk3588.dds-image-matrix-plan.v1'
    created_utc = [DateTime]::UtcNow.ToString('o')
    mode = $Mode
    selection = [ordered]@{
        band = $Band; case = $Case; variant = $Variant
        bands = @($selectedBands.name); case_ids = @($selectedCases | ForEach-Object { $_.id })
        variants = @($selectedVariants.name)
        runnable_variant_runs = @($selectedCases | Where-Object { $_.runnable }).Count * $selectedBands.Count * $selectedVariants.Count
    }
    transport = [ordered]@{
        kind = 'DDS'; domain = 150; codec = 'h264_annexb'
        control_topics = @('HwaSimIR.Control', 'HwaSimIR.Init', 'HwaSimIR.Realtime', 'HwaSimIR.InitAck', 'HwaSimIR.VideoStatus')
        video_topic = 'HwaSimIR.Video.1001.2.H264'
        udp_payload_tested = $false; tcp_payload_tested = $false
    }
    duration_default_sec = $Seconds
    deployment_receipt = $DeploymentReceipt
    deployment_stage_id = if ($receipt) { [string]$receipt.stage_id } else { $null }
    elf_sha256 = if ($receipt) { ([string]$receipt.elf_sha256).ToLowerInvariant() } else { $null }
    config_manifest_sha256 = if ($receipt) { ([string]$receipt.config_manifest_sha256).ToLowerInvariant() } else { $null }
    blocked_coverage = @($matrix.blockedCoverage)
    tools = $toolIdentities
    program_identities = $programIdentities
}
Write-JsonFile -Path (Join-Path $OutputRoot 'matrix_plan.json') -Value $plan
Write-JsonFile -Path (Join-Path $OutputRoot 'blocked_coverage.json') -Value ([ordered]@{
    schema = 'hwasimir.p11.rk3588.dds-image-blocked-coverage.v1'
    count = @($matrix.blockedCoverage).Count
    entries = @($matrix.blockedCoverage)
})
if ($Mode -eq 'Plan') {
    Write-Host "[P11 RK3588 DDS Image Matrix] mode=Plan result=PASS remoteMutation=none payloadTransport=DDS-only output=$OutputRoot"
    return
}

$caseRows = New-Object System.Collections.Generic.List[object]
$failed = $false
foreach ($bandSpec in $selectedBands) {
    $bandName = [string]$bandSpec.name
    $protocolBand = [int]$bandSpec.protocolValue
    foreach ($sourceScenario in $selectedCases) {
        $scenario = $sourceScenario.PSObject.Copy()
        $caseDir = Join-Path (Join-Path $OutputRoot $bandName) ([string]$scenario.id)
        if (Test-Path -LiteralPath (Join-Path $caseDir 'case_request.json')) {
            throw "Refusing to overwrite existing DDS image case: $caseDir"
        }
        New-Item -ItemType Directory -Force -Path (Join-Path $caseDir 'variants') | Out-Null
        if (-not [bool]$scenario.runnable) {
            $blockedRequest = [ordered]@{
                schema = 'hwasimir.p11.rk3588.dds-image-case-request.v1'
                case_id = [string]$scenario.id; factor = [string]$scenario.factor
                comparison_group = [string]$scenario.comparisonGroup
                band = $bandName; protocol_band = $protocolBand; scenario = $scenario
                transport = [ordered]@{ control = 'DDS'; video = 'DDS'; udp_payload_tested = $false; tcp_payload_tested = $false }
                deployment_stage_id = [string]$receipt.stage_id
                expected_elf_sha256 = ([string]$receipt.elf_sha256).ToLowerInvariant()
                expected_config_manifest_sha256 = ([string]$receipt.config_manifest_sha256).ToLowerInvariant()
                program_identities = $programIdentities
            }
            Write-JsonFile -Path (Join-Path $caseDir 'case_request.json') -Value $blockedRequest
            $blockedSummary = [ordered]@{
                schema = 'hwasimir.p11.rk3588.dds-image-case.v1'; result = 'BLOCKED'
                case_id = [string]$scenario.id; factor = [string]$scenario.factor
                band = $bandName; protocol_band = $protocolBand; reason = [string]$scenario.blockedReason
            }
            $caseJson = Join-Path $caseDir 'case.json'
            Write-JsonFile -Path $caseJson -Value $blockedSummary
            $caseRows.Add([ordered]@{
                caseId = [string]$scenario.id; band = $bandName; factor = [string]$scenario.factor
                comparisonGroup = [string]$scenario.comparisonGroup; result = 'BLOCKED'
                path = $caseDir; caseSha256 = Get-Sha256 $caseJson
            })
            continue
        }

        if (-not [bool]$scenario.useAstronomicalSolar -and $null -eq $scenario.solarAzimuthDeg -and $null -eq $scenario.solarElevationDeg) {
            $scenario | Add-Member -NotePropertyName solarAzimuthDeg -NotePropertyValue ([double]$matrix.controlledSolarDefault.azimuthDeg) -Force
            $scenario | Add-Member -NotePropertyName solarElevationDeg -NotePropertyValue ([double]$matrix.controlledSolarDefault.elevationDeg) -Force
        }
        $scenarioFixture = Join-Path $repo ([string]$matrix.baseFixture)
        if ($scenario.fixture) { $scenarioFixture = Join-Path $repo ([string]$scenario.fixture) }
        $scenarioFixture = (Resolve-Path -LiteralPath $scenarioFixture).Path
        $fixturePath = Join-Path $caseDir 'input_fixture.json'
        New-P11DdsFixture -Scenario $scenario -SourceFixture $scenarioFixture -Destination $fixturePath
        $fixtureDocument = Get-Content -Raw -LiteralPath $fixturePath -Encoding UTF8 | ConvertFrom-Json
        $protocolTargetType = [int]$fixtureDocument.SyntheticTelemetryTargets[0][0]
        $caseSeconds = if ($null -ne $scenario.seconds) { [int]$scenario.seconds } else { $Seconds }
        if ($caseSeconds -lt 10 -or $caseSeconds -gt 3600) { throw "Invalid DDS image duration: $caseSeconds" }
        $evidenceTimeSec = if ($null -ne $scenario.evidenceTimeSec) { [double]$scenario.evidenceTimeSec } else { $caseSeconds / 2.0 }
        if ($evidenceTimeSec -le 2.0 -or $evidenceTimeSec -ge ($caseSeconds - 1.0)) {
            throw "DDS evidence time must be after startup and before stop: $evidenceTimeSec"
        }
        $dumpSeq = [Math]::Max(121, [Math]::Min(($caseSeconds * 60) - 1, [int][Math]::Round($evidenceTimeSec * 60.0)))
        $rawSeqs = "120,$dumpSeq"
        $active = Get-ActiveParameters -Scenario $scenario -BandName $bandName
        $sensorPixelAngleUrad = if ($null -ne $scenario.sensorPixelAngleUrad) { [double]$scenario.sensorPixelAngleUrad } else { 200.0 }
        $stimExtra = @(
            "--env-sky=$([int]$scenario.weather)",
            "--engine-state=$([int]$scenario.engineState)",
            "--illuminator-en=$(if ([bool]$scenario.illuminator) { 1 } else { 0 })",
            "--sensor-pixel-angle-urad=$(ConvertTo-Invariant $sensorPixelAngleUrad)",
            "--utc-hour=$(ConvertTo-Invariant ([double]$scenario.utcHour))",
            '--freeze-geometry'
        )
        foreach ($mapping in @(
            @('illuminatorAngleMrad', '--illuminator-angle-mrad='),
            @('illuminatorSpotRad', '--illuminator-spot-rad=')
        )) {
            if ($null -ne $scenario.($mapping[0])) {
                $stimExtra += $mapping[1] + (ConvertTo-Invariant ([double]$scenario.($mapping[0])))
            }
        }
        if ([bool]$scenario.illuminator) {
            foreach ($mapping in @(
                @('illuminatorOnStartSec', '--illuminator-on-start-sec='),
                @('illuminatorOnEndSec', '--illuminator-on-end-sec=')
            )) {
                if ($null -ne $scenario.($mapping[0])) {
                    $stimExtra += $mapping[1] + (ConvertTo-Invariant ([double]$scenario.($mapping[0])))
                }
            }
        }
        $solarOverride = ($null -ne $scenario.solarAzimuthDeg) -or ($null -ne $scenario.solarElevationDeg)
        if ($solarOverride -and (($null -eq $scenario.solarAzimuthDeg) -or ($null -eq $scenario.solarElevationDeg))) {
            throw "Solar override requires azimuth and elevation: $($scenario.id)"
        }
        $naturalSolar = if ($null -eq $scenario.naturalSolarEnable -or [bool]$scenario.naturalSolarEnable) { 'true' } else { 'false' }
        $boardBase = @{
            NaturalSolarEnable = $naturalSolar
            NaturalSolarEnableOpticalShadow = 'true'
            NaturalSolarEnableSolarThermal = 'true'
            NaturalSolarDebugLog = 'true'
            M1SolarOverrideEnable = if ($solarOverride) { 'true' } else { 'false' }
            M1SolarOverrideAzimuthDeg = ConvertTo-Invariant $(if ($solarOverride) { [double]$scenario.solarAzimuthDeg } else { 180.0 })
            M1SolarOverrideElevationDeg = ConvertTo-Invariant $(if ($solarOverride) { [double]$scenario.solarElevationDeg } else { 45.0 })
            M1SunVisibility = ConvertTo-Invariant $(if ($null -ne $scenario.m1SunVisibility) { [double]$scenario.m1SunVisibility } else { 1.0 })
            ActiveIlluminatorEnable = 'true'
            ActiveIlluminatorBand = [string]$active.Band
            ActiveIlluminatorCenterWavelengthUm = ConvertTo-Invariant ([double]$active.Center)
            ActiveIlluminatorBandwidthUm = ConvertTo-Invariant ([double]$active.Width)
            ActiveIlluminatorIntensityMode = 'BandIrradianceAtReference'
            ActiveIlluminatorDebugLog = 'true'
            EnableAeroThermalModel = 'true'
            ApplyAeroToRadiance = 'true'
            AeroApplyOnlyBand = 'SWIR_MWIR'
            PostprocessAA = if ($scenario.postprocessAA) { [string]$scenario.postprocessAA } else { 'Off' }
        }
        if ($scenario.solarDateUtc) { $boardBase['M1FallbackUtcDate'] = [string]$scenario.solarDateUtc }

        $request = [ordered]@{
            schema = 'hwasimir.p11.rk3588.dds-image-case-request.v1'
            created_utc = [DateTime]::UtcNow.ToString('o')
            case_id = [string]$scenario.id; factor = [string]$scenario.factor
            comparison_group = [string]$scenario.comparisonGroup; factor_delta = [string]$scenario.factorDelta
            band = $bandName; protocol_band = $protocolBand; range_um = @($bandSpec.rangeUm)
            duration_sec = $caseSeconds; resolution = '800x800'
            scenario = $scenario
            fixture = $fixturePath; fixture_sha256 = Get-Sha256 $fixturePath
            base_fixture = $scenarioFixture; base_fixture_sha256 = Get-Sha256 $scenarioFixture
            target_key = @('0x55', 3101, 5501); target_type = $protocolTargetType
            deterministic_replay_source_seq = $dumpSeq; raw_diagnostic_sequences = @(120, $dumpSeq)
            variants = @($selectedVariants.name)
            transport = [ordered]@{
                control = 'DDS'; video = 'DDS'; domain = 150; codec = 'h264_annexb'
                video_topic = 'HwaSimIR.Video.1001.2.H264'
                udp_payload_tested = $false; tcp_payload_tested = $false
            }
            fixed_mapping_comparison = [ordered]@{
                group = [string]$scenario.comparisonGroup; fixed_variant_agc = $false
                same_config_manifest_sha256 = ([string]$receipt.config_manifest_sha256).ToLowerInvariant()
                scaling_policy = 'no arbitrary per-case scaling'
            }
            deployment_stage_id = [string]$receipt.stage_id
            expected_elf_sha256 = ([string]$receipt.elf_sha256).ToLowerInvariant()
            expected_config_manifest_sha256 = ([string]$receipt.config_manifest_sha256).ToLowerInvariant()
            stimulus_extra_args = $stimExtra
            board_environment_base = $boardBase
            program_identities = $programIdentities
        }
        Write-JsonFile -Path (Join-Path $caseDir 'case_request.json') -Value $request

        foreach ($variantSpec in $selectedVariants) {
            $variantName = [string]$variantSpec.name
            $variantDir = Join-Path (Join-Path $caseDir 'variants') $variantName
            $acceptanceRoot = Join-Path $variantDir 'acceptance'
            New-Item -ItemType Directory -Force -Path $variantDir | Out-Null
            $boardEnvironment = @{}
            foreach ($key in $boardBase.Keys) { $boardEnvironment[$key] = $boardBase[$key] }
            $boardEnvironment['EnableAGC'] = if ($variantName -eq 'fixed') { 'false' } else { 'true' }
            $boardEnvironment['AnnotationOverlayInSensorImage'] = if ($variantName -eq 'annotated') { 'true' } else { 'false' }
            $invoke = @{
                Mode = 'Run'; Band = $bandName; RepoRoot = $repo
                DeploymentReceipt = $DeploymentReceipt; BoardHost = $BoardHost; BoardUser = $BoardUser
                BoardPassword = $BoardPassword; SshKey = $SshKey; PresentationMode = $PresentationMode
                DurationSec = $caseSeconds; MinFps = $MinFps; MaxFps = $MaxFps
                RawSeqs = $rawSeqs; ReceiverDumpFrameIndex = $dumpSeq
                CameraInput = $fixturePath; ScenarioId = [string]$scenario.id
                CaptureVariant = $variantName; StimExtraArgs = $stimExtra
                BoardEnvironment = $boardEnvironment; OutputDirectory = $acceptanceRoot
                ConfirmAcceptanceRun = $true
            }
            & $acceptance @invoke *> (Join-Path $variantDir 'acceptance_runner.log')
            if ($LASTEXITCODE -ne 0) { throw "DDS acceptance failed: band=$bandName case=$($scenario.id) variant=$variantName" }
        }
        & $python $finalizer --case-dir $caseDir --band $bandName --variants ($selectedVariants.name -join ',')
        if ($LASTEXITCODE -ne 0) { $failed = $true }
        $caseJson = Join-Path $caseDir 'case.json'
        if (-not (Test-Path -LiteralPath $caseJson)) { throw "DDS image finalizer did not write case.json: $caseDir" }
        $caseDocument = Get-Content -Raw -LiteralPath $caseJson -Encoding UTF8 | ConvertFrom-Json
        if ($caseDocument.result -eq 'FAIL') { $failed = $true }
        $caseRows.Add([ordered]@{
            caseId = [string]$scenario.id; band = $bandName; factor = [string]$scenario.factor
            comparisonGroup = [string]$scenario.comparisonGroup; result = [string]$caseDocument.result
            path = $caseDir; caseSha256 = Get-Sha256 $caseJson
        })
        Write-Host "[P11 RK3588 DDS Image Case] band=$bandName case=$($scenario.id) result=$($caseDocument.result) output=$caseDir"
    }
}

$summaryResult = if ($failed) { 'FAIL' } elseif (@($matrix.blockedCoverage).Count -gt 0) { 'PARTIAL' } else { 'PASS' }
$comparisonGroups = @($selectedCases | Group-Object comparisonGroup | ForEach-Object {
    [ordered]@{
        name = [string]$_.Name
        case_ids = @($_.Group | ForEach-Object { [string]$_.id })
        factors = @($_.Group | ForEach-Object { [string]$_.factor } | Sort-Object -Unique)
        fixed_mapping_variant = 'fixed'
        same_config_manifest_sha256 = ([string]$receipt.config_manifest_sha256).ToLowerInvariant()
        arbitrary_per_case_scaling = $false
    }
})
$summary = [ordered]@{
    schema = 'hwasimir.p11.rk3588.dds-image-matrix-summary.v1'
    result = $summaryResult
    completed_utc = [DateTime]::UtcNow.ToString('o')
    deployment_stage_id = [string]$receipt.stage_id
    elf_sha256 = ([string]$receipt.elf_sha256).ToLowerInvariant()
    config_manifest_sha256 = ([string]$receipt.config_manifest_sha256).ToLowerInvariant()
    transport = [ordered]@{
        kind = 'DDS'; domain = 150; codec = 'h264_annexb'
        video_topic = 'HwaSimIR.Video.1001.2.H264'
        udp_payload_tested = $false; tcp_payload_tested = $false
    }
    selection = $plan.selection
    cases = @($caseRows | ForEach-Object { $_ })
    comparison_groups = $comparisonGroups
    blocked_coverage = @($matrix.blockedCoverage)
    tool_identities = $toolIdentities
    program_identities = $programIdentities
    plan_sha256 = Get-Sha256 (Join-Path $OutputRoot 'matrix_plan.json')
}
Write-JsonFile -Path (Join-Path $OutputRoot 'dds_image_matrix_summary.json') -Value $summary
$rootManifest = Join-Path $OutputRoot 'artifact_manifest.sha256'
$manifestLines = @()
Get-ChildItem -LiteralPath $OutputRoot -Recurse -File | Where-Object { $_.FullName -ne $rootManifest } |
    Sort-Object FullName | ForEach-Object {
        $relative = $_.FullName.Substring($OutputRoot.Length).TrimStart('\') -replace '\\', '/'
        $manifestLines += "$(Get-Sha256 $_.FullName)  $relative"
    }
[IO.File]::WriteAllText($rootManifest, ($manifestLines -join "`n") + "`n", $utf8NoBom)
Write-Host "[P11 RK3588 DDS Image Matrix] mode=Run result=$summaryResult transport=DDS-only cases=$($caseRows.Count) blockers=$(@($matrix.blockedCoverage).Count) output=$OutputRoot"
if ($failed) { exit 1 }
