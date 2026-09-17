param(
    [ValidateSet('DryRun','QuickCheck','Run')][string]$Mode='DryRun',
    [ValidateSet('All','SWIR','MWIR')][string]$Band='All',
    [string]$Case='*',
    [ValidateSet('All','fixed','agc','annotated')][string]$Variant='All',
    [ValidateRange(3,3600)][int]$Seconds=8,
    [string]$MatrixPath='',
    [string]$OutputRoot=''
)

$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if(!$MatrixPath){$MatrixPath=Join-Path $PSScriptRoot 'p11_windows_evidence_matrix.json'}
$MatrixPath=(Resolve-Path -LiteralPath $MatrixPath).Path
$python='F:\Programs\anaconda3\python.exe'
if(!(Test-Path -LiteralPath $python)){$python=(Get-Command python -ErrorAction Stop).Source}
$checker=Join-Path $PSScriptRoot 'p11_windows_evidence_check.py'
$finalizer=Join-Path $PSScriptRoot 'p11_windows_evidence_finalize.py'
$phase2a=Join-Path $PSScriptRoot 'phase2a_sync60_save_smoke.ps1'
$formalProgramPaths=[ordered]@{
    HwaSim_IR=(Join-Path $repo 'HwaSim_IR\Bin\HwaSim_IR.exe')
    HwaSim_IR_VideoDisplay=(Join-Path $repo 'HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe')
    DataDrivenTestQT=(Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe')
}
$formalProgramIdentities=@()
foreach($program in $formalProgramPaths.GetEnumerator()){
    if(!(Test-Path -LiteralPath $program.Value)){throw "Formal program missing: $($program.Value)"}
    $item=Get-Item -LiteralPath $program.Value
    $formalProgramIdentities+=[ordered]@{
        role=[string]$program.Key;path=$item.FullName;bytes=$item.Length
        sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $item.FullName).Hash
    }
}

$checkVariant=$(if($Variant -eq 'All'){'ALL'}else{$Variant})
$checkArgs=@($checker,'--matrix',$MatrixPath,'--mode',$(if($Mode -eq 'DryRun'){'dry-run'}else{'quick-check'}),'--band',$Band.ToUpperInvariant(),'--case',$Case,'--variant',$checkVariant)
& $python @checkArgs
if($LASTEXITCODE -ne 0){throw 'P11 Windows evidence matrix/preflight failed'}
if($Mode -ne 'Run'){
    Write-Output "[P11WindowsEvidence] mode=$Mode mutation=none processLaunch=none"
    exit 0
}

$matrix=Get-Content -LiteralPath $MatrixPath -Raw -Encoding UTF8 | ConvertFrom-Json
$selectedBands=@($matrix.bands | Where-Object {$Band -eq 'All' -or $_.name -eq $Band})
$selectedCases=@($matrix.scenarios | Where-Object {$_.id -like $Case})
$selectedVariants=@($matrix.captureVariants | Where-Object {$Variant -eq 'All' -or $_.name -eq $Variant})
if(!$selectedBands -or !$selectedCases -or !$selectedVariants){throw 'Selection expanded to zero runs'}

