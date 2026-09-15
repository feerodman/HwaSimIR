param([string]$Name='P10_normal_ui60')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$dir=Join-Path $root "logs/p10/$Name"
New-Item -ItemType Directory -Force $dir | Out-Null
if(!$env:HWASIMIR_SSH_PASSWORD){throw 'Set HWASIMIR_SSH_PASSWORD for the authorized board.'}
$env:SSH_ASKPASS=Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd';$env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='p10'
if($Name -notmatch '^P10_[a-zA-Z0-9_]+$'){throw 'Invalid case name'}
$remote="/userdata/HwaSimIR/logs/$Name"
& ssh -o StrictHostKeyChecking=yes root@192.168.1.116 "test -z `"`$(pgrep -x HwaSim_IR || true)`" && mkdir -p '$remote'"
if($LASTEXITCODE){throw 'A renderer is already running or test directory unavailable'}
$ssh=Start-Process ssh.exe -WindowStyle Hidden -PassThru -ArgumentList @('-o','StrictHostKeyChecking=yes','root@192.168.1.116',"cd /userdata/HwaSimIR && HwaInputAuditDirectory='$remote' ./run_precise.sh >'$remote/board.log' 2>&1") -RedirectStandardOutput "$dir/ssh.out.log" -RedirectStandardError "$dir/ssh.err.log"
try {
    & "$PSScriptRoot/p9_start_windows.ps1" -OutputDirectory $dir
    $processes=Get-Content "$dir/processes.json" -Raw | ConvertFrom-Json
    Start-Sleep -Seconds 6
    & "$PSScriptRoot/p9_ui_control.ps1" -ProcessId $processes.sender -Action reset | Add-Content -Encoding UTF8 "$dir/ui_actions.jsonl"
    Start-Sleep -Seconds 1
    & "$PSScriptRoot/p9_ui_control.ps1" -ProcessId $processes.sender -Action init | Add-Content -Encoding UTF8 "$dir/ui_actions.jsonl"
    Start-Sleep -Seconds 6
    & "$PSScriptRoot/p9_run_ui60.ps1" -ProcessId $processes.sender -OutputDirectory $dir
    $drained=$false
    for($attempt=0;$attempt -lt 45;$attempt++){
        $count=& ssh -o StrictHostKeyChecking=yes root@192.168.1.116 "grep -c SyncRoundConservation '$remote/board.log' || true"
        if([int]$count -ge 1){$drained=$true;break}
        if($attempt%10 -eq 0){Write-Output 'Waiting for actual STOP finalization and audit flush'}
        Start-Sleep -Seconds 2
    }
    if(!$drained){throw 'STOP finalization marker did not arrive; files cannot be accepted'}
    Start-Sleep -Seconds 2
    & "$PSScriptRoot/p9_ui_control.ps1" -ProcessId $processes.sender -Action close
    & "$PSScriptRoot/p9_ui_control.ps1" -ProcessId $processes.receiver -Action close
    foreach($id in @($processes.sender,$processes.receiver)){
        $owned=Get-Process -Id $id -ErrorAction SilentlyContinue
        if($owned -and !$owned.WaitForExit(30000)){throw 'Windows process has not flushed/exited'}
    }
} finally {
    # This case starts only after proving no renderer was present.
    & ssh -o StrictHostKeyChecking=yes root@192.168.1.116 'pkill -TERM -x HwaSim_IR || true; for n in 1 2 3 4 5; do pgrep -x HwaSim_IR >/dev/null || break; sleep 1; done; ! pgrep -x HwaSim_IR >/dev/null'
    if(!$ssh.WaitForExit(10000)){throw 'Renderer launcher has not finished flushing/restoring policy'}
    & scp -q -o StrictHostKeyChecking=yes "root@192.168.1.116:$remote/*" $dir
    $ssh.Refresh();if(!$ssh.HasExited){Stop-Process -Id $ssh.Id -Force}
}
& F:/Programs/anaconda3/python.exe "$PSScriptRoot/p9_collect_results.py" $dir *> "$dir/analysis.log"
if($LASTEXITCODE){throw 'Actual normal UI evidence validation failed'}
$result=Get-Content "$dir/p9_results.json" -Raw | ConvertFrom-Json
if($result.inputAudit.result -ne 'PASS' -or $result.inputProductValidation.result -ne 'PASS'){throw 'Saved products or input ledger do not correspond completely'}
