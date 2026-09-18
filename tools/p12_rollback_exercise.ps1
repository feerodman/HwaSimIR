[CmdletBinding()]
param(
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:HWASIMIR_SSH_PASSWORD,
    [int]$Seconds = 8,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference='Stop'
if($Seconds -lt 6 -or $Seconds -gt 30){throw 'Seconds must be 6..30'}
$repo=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$remoteToolLocal=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p12_rollback_exercise.sh')).Path
$ordinaryTool=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p12_ordinary_target_case.ps1')).Path
$askPass=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd')).Path
if(-not $BoardPassword){$BoardPassword='123'}
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$stage="p12rb-$stamp"
if(-not $OutputDirectory){$OutputDirectory=Join-Path $repo "logs\p12\p12d\rollback\$stage"}
if(Test-Path -LiteralPath $OutputDirectory){throw "Refusing to overwrite rollback evidence: $OutputDirectory"}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$OutputDirectory=(Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8=New-Object Text.UTF8Encoding($false)
$remoteTool="/tmp/p12_rollback_exercise_$stage.sh"
$switched=$false

# Windows environment names are case-insensitive, but Start-Process rejects an
# inherited block that contains both PATH and Path.  Normalize this controller
# process before creating the long-lived SSH launcher.
$taskPath=[Environment]::GetEnvironmentVariable('Path','Process')
[Environment]::SetEnvironmentVariable('PATH',$null,'Process')
[Environment]::SetEnvironmentVariable('Path',$taskPath,'Process')

function Use-BoardAuth([scriptblock]$Action){
    $oldPassword=$env:HWASIMIR_SSH_PASSWORD;$oldAsk=$env:SSH_ASKPASS
    $oldRequire=$env:SSH_ASKPASS_REQUIRE;$oldDisplay=$env:DISPLAY
    try{
        $env:HWASIMIR_SSH_PASSWORD=$BoardPassword;$env:SSH_ASKPASS=$askPass
        $env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='p12-rollback'
        & $Action
    }finally{
        $env:HWASIMIR_SSH_PASSWORD=$oldPassword;$env:SSH_ASKPASS=$oldAsk
        $env:SSH_ASKPASS_REQUIRE=$oldRequire;$env:DISPLAY=$oldDisplay
    }
}
function Ssh-Args{
    @('-o','ConnectTimeout=10','-o','ServerAliveInterval=10','-o','StrictHostKeyChecking=accept-new',
      '-o','PubkeyAuthentication=no','-o','PreferredAuthentications=password')
}
function Invoke-Board([string]$Command,[string]$Log=''){
    $result=Use-BoardAuth{
        $saved=$ErrorActionPreference;$ErrorActionPreference='Continue'
        $lines=& ssh.exe @(Ssh-Args) "$BoardUser@$BoardHost" $Command 2>&1
        $code=$LASTEXITCODE;$ErrorActionPreference=$saved
        [pscustomobject]@{Lines=@($lines);Code=$code}
    }
    if($Log){[IO.File]::WriteAllLines($Log,@($result.Lines|ForEach-Object{[string]$_}),$utf8)}
    if($result.Code -ne 0){throw "Board command failed ($($result.Code)): $Command`n$($result.Lines -join "`n")"}
    return @($result.Lines|ForEach-Object{[string]$_})
}
function Copy-ToBoard([string]$Source,[string]$Destination){
    Use-BoardAuth{
        & scp.exe @(Ssh-Args) $Source "$BoardUser@$BoardHost`:$Destination"
        if($LASTEXITCODE -ne 0){throw "SCP to board failed: $LASTEXITCODE"}
    }
}
function Copy-FromBoard([string]$Source,[string]$Destination){
    Use-BoardAuth{
        & scp.exe @(Ssh-Args) "$BoardUser@$BoardHost`:$Source" $Destination
        if($LASTEXITCODE -ne 0){throw "SCP from board failed: $LASTEXITCODE"}
    }
}
function Parse-Facts([string[]]$Lines){
    $facts=[ordered]@{}
    foreach($line in $Lines){if($line -match '^([^=]+)=(.*)$'){$facts[$Matches[1]]=$Matches[2]}}
    return $facts
}
function Invoke-ReleaseLoop([string]$Release){
    $phase=Join-Path $OutputDirectory $Release
    New-Item -ItemType Directory -Path $phase|Out-Null
    $remote="cd /userdata/HwaSimIR && timeout -s TERM -k 8s $($Seconds+80)s ./run_precise.sh"
    $board=Use-BoardAuth{
        Start-Process -FilePath 'ssh.exe' -WindowStyle Hidden -PassThru `
            -ArgumentList (@(Ssh-Args)+@("$BoardUser@$BoardHost",$remote)) `
            -RedirectStandardOutput (Join-Path $phase 'board.log') `
            -RedirectStandardError (Join-Path $phase 'board.ssh.err.log')
    }
    $null=$board.Handle
    try{
        Start-Sleep -Seconds 4
        $ordinaryOutput=@(& $ordinaryTool -Name 'ordinary_ui' -Seconds $Seconds -OutputRoot $phase 2>&1)
        [IO.File]::WriteAllLines((Join-Path $phase 'ordinary_runner.log'),@($ordinaryOutput|ForEach-Object{[string]$_}),$utf8)
        $ordinaryOutput|ForEach-Object{Write-Host ([string]$_)}
        if($LASTEXITCODE -ne 0){throw "$Release ordinary DDS loop failed"}
    }finally{
        try{Invoke-Board 'pkill -TERM -x HwaSim_IR 2>/dev/null || true'|Out-Null}catch{}
        if(-not $board.WaitForExit(30000)){Stop-Process -Id $board.Id -Force;throw "$Release board launcher did not exit"}
    }
    $board.Refresh()
    if([int]$board.ExitCode -notin @(0,124,143)){throw "$Release board launcher exit=$($board.ExitCode)"}
    $resultPath=Join-Path $phase 'ordinary_ui\result.json'
    $result=Get-Content -LiteralPath $resultPath -Raw|ConvertFrom-Json
    $widget=Join-Path $phase 'ordinary_ui\receiver_actual_widget.png'
    $received=Join-Path $phase 'ordinary_ui\received.h264'
    if($result.result -ne 'PASS' -or -not (Test-Path -LiteralPath $widget) -or -not (Test-Path -LiteralPath $received)){
        throw "$Release lacks decoded widget/Annex-B evidence"
    }
    return [ordered]@{
        release=$Release;result='PASS';facts=Parse-Facts (Invoke-Board "sh $remoteTool facts $stage /userdata/HwaSimIR")
        widgetSha256=(Get-FileHash -LiteralPath $widget -Algorithm SHA256).Hash.ToLowerInvariant()
        receivedAnnexBSha256=(Get-FileHash -LiteralPath $received -Algorithm SHA256).Hash.ToLowerInvariant()
        receiverFinal=$result.evidence.receiverFinal;resultJsonSha256=(Get-FileHash -LiteralPath $resultPath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$phases=@()
try{
    Copy-ToBoard $remoteToolLocal $remoteTool
    Invoke-Board "chmod 755 $remoteTool; sh -n $remoteTool; ! pgrep -x HwaSim_IR >/dev/null" (Join-Path $OutputDirectory 'remote_tool_preflight.log')|Out-Null
    $p12Before=Parse-Facts (Invoke-Board "sh $remoteTool facts $stage /userdata/HwaSimIR" (Join-Path $OutputDirectory 'p12_before_facts.log'))
    # Arm controller-side recovery before the first remote mutation.  The
    # recovery mode is idempotent when the switch failed before directory swap.
    $switched=$true
    $switch=Invoke-Board "sh $remoteTool switch-p11 $stage /userdata/HwaSimIR" (Join-Path $OutputDirectory 'switch_to_p11.log')
    if(-not ($switch -match '\[P12RollbackSwitch\] result=PASS')){throw 'P11 switch receipt missing'}
    $phases += Invoke-ReleaseLoop 'p11_rollback'

    $restore=Invoke-Board "sh $remoteTool restore-p12 $stage /userdata/HwaSimIR" (Join-Path $OutputDirectory 'restore_p12.log')
    if(-not ($restore -match '\[P12RollbackRestore\] result=PASS')){throw 'P12 restore receipt missing'}
    $switched=$false
    $phases += Invoke-ReleaseLoop 'p12_restored'
    $p12After=Parse-Facts (Invoke-Board "sh $remoteTool facts $stage /userdata/HwaSimIR" (Join-Path $OutputDirectory 'p12_after_facts.log'))

    foreach($key in @('ElfSha256','BuildId','LauncherSha256','PerformanceToolSha256','ConfigManifestSha256','RuntimeConfigSha256','NetworkConfigSha256','PreciseNetworkConfigSha256','LutSha256','TargetsSha256','TargetLibManifestSubsetSha256')){
        if($p12Before[$key] -ne $p12After[$key]){throw "P12 restore identity mismatch: $key"}
    }
    $p11Phase=@($phases|Where-Object{$_.release -eq 'p11_rollback'})
    $p12Phase=@($phases|Where-Object{$_.release -eq 'p12_restored'})
    if($p11Phase.Count -ne 1 -or $p12Phase.Count -ne 1){throw 'Rollback phase records are incomplete'}
    if($p11Phase[0].facts.ElfSha256 -eq $p12Phase[0].facts.ElfSha256){throw 'Rollback and restored ELF identities are equal'}
    $summary=[ordered]@{
        schema='hwasimir.p12d.rollback-and-return.v1';result='PASS';stage=$stage
        rollbackSnapshot='.before_p11-20260918-001831';p11SnapshotRetained=$true
        p11TestedSnapshot="/userdata/HwaSimIR/rollback-p11-tested-$stage"
        p12RestoreSnapshot="/userdata/HwaSimIR/rollback-p12-exercise-$stage"
        p12Before=$p12Before;phases=$phases;p12After=$p12After
        gates=[ordered]@{
            p11FullConfigManifest=$true;p11DdsNewImage=$true;p12RestoredDdsNewImage=$true
            activeElfRestored=$true;activeConfigRestored=$true;modelsAndLutIdentityRestored=$true
            processExistenceAloneNotAccepted=$true
        }
        finishedUtc=[DateTime]::UtcNow.ToString('o')
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'rollback_summary.json'),($summary|ConvertTo-Json -Depth 12)+"`n",$utf8)
    Copy-FromBoard "/userdata/HwaSimIR/rollback-p12-exercise-$stage/p12_active_facts.env" (Join-Path $OutputDirectory 'board_p12_active_facts.env')
    Copy-FromBoard "/userdata/HwaSimIR/rollback-p12-exercise-$stage/p11_active_facts.env" (Join-Path $OutputDirectory 'board_p11_active_facts.env')
    Copy-FromBoard "/userdata/HwaSimIR/rollback-p12-exercise-$stage/p12_restored_facts.env" (Join-Path $OutputDirectory 'board_p12_restored_facts.env')
    Write-Output "[P12 Rollback] result=PASS stage=$stage output=$OutputDirectory"
}
finally{
    if($switched){
        try{
            Invoke-Board 'pkill -TERM -x HwaSim_IR 2>/dev/null || true'|Out-Null
            Invoke-Board "sh $remoteTool recover-p12 $stage /userdata/HwaSimIR" (Join-Path $OutputDirectory 'emergency_recover.log')|Out-Null
            $switched=$false
        }catch{
            Write-Error "EMERGENCY P12 RECOVERY FAILED: $($_.Exception.Message)"
        }
    }
}