$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
if(!$OutputRoot){$OutputRoot=Join-Path $repo "logs\p11\windows\run-$stamp"}
$OutputRoot=[IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$runtimeIni=Join-Path $repo 'HwaSim_IR\Bin\Config\HwaSimIRRuntime.ini'
$hwaNetwork=Join-Path $repo 'HwaSim_IR\Bin\Config\NetworkConfig.ini'
$videoNetwork=Join-Path $repo 'HwaSim_IR_VideoDisplay\x64\Release\NetworkConfig.ini'
$stimNetwork=Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\NetworkConfig.ini'
$configPaths=@($runtimeIni,$hwaNetwork,$videoNetwork,$stimNetwork)
$configBytes=@{}
$configBefore=@{}
$backupDir=Join-Path $OutputRoot '_config_backup'
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
foreach($path in $configPaths){
    if(Test-Path -LiteralPath $path){
        $configBytes[$path]=[IO.File]::ReadAllBytes($path)
        $hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
        $configBefore[$path]=$hash
        $backupName=(($path.Substring($repo.Length).TrimStart('\')) -replace '[\\/:]','_')+'.'+$hash.Substring(0,12)+'.bak'
        Copy-Item -LiteralPath $path -Destination (Join-Path $backupDir $backupName) -Force
    }else{
        $configBytes[$path]=$null
        $configBefore[$path]='MISSING'
    }
}

$environmentBefore=[Environment]::GetEnvironmentVariables('Process')
$baseFixture=(Resolve-Path -LiteralPath (Join-Path $repo $matrix.baseFixture)).Path
$runErrors=New-Object System.Collections.Generic.List[string]
$activeScenarioId=''
$activeBand=''
$activeVariant=''
$abortRecord=$null

function Restore-P11Environment {
    $environmentCurrent=[Environment]::GetEnvironmentVariables('Process')
    foreach($key in @($environmentCurrent.Keys)){
        if(!$environmentBefore.Contains($key)){
            [Environment]::SetEnvironmentVariable([string]$key,$null,'Process')
        }
    }
    foreach($key in $environmentBefore.Keys){
        [Environment]::SetEnvironmentVariable([string]$key,[string]$environmentBefore[$key],'Process')
    }
}

function New-P11Fixture([object]$scenario,[string]$sourceFixture,[string]$destination){
    $fixture=Get-Content -LiteralPath $sourceFixture -Raw -Encoding UTF8 | ConvertFrom-Json
    if(!$fixture.SyntheticTelemetryTargets -or $fixture.SyntheticTelemetryTargets.Count -ne 1){throw 'P11 evidence fixture must carry exactly one target'}
    $target=$fixture.SyntheticTelemetryTargets[0]
    $targetLat=[double]$target[3]
    $targetLon=[double]$target[4]
    $target[5]=[double]$scenario.targetAltM
	if($null -ne $scenario.targetYawDeg){$target[6]=[double]$scenario.targetYawDeg}
	# Protocol targetLoc.speed is km/h.  Matrix values remain SI m/s for the
	# requested physical scenario and are converted exactly once at the sender.
	$target[9]=[double]$scenario.targetSpeedMps*3.6
	$bearingDeg=if($null -ne $scenario.cameraBearingDeg){[double]$scenario.cameraBearingDeg}else{180.0}
	$bearingRad=$bearingDeg*[Math]::PI/180.0
	$cameraLat=$targetLat+([double]$scenario.rangeKm*[Math]::Cos($bearingRad)/111.32)
	$cosLat=[Math]::Max(0.01,[Math]::Cos($targetLat*[Math]::PI/180.0))
	$cameraLon=$targetLon+([double]$scenario.rangeKm*[Math]::Sin($bearingRad)/(111.32*$cosLat))
	$sensorAltM=if($null -ne $scenario.sensorAltM){[double]$scenario.sensorAltM}else{[double]$scenario.targetAltM}
    foreach($frame in $fixture.Keyframes){
        $frame[1]=$cameraLat
		$frame[2]=$cameraLon
		$frame[3]=$sensorAltM
    }
	$visibilityKm = if($null -ne $scenario.expectedVisibilityKm){[double]$scenario.expectedVisibilityKm}else{6.0}
	$initWeather=[ordered]@{}
	if($fixture.InitializationWeather){
		foreach($property in $fixture.InitializationWeather.PSObject.Properties){$initWeather[$property.Name]=$property.Value}
	}
	$initWeather['envVisibility']=$visibilityKm*1000.0
	if($null -ne $scenario.relativeHumidityPercent){
		$initWeather['envHumidity']=[double]$scenario.relativeHumidityPercent
	}
	$optionalWeatherFields=@(
		@('rainMaxHeightM','envMaxHeightRain'),
		@('rainTransitionHeightM','envTransHeightRain'),
		@('snowMaxHeightM','envMaxHeightSnow'),
		@('snowTransitionHeightM','envTransHeightSnow'),
		@('rainSnowSpeedScale','envRainSnowSpeedScale')
	)
	foreach($mapping in $optionalWeatherFields){
		$scenarioName=$mapping[0]
		$fixtureName=$mapping[1]
		if($null -ne $scenario.$scenarioName){$initWeather[$fixtureName]=[double]$scenario.$scenarioName}
	}
	$fixture | Add-Member -NotePropertyName InitializationWeather -NotePropertyValue ([pscustomobject]$initWeather) -Force
	# WeatherCameraInput owns simulation time after startup, so encode the
	# requested UTC hour in the fixture itself.  Passing --utc-hour alone is
	# overwritten on the first realtime frame and makes solar A/B cases equal.
	$baseUtc=[DateTimeOffset]::FromUnixTimeMilliseconds([int64]$fixture.SimulationEpochMs).UtcDateTime
	if($scenario.solarDateUtc){
		$baseUtc=[DateTime]::ParseExact([string]$scenario.solarDateUtc,'yyyy-MM-dd',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::AssumeUniversal)
	}
	$scenarioUtc=[DateTime]::SpecifyKind($baseUtc.Date.AddHours([double]$scenario.utcHour),[DateTimeKind]::Utc)
	$fixture.SimulationEpochMs=([DateTimeOffset]$scenarioUtc).ToUnixTimeMilliseconds()
    $fixture.Description="P11 formal TCP evidence fixture derived from $sourceFixture; case=$($scenario.id); range=$($scenario.rangeKm) km; bearing=$bearingDeg deg; sensorAlt=$sensorAltM m; targetAlt=$($scenario.targetAltM) m; full target key preserved."
    $fixture | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $destination -Encoding UTF8
}

function Get-ActiveParameters([object]$scenario,[string]$bandName){
    $source=[string]$scenario.illuminatorBand
    if(!$source){$source='FollowSensor'}
    if($source -eq 'OppositeSensor'){$source=$(if($bandName -eq 'SWIR'){'MWIR'}else{'SWIR'})}
    if($source -eq 'MWIR'){
        return @{Band='MWIR';Center=4.0;Width=0.5}
    }
    if($source -eq 'SWIR'){
        return @{Band='SWIR';Center=1.55;Width=0.10}
    }
    if($bandName -eq 'MWIR'){
        return @{Band='FollowSensor';Center=4.0;Width=0.5}
    }
    return @{Band='FollowSensor';Center=1.55;Width=0.10}
}

try {
    foreach($scenario in $selectedCases){
        $activeScenarioId=[string]$scenario.id
        $activeBand=''
        $activeVariant=''
        if(!$scenario.runnable){
            $runErrors.Add("BLOCKED case=$($scenario.id) reason=$($scenario.blockedReason)")
            Write-Warning $runErrors[$runErrors.Count-1]
            continue
        }
		if(!$scenario.useAstronomicalSolar -and $null -eq $scenario.solarAzimuthDeg -and $null -eq $scenario.solarElevationDeg){
			$scenario | Add-Member -NotePropertyName solarAzimuthDeg -NotePropertyValue ([double]$matrix.controlledSolarDefault.azimuthDeg) -Force
			$scenario | Add-Member -NotePropertyName solarElevationDeg -NotePropertyValue ([double]$matrix.controlledSolarDefault.elevationDeg) -Force
        }
        foreach($bandSpec in $selectedBands){
            $activeBand=[string]$bandSpec.name
            $activeVariant=''
            $caseDir=Join-Path (Join-Path $OutputRoot $bandSpec.name) $scenario.id
            if(Test-Path -LiteralPath (Join-Path $caseDir 'case_request.json')){throw "Refusing to overwrite case evidence: $caseDir"}
            New-Item -ItemType Directory -Force -Path (Join-Path $caseDir 'variants') | Out-Null
            $fixturePath=Join-Path $caseDir 'input_fixture.json'
			$scenarioFixture=$baseFixture
			if($scenario.fixture){$scenarioFixture=(Resolve-Path -LiteralPath (Join-Path $repo ([string]$scenario.fixture))).Path}
			New-P11Fixture $scenario $scenarioFixture $fixturePath
			$fixtureDocument=Get-Content -LiteralPath $fixturePath -Raw -Encoding UTF8 | ConvertFrom-Json
			$protocolTargetType=[int]$fixtureDocument.SyntheticTelemetryTargets[0][0]
			$caseSeconds=if($null -ne $scenario.seconds){[int]$scenario.seconds}else{$Seconds}
			if($caseSeconds -lt 3 -or $caseSeconds -gt 3600){throw "Invalid per-case seconds=$caseSeconds case=$($scenario.id)"}
			$evidenceTimeSec=if($null -ne $scenario.evidenceTimeSec){[double]$scenario.evidenceTimeSec}else{$caseSeconds/2.0}
			if($evidenceTimeSec -le 0.0 -or $evidenceTimeSec -ge $caseSeconds){throw "Invalid evidenceTimeSec=$evidenceTimeSec case=$($scenario.id) seconds=$caseSeconds"}
			$dumpSeq=[Math]::Max(1,[Math]::Min(($caseSeconds*60)-1,[int][Math]::Round($evidenceTimeSec*60.0)))
			# In PowerShell @($null).Count is 1.  Gate missing optional timeline
			# properties explicitly so an ordinary static fixture is not mislabeled
			# as dynamic evidence.
			$hasTelemetryTimeline=$null -ne $fixtureDocument.SyntheticTelemetryKeyframes -and
				@($fixtureDocument.SyntheticTelemetryKeyframes).Count -gt 0
			$hasIlluminationTimeline=$null -ne $fixtureDocument.OrdinaryIlluminationEnableSteps -and
				@($fixtureDocument.OrdinaryIlluminationEnableSteps).Count -gt 0
			$dynamicScenario=$hasTelemetryTimeline -or $hasIlluminationTimeline -or [bool]$scenario.dynamic
            $request=[ordered]@{
                schema='hwasimir_p11_windows_case_request_1';caseId=$scenario.id;factor=$scenario.factor
                comparisonGroup=$scenario.comparisonGroup;factorDelta=$scenario.factorDelta
                band=$bandSpec.name;protocolBand=[int]$bandSpec.protocolValue;rangeUm=$bandSpec.rangeUm
				transport='TCP';resolution='800x800';seconds=$caseSeconds
                formalPrograms=@('HwaSim_IR','DataDrivenTestQT','HwaSim_IR_VideoDisplay')
                formalProgramIdentities=$formalProgramIdentities
                scenario=$scenario;fixture=$fixturePath;fixtureSha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $fixturePath).Hash
				baseFixture=$scenarioFixture;baseFixtureSha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $scenarioFixture).Hash
                profile=(Join-Path $repo $bandSpec.profile);profileSha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $repo $bandSpec.profile)).Hash
				configHashesBefore=$configBefore;captureVariants=@($selectedVariants.name)
				dynamicScenario=$dynamicScenario;evidenceTimeSec=$evidenceTimeSec;deterministicReplaySourceSeq=$dumpSeq
				deterministicReplay='independent formal replays decoded at the same requested sourceSeq'
                imageSource='actual formal TCP receiver recording decode';rawSource='renderer Stage6 pre-display floating texture only'
            }
            $request | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath (Join-Path $caseDir 'case_request.json') -Encoding UTF8

            foreach($variantSpec in $selectedVariants){
                $activeVariant=[string]$variantSpec.name
                $variantDir=Join-Path (Join-Path $caseDir 'variants') $variantSpec.name
                New-Item -ItemType Directory -Force -Path $variantDir | Out-Null
                $rawBase=Join-Path $variantDir 'raw_radiance'
                $env:WeatherCameraInput=$fixturePath
                $env:WeatherStartSec='0'
				# The reserved P11 test model pool is selected through the existing
				# process-scoped compatibility hook.  DataDrivenTestQT intentionally
				# has no undocumented --target-type CLI switch.
				$env:P6TestTargetType="0x$($protocolTargetType.ToString('X2'))"
                $env:RenderPresentationMode='HeadlessOffscreen'
                $env:LinearDiagnosticPath=$rawBase
                $env:LinearDiagnosticSeq=[string]$dumpSeq
                $env:Stage5OutputFrameDump='true'
                $env:Stage5OutputFrameDumpEvery=[string]$dumpSeq
                $env:Stage5OutputFrameDumpPath=(Join-Path $variantDir 'gpu_mapped.png')
				$env:Stage6DiagnosticsEnable='true'
                $env:HwaInputAuditDirectory=(Join-Path $variantDir 'input_audit')
                New-Item -ItemType Directory -Force -Path $env:HwaInputAuditDirectory | Out-Null
                $active=Get-ActiveParameters $scenario $bandSpec.name
				$sensorPixelAngleUrad=if($null -ne $scenario.sensorPixelAngleUrad){[double]$scenario.sensorPixelAngleUrad}else{200.0}
                $extra=@(
                    "--env-sky=$([int]$scenario.weather)",
                    "--engine-state=$([int]$scenario.engineState)",
                    "--illuminator-en=$(if($scenario.illuminator){1}else{0})",
					"--sensor-pixel-angle-urad=$([string]::Format([Globalization.CultureInfo]::InvariantCulture,'{0:R}',$sensorPixelAngleUrad))",
                    '--freeze-geometry'
                )
				foreach($activeArg in @(
					@('illuminatorAngleMrad','--illuminator-angle-mrad=', $true),
					@('illuminatorSpotRad','--illuminator-spot-rad=', $true),
					# DataDrivenTestQT's explicit on/off interval intentionally
					# overrides --illuminator-en.  Do not pass that interval for an
					# OFF control case, otherwise the stimulus would turn the protocol
					# bit back on even though the matrix requests illuminator=false.
					@('illuminatorOnStartSec','--illuminator-on-start-sec=', [bool]$scenario.illuminator),
					@('illuminatorOnEndSec','--illuminator-on-end-sec=', [bool]$scenario.illuminator)
				)){
					$name=$activeArg[0]
					if([bool]$activeArg[2] -and $null -ne $scenario.$name){
						$value=[string]::Format([Globalization.CultureInfo]::InvariantCulture,'{0:R}',[double]$scenario.$name)
						$extra+="$($activeArg[1])$value"
					}
				}
				$solarOverrideRequested=($null -ne $scenario.solarAzimuthDeg) -or ($null -ne $scenario.solarElevationDeg)
				if($solarOverrideRequested -and (($null -eq $scenario.solarAzimuthDeg) -or ($null -eq $scenario.solarElevationDeg))){
					throw "Solar override requires both azimuth and elevation: case=$($scenario.id)"
				}
				if($scenario.solarDateUtc){$env:M1FallbackUtcDate=[string]$scenario.solarDateUtc}
				$naturalSolarEnable=if($null -eq $scenario.naturalSolarEnable){'true'}elseif([bool]$scenario.naturalSolarEnable){'true'}else{'false'}
                $invoke=@{
                    Seconds=$caseSeconds;PostStimulusWaitSeconds=3;InitAckTimeoutSeconds=30;StopCompletionTimeoutSeconds=60
                    StimSensorBand=[int]$bandSpec.protocolValue
                    StimUtcHour=[double]$scenario.utcHour;StimExtraArgs=$extra;EnablePerfLog='true'
                    Stage5LogComponents='true';Stage5ComponentLogEveryFrames=30
                    M1CompareOnly='false';M1EnableRuntime='true';M1EnableNIRRuntime='true'
                    M1EnableSWIRRuntime='true';M1EnableMWIRRuntime='true'
                    EnableAeroThermalModel='true';ApplyAeroToRadiance='true';AeroApplyOnlyBand='SWIR_MWIR'
                    NaturalSolarEnable=$naturalSolarEnable;NaturalSolarEnableOpticalShadow='true';NaturalSolarEnableSolarThermal='true';NaturalSolarDebugLog='true'
					M1SolarOverrideEnable=$(if($solarOverrideRequested){'true'}else{'false'})
					M1SolarOverrideAzimuthDeg=$(if($solarOverrideRequested){[double]$scenario.solarAzimuthDeg}else{180.0})
					M1SolarOverrideElevationDeg=$(if($solarOverrideRequested){[double]$scenario.solarElevationDeg}else{45.0})
					M1SunVisibility=$(if($null -ne $scenario.m1SunVisibility){[double]$scenario.m1SunVisibility}else{1.0})
                    ActiveIlluminatorEnable='true';ActiveIlluminatorBand=$active.Band
                    ActiveIlluminatorCenterWavelengthUm=$active.Center;ActiveIlluminatorBandwidthUm=$active.Width
                    ActiveIlluminatorIntensityMode='BandIrradianceAtReference';ActiveIlluminatorDebugLog='true'
                    EnableAGC=$(if($variantSpec.agc){'true'}else{'false'})
                    AGCDebugLog=$(if($variantSpec.agc){'true'}else{'false'})
                    AnnotationOverlayInSensorImage=$(if($variantSpec.annotation){'true'}else{'false'})
					PostprocessAA=$(if($scenario.postprocessAA){[string]$scenario.postprocessAA}else{'Off'})
                }
                $started=[DateTime]::UtcNow
                try {
                    & $phase2a @invoke *> (Join-Path $variantDir 'phase2a_runner.log')
                    $formal=Get-ChildItem -LiteralPath (Join-Path $repo 'logs') -Directory -Filter 'phase2a-final-*' |
                        Where-Object {$_.LastWriteTimeUtc -ge $started.AddSeconds(-2)} |
                        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
                    if(!$formal){throw 'phase2a output directory not found'}
                    $formalCopy=Join-Path $variantDir 'formal_tcp_run'
                    Copy-Item -LiteralPath $formal.FullName -Destination $formalCopy -Recurse -Force
                    $summary=Get-Content -LiteralPath (Join-Path $formal.FullName 'phase2a_sync60_save_summary.json') -Raw -Encoding UTF8 | ConvertFrom-Json
                    if(!$summary.roundDir -or !(Test-Path -LiteralPath $summary.roundDir)){throw 'formal TCP receiver recording directory missing'}
                    Copy-Item -LiteralPath $summary.roundDir -Destination (Join-Path $variantDir 'receiver_recording') -Recurse -Force
                    [ordered]@{
                        sourceFormalLogDir=$formal.FullName;sourceRoundDir=$summary.roundDir
                        sourceFormalLogSha256=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $formal.FullName 'phase2a_sync60_save_summary.json')).Hash
                        requestedAgc=[bool]$variantSpec.agc;requestedAnnotation=[bool]$variantSpec.annotation
						rawDumpSeq=$dumpSeq;deterministicReplaySourceSeq=$dumpSeq
						dynamicScenario=$dynamicScenario;transport='TCP'
                    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $variantDir 'capture_provenance.json') -Encoding UTF8
                } finally {
                    Restore-P11Environment
                }
            }
            if($selectedVariants.Count -eq 3){
                & $python $finalizer $caseDir
                if($LASTEXITCODE -ne 0){$runErrors.Add("FINALIZE_FAIL case=$($scenario.id) band=$($bandSpec.name)")}
            }else{
                [ordered]@{result='INCOMPLETE';reason='All fixed/agc/annotated variants are required before finalization';selectedVariants=@($selectedVariants.name)} |
                    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $caseDir 'case.json') -Encoding UTF8
            }
        }
    }
} catch {
    $abortRecord=[ordered]@{
        schema='hwasimir_p11_windows_aborted_run_1'
        createdUtc=[DateTime]::UtcNow.ToString('o')
        result='FAIL'
        failedCase=$activeScenarioId
        failedBand=$activeBand
        failedVariant=$activeVariant
        error=$_.Exception.Message
        exceptionType=$_.Exception.GetType().FullName
        invocation=$_.InvocationInfo.PositionMessage
        preserved=$true
    }
    throw
} finally {
    Restore-P11Environment
    foreach($path in $configPaths){
        if($null -eq $configBytes[$path]){
            if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path -Force}
        }else{
            [IO.File]::WriteAllBytes($path,$configBytes[$path])
        }
    }
    $restoreRows=@()
    foreach($path in $configPaths){
        $after=$(if(Test-Path -LiteralPath $path){(Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash}else{'MISSING'})
        $restoreRows+=[ordered]@{path=$path;sha256Before=$configBefore[$path];sha256After=$after;restored=($after -eq $configBefore[$path])}
    }
    $restoreDocument=[ordered]@{
        schema='hwasimir_p11_windows_config_restore_1';result=$(if(@($restoreRows|Where-Object {!$_.restored}).Count -eq 0){'PASS'}else{'FAIL'})
        backupDirectory=$backupDir;files=$restoreRows;runErrors=$runErrors
    }
    $restoreDocument | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputRoot 'config_restore_manifest.json') -Encoding UTF8
    if($null -ne $abortRecord){
        $abortRecord.configRestoreResult=$restoreDocument.result
        $abortRecord.runErrorsBeforeAbort=@($runErrors)
        $abortRecord | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputRoot 'ABORTED_RUN.json') -Encoding UTF8
    }
}

if(@($runErrors | Where-Object {$_ -notlike 'BLOCKED*'}).Count -gt 0){throw "P11 Windows evidence run has failures; see $OutputRoot"}
Write-Output "[P11WindowsEvidence] mode=Run output=$OutputRoot blocked=$(@($runErrors|Where-Object {$_ -like 'BLOCKED*'}).Count)"
