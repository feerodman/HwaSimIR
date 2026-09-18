[CmdletBinding()]
param(
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:HWASIMIR_SSH_PASSWORD,
    [string]$OutputDirectory = '',
    [switch]$SkipSwir,
    [switch]$SkipLong
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$senderDir = Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release\release'
$senderConfig = Join-Path $senderDir 'NetworkConfig.ini'
$longInput = Join-Path $senderDir 'ordinary_demo_1km_360s.txt'
$longManifest = "$longInput.json"
$receiverExe = Join-Path $repo 'HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe'
$askPass = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd')).Path
$analyzer = Join-Path $PSScriptRoot 'p12_performance_analyze.py'
if (-not $BoardPassword) { $BoardPassword = '123' }
foreach ($path in @($senderConfig,$longInput,$longManifest,$receiverExe,$analyzer)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing P12D runtime input: $path" }
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$runId = "ordinary-performance-$stamp"
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo "logs\p12\p12d\performance\$runId" }
if (Test-Path -LiteralPath $OutputDirectory) { throw "Refusing to overwrite evidence: $OutputDirectory" }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8 = New-Object Text.UTF8Encoding($false)
$senderConfigBackup = [IO.File]::ReadAllBytes($senderConfig)
$oldRecordingRoot = $env:P7RecordingRoot

# Start-Process rejects a process environment containing both PATH and Path.
$taskPath = [Environment]::GetEnvironmentVariable('Path','Process')
[Environment]::SetEnvironmentVariable('PATH',$null,'Process')
[Environment]::SetEnvironmentVariable('Path',$taskPath,'Process')

function Use-BoardAuth([scriptblock]$Action) {
    $oldPassword=$env:HWASIMIR_SSH_PASSWORD; $oldAsk=$env:SSH_ASKPASS
    $oldRequire=$env:SSH_ASKPASS_REQUIRE; $oldDisplay=$env:DISPLAY
    try {
        $env:HWASIMIR_SSH_PASSWORD=$BoardPassword; $env:SSH_ASKPASS=$askPass
        $env:SSH_ASKPASS_REQUIRE='force'; $env:DISPLAY='p12d-suite'
        & $Action
    } finally {
        $env:HWASIMIR_SSH_PASSWORD=$oldPassword; $env:SSH_ASKPASS=$oldAsk
        $env:SSH_ASKPASS_REQUIRE=$oldRequire; $env:DISPLAY=$oldDisplay
    }
}

function Ssh-Args {
    @('-o','ConnectTimeout=10','-o','ServerAliveInterval=10','-o','StrictHostKeyChecking=accept-new',
      '-o','PubkeyAuthentication=no','-o','PreferredAuthentications=password')
}

function Invoke-Board([string]$Command) {
    $result=Use-BoardAuth {
        $saved=$ErrorActionPreference; $ErrorActionPreference='Continue'
        $lines=& ssh.exe @(Ssh-Args) "$BoardUser@$BoardHost" $Command 2>&1
        $code=$LASTEXITCODE; $ErrorActionPreference=$saved
        [pscustomobject]@{Lines=@($lines);Code=$code}
    }
    if($result.Code -ne 0){throw "Board command failed ($($result.Code)): $Command`n$($result.Lines -join "`n")"}
    return @($result.Lines)
}

function Set-IniCase([int]$Band,[int]$EnvSky) {
    $text=[IO.File]::ReadAllText($senderConfig)
    foreach($replacement in @(
        @('(?im)^trackerSensorBand\s*=\s*\d+\s*$',[string]"trackerSensorBand=$Band"),
        @('(?im)^envSky\s*=\s*\d+\s*$',[string]"envSky=$EnvSky"),
        @('(?im)^InputFile\s*=\s*\S+\s*$','InputFile=ordinary_demo_1km_360s.txt'),
        @('(?im)^trackerSensorWidth\s*=\s*\d+\s*$','trackerSensorWidth=800'),
        @('(?im)^trackerSensorHeight\s*=\s*\d+\s*$','trackerSensorHeight=800'),
        @('(?im)^saveMP4En\s*=\s*\d+\s*$','saveMP4En=1'),
        @('(?im)^h264En\s*=\s*\d+\s*$','h264En=1')
    )) {
        $updated=[regex]::Replace($text,$replacement[0],$replacement[1])
        if($updated -eq $text -and $text -notmatch [regex]::Escape($replacement[1])) {
            throw "Effective sender INI lacks field for $($replacement[1])"
        }
        $text=$updated
    }
    [IO.File]::WriteAllText($senderConfig,$text,$utf8)
}

$cases=@(
    [ordered]@{Name='SWIR_60s_Cloudy';Seconds=60;Band=0;BandName='SWIR';EnvSky=1;Weather='Cloudy'},
    [ordered]@{Name='MWIR_60s_Cloudy';Seconds=60;Band=2;BandName='MWIR';EnvSky=1;Weather='Cloudy'}
)
if($SkipSwir){$cases=@($cases|Where-Object Name -ne 'SWIR_60s_Cloudy')}
if(-not $SkipLong){
    $cases += [ordered]@{Name='MWIR_300s_Snow';Seconds=300;Band=2;BandName='MWIR';EnvSky=3;Weather='Snow'}
}

$suite=[ordered]@{
    schema='hwasimir.p12d.ordinary-performance-suite.v1'; runId=$runId
    owner='single_serial_test_owner'; board="$BoardUser@$BoardHost"; windows='192.168.1.188'
    ordinaryEntryPoints=$true; forcedIdentityTopicQos=$false; ddsOnly=$true
    p6Diagnostics=$false; annexBDiagnosticCopy=$false; resolution='800x800'
    inputManifest=(Get-Content -LiteralPath $longManifest -Raw|ConvertFrom-Json)
    receiverExeSha256=(Get-FileHash -LiteralPath $receiverExe -Algorithm SHA256).Hash.ToLowerInvariant()
    cases=@(); startedUtc=[DateTime]::UtcNow.ToString('o')
}

try {
    $preflight=Invoke-Board "set -eu; cd /userdata/HwaSimIR; ! pgrep -x HwaSim_IR >/dev/null; test -x HwaSim_IR; test -x run_precise.sh; echo ElfSha256=`$(sha256sum HwaSim_IR|awk '{print `$1}'); echo BuildId=`$(readelf -n HwaSim_IR|awk '/Build ID:/ {print `$3;exit}'); echo RuntimeConfigSha256=`$(sha256sum Config/HwaSimIRRuntime.ini|awk '{print `$1}'); echo LauncherSha256=`$(sha256sum run_precise.sh|awk '{print `$1}'); grep -E '^(RenderPerfProbe|LinearDiagnostic|EnablePerfLog|QuietPerfMode)' Config/HwaSimIRRuntime.ini || true"
    [IO.File]::WriteAllLines((Join-Path $OutputDirectory 'board_preflight.log'),$preflight,$utf8)
    Copy-Item -LiteralPath $longManifest -Destination (Join-Path $OutputDirectory 'ordinary_demo_1km_360s.txt.json')

    foreach($case in $cases){
        Write-Output "[P12D Suite] starting case=$($case.Name) seconds=$($case.Seconds) band=$($case.BandName) weather=$($case.Weather)"
        $caseDir=Join-Path $OutputDirectory $case.Name
        New-Item -ItemType Directory -Path $caseDir | Out-Null
        Set-IniCase -Band $case.Band -EnvSky $case.EnvSky
        $plan=[ordered]@{
            name=$case.Name; seconds=$case.Seconds; band=$case.BandName; protocolBand=$case.Band
            weather=$case.Weather; envSky=$case.EnvSky; ordinaryZeroBusinessOverrides=$true
            input='ordinary_demo_1km_360s.txt'; recordingRoot=(Join-Path $caseDir 'recording')
            startedUtc=[DateTime]::UtcNow.ToString('o')
        }
        [IO.File]::WriteAllText((Join-Path $caseDir 'case_plan.json'),($plan|ConvertTo-Json -Depth 5)+"`n",$utf8)
        Copy-Item -LiteralPath $senderConfig -Destination (Join-Path $caseDir 'effective_sender_NetworkConfig.ini')

        $remoteTimeout=$case.Seconds+90
        $remote="cd /userdata/HwaSimIR && timeout -s TERM -k 8s ${remoteTimeout}s ./run_precise.sh"
        $sshProcess=Use-BoardAuth {
            Start-Process -FilePath 'ssh.exe' -WindowStyle Hidden -PassThru `
                -ArgumentList (@(Ssh-Args)+@("$BoardUser@$BoardHost",$remote)) `
                -RedirectStandardOutput (Join-Path $caseDir 'board.log') `
                -RedirectStandardError (Join-Path $caseDir 'board.ssh.err.log')
        }
        $null=$sshProcess.Handle
        try {
            Start-Sleep -Seconds 4
            $env:P7RecordingRoot=Join-Path $caseDir 'recording'
            & (Join-Path $PSScriptRoot 'p12_ordinary_target_case.ps1') -Name 'ordinary_ui' `
                -Seconds $case.Seconds -OutputRoot $caseDir -SkipAnnexBDump -SkipWidgetCapture 2>&1 | `
                Tee-Object -FilePath (Join-Path $caseDir 'ordinary_runner.log')
            if($LASTEXITCODE -ne 0){throw "Ordinary UI case failed: $($case.Name)"}
        } finally {
            $env:P7RecordingRoot=$oldRecordingRoot
            try { Invoke-Board "pkill -TERM -x HwaSim_IR 2>/dev/null || true" | Out-Null } catch {}
            if(-not $sshProcess.WaitForExit(30000)){
                try { Stop-Process -Id $sshProcess.Id -Force } catch {}
                throw "Board launcher did not exit for $($case.Name)"
            }
        }
        $sshProcess.Refresh()
        $boardExitCode=[int]$sshProcess.ExitCode
        if($boardExitCode -notin @(0,124,143)){throw "Board launcher exit code $boardExitCode for $($case.Name)"}
        $boardText=Get-Content -LiteralPath (Join-Path $caseDir 'board.log') -Raw
        if($boardText -match '\[P6Linear(Capture|Writer)Perf\]'){throw "P6 diagnostic capture leaked into ordinary case $($case.Name)"}
        if($boardText -notmatch '\[H264EncodeSuccess\].*backend=mpp.*codec=h264_annexb.*resolution=800x800'){
            throw "Case $($case.Name) did not prove RK MPP H.264 at 800x800"
        }
        if($boardText -notmatch ("\[Stage7 Weather\].*envSky="+$case.EnvSky+".*weatherName="+$case.Weather)){
            throw "Case $($case.Name) did not apply requested ordinary weather"
        }
        if($case.Weather -eq 'Snow'){
            $batchProof=($boardText -match '\[Stage7 WeatherConfig\].*precipitationMode=Batch') -and
                ($boardText -match '\[Stage7 PrecipitationOverlay\].*mode=Batch.*active=0.*type=snow') -and
                ($boardText -match '\[Stage7 Perf\].*precipitationNodeCount=[1-9][0-9]*.*precipitationMode=Batch')
            $overlayProof=($boardText -match '\[Stage7 WeatherConfig\].*precipitationMode=ScreenOverlay') -and
                ($boardText -match '\[Stage7 PrecipitationOverlay\].*mode=ScreenOverlay.*active=1.*type=snow')
            if(-not ($batchProof -or $overlayProof)){
                throw 'Snow long case did not prove the configured Batch or ScreenOverlay draw path'
            }
        }

        & python $analyzer $caseDir --requested-seconds $case.Seconds --band $case.BandName --weather $case.Weather | `
            Tee-Object -FilePath (Join-Path $caseDir 'analyzer.log')
        if($LASTEXITCODE -ne 0){throw "Analyzer failed for $($case.Name)"}
        $analysis=Get-Content -LiteralPath (Join-Path $caseDir 'performance.json') -Raw|ConvertFrom-Json
        $plan.finishedUtc=[DateTime]::UtcNow.ToString('o');$plan.boardExitCode=$boardExitCode
        $plan.functionalResult=$analysis.functionalResult;$plan.performanceResult=$analysis.performanceResult
        [IO.File]::WriteAllText((Join-Path $caseDir 'case_plan.json'),($plan|ConvertTo-Json -Depth 5)+"`n",$utf8)
        $suite.cases += [ordered]@{name=$case.Name;seconds=$case.Seconds;band=$case.BandName;weather=$case.Weather;
            functionalResult=$analysis.functionalResult;performanceResult=$analysis.performanceResult;
            products=$analysis.products;coldOver80=$analysis.hard80Policy.coldOver80Count;
            steadyOver80=$analysis.hard80Policy.steadyOver80Count}
        Write-Output "[P12D Suite] finished case=$($case.Name) functional=$($analysis.functionalResult) performance=$($analysis.performanceResult)"
    }
    $suite.finishedUtc=[DateTime]::UtcNow.ToString('o')
    $suite.functionalResult=if(@($suite.cases|Where-Object functionalResult -ne 'PASS').Count -eq 0){'PASS'}else{'FAIL'}
    $suite.performanceResult=if(@($suite.cases|Where-Object performanceResult -ne 'PASS').Count -eq 0){'PASS'}else{'FAIL'}
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'suite_summary.json'),($suite|ConvertTo-Json -Depth 10)+"`n",$utf8)
    Write-Output "[P12D Suite] completed functional=$($suite.functionalResult) performance=$($suite.performanceResult) output=$OutputDirectory"
}
finally {
    [IO.File]::WriteAllBytes($senderConfig,$senderConfigBackup)
    $env:P7RecordingRoot=$oldRecordingRoot
}
