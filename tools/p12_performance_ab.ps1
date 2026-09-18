[CmdletBinding()]
param(
    [int]$Seconds = 20,
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:HWASIMIR_SSH_PASSWORD,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$askPass = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd')).Path
if (-not $BoardPassword) { $BoardPassword = '123' }
if ($Seconds -lt 16) { throw 'Seconds must be at least 16 so sourceSeq 900 is reached at 60 Hz' }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$runId = "p12d-perf-ab-$stamp"
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo "logs\p12\p12d\performance\$runId" }
if (Test-Path -LiteralPath $OutputDirectory) { throw "Refusing to overwrite evidence: $OutputDirectory" }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8 = New-Object Text.UTF8Encoding($false)
# Windows environment names are case-insensitive, but this host can expose both
# PATH and Path. Start-Process rejects that duplicate dictionary, so normalize it.
$taskPath = [Environment]::GetEnvironmentVariable('Path','Process')
[Environment]::SetEnvironmentVariable('PATH',$null,'Process')
[Environment]::SetEnvironmentVariable('Path',$taskPath,'Process')

function Use-BoardAuth([scriptblock]$Action) {
    $oldPassword=$env:HWASIMIR_SSH_PASSWORD; $oldAsk=$env:SSH_ASKPASS
    $oldRequire=$env:SSH_ASKPASS_REQUIRE; $oldDisplay=$env:DISPLAY
    try {
        $env:HWASIMIR_SSH_PASSWORD=$BoardPassword; $env:SSH_ASKPASS=$askPass
        $env:SSH_ASKPASS_REQUIRE='force'; $env:DISPLAY='p12d-ab'
        & $Action
    } finally {
        $env:HWASIMIR_SSH_PASSWORD=$oldPassword; $env:SSH_ASKPASS=$oldAsk
        $env:SSH_ASKPASS_REQUIRE=$oldRequire; $env:DISPLAY=$oldDisplay
    }
}

function Ssh-Args { @('-o','ConnectTimeout=10','-o','ServerAliveInterval=10','-o','StrictHostKeyChecking=accept-new','-o','PubkeyAuthentication=no','-o','PreferredAuthentications=password') }

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

$preflight=Invoke-Board "set -eu; cd /userdata/HwaSimIR; ! pgrep -x HwaSim_IR >/dev/null; test -x HwaSim_IR; test -x run_precise.sh; echo ElfSha256=`$(sha256sum HwaSim_IR|awk '{print `$1}'); echo BuildId=`$(readelf -n HwaSim_IR|awk '/Build ID:/ {print `$3;exit}'); echo RuntimeConfigSha256=`$(sha256sum Config/HwaSimIRRuntime.ini|awk '{print `$1}')"
[IO.File]::WriteAllLines((Join-Path $OutputDirectory 'board_preflight.log'),$preflight,$utf8)

$cases=@(
    [ordered]@{name='A_ordinary_diagnostics_off';diagnostic=$false},
    [ordered]@{name='B_pfm_capture_on';diagnostic=$true}
)
$caseResults=@()
foreach($case in $cases){
    $caseDir=Join-Path $OutputDirectory $case.name
    New-Item -ItemType Directory -Path $caseDir | Out-Null
    $remoteDir="/userdata/HwaSimIR/logs/$runId/$($case.name)"
    Invoke-Board "set -eu; ! pgrep -x HwaSim_IR >/dev/null; mkdir -p '$remoteDir'" | Out-Null
    $assignments=''
    if($case.diagnostic){
        $assignments="RenderPerfProbe=1 LinearDiagnosticPath='$remoteDir/capture' LinearDiagnosticSeqs='180,900'"
    }
    $remote="cd /userdata/HwaSimIR && $assignments timeout -s TERM -k 8s $($Seconds+80)s ./run_precise.sh"
    $sshProcess=Use-BoardAuth {
        Start-Process -FilePath 'ssh.exe' -WindowStyle Hidden -PassThru -ArgumentList (@(Ssh-Args)+@("$BoardUser@$BoardHost",$remote)) `
            -RedirectStandardOutput (Join-Path $caseDir 'board.log') -RedirectStandardError (Join-Path $caseDir 'board.ssh.err.log')
    }
	# Force Process.Handle materialization before the child exits; otherwise
	# Windows PowerShell can return an empty ExitCode for a fast SSH wrapper.
	$null=$sshProcess.Handle
    try {
        Start-Sleep -Seconds 4
        & (Join-Path $PSScriptRoot 'p12_ordinary_target_case.ps1') -Name 'ordinary_ui' -Seconds $Seconds -OutputRoot $caseDir
        if($LASTEXITCODE -ne 0){throw "Ordinary UI case failed: $($case.name)"}
    } finally {
        try { Invoke-Board "pkill -TERM -x HwaSim_IR 2>/dev/null || true" | Out-Null } catch {}
        if(-not $sshProcess.WaitForExit(20000)){
            try { Stop-Process -Id $sshProcess.Id -Force } catch {}
            throw "Board launcher did not exit for $($case.name)"
        }
    }
	$sshProcess.Refresh()
	$boardExitCode=[int]$sshProcess.ExitCode
    if($boardExitCode -notin @(0,124,143)){throw "Board launcher exit code $boardExitCode for $($case.name)"}
    if($case.diagnostic){
        $artifactDir=Join-Path $caseDir 'board_diagnostics'
        New-Item -ItemType Directory -Path $artifactDir | Out-Null
        Use-BoardAuth {
            & scp.exe @(Ssh-Args) -r "$BoardUser@$BoardHost`:$remoteDir/." $artifactDir
            if($LASTEXITCODE -ne 0){throw "Failed to retrieve diagnostic files for $($case.name)"}
        }
    }
    $boardText=Get-Content -LiteralPath (Join-Path $caseDir 'board.log') -Raw
    $ordinaryOff=($boardText -match '\[RenderBackendConfig\].*RenderPerfProbe=0') -and ($boardText -notmatch '\[P6LinearCapturePerf\]')
    $perfRows=@([regex]::Matches($boardText,'(?m)^\[P6LinearCapturePerf\].*$')|ForEach-Object Value)
    if(-not $case.diagnostic -and -not $ordinaryOff){throw 'A case did not prove ordinary diagnostics-off state'}
    if($case.diagnostic -and $perfRows.Count -ne 2){throw "B case expected two PFM timing rows, got $($perfRows.Count)"}
    $caseResults += [ordered]@{
        name=$case.name; diagnostic=$case.diagnostic; seconds=$Seconds
        board_exit_code=$boardExitCode; ordinary_diagnostics_off=$ordinaryOff
        p6_capture_perf_rows=$perfRows
        board_log_sha256=(Get-FileHash -LiteralPath (Join-Path $caseDir 'board.log') -Algorithm SHA256).Hash.ToLowerInvariant()
        ordinary_result=(Get-Content -LiteralPath (Join-Path $caseDir 'ordinary_ui\result.json') -Raw|ConvertFrom-Json).result
    }
}

$summary=[ordered]@{
    schema='hwasimir.p12d.performance-ab.v1'; result='PASS'; run_id=$runId
    started_from=@($preflight); same_input_and_config=$true; source_sequences=@(180,900)
    cases=$caseResults; finished_utc=[DateTime]::UtcNow.ToString('o')
}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'summary.json'),($summary|ConvertTo-Json -Depth 8)+"`n",$utf8)
Write-Output "[P12D Performance A/B] result=PASS output=$OutputDirectory"
