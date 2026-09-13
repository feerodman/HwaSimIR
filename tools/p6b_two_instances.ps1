param([string]$Name='two_instances')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=Join-Path $root "logs/p6b/$Name"
New-Item -ItemType Directory -Force $out | Out-Null
$runner=Join-Path $PSScriptRoot 'p6b_run_case.ps1'
$a=$null;$b=$null
try {
    $a=Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',$runner,
        '-Name',"${Name}_a",'-Windows','-Normal','-SkipRemoteCleanup','-PlatId','2001','-SensorId','2',
        '-CameraInput',"$root/tools/p6b_inputs/pair_mid.json",'-Seconds','24','-Rate','20','-RawDump') -RedirectStandardOutput "$out/a.runner.out.log" -RedirectStandardError "$out/a.runner.err.log"
    $aHandle=$a.Handle # Keep the process handle so ExitCode survives early child exit.
    # Late launch is a process-order test. Static world state uses no OS clock.
    Start-Sleep -Seconds 6
    $b=Start-Process powershell -WindowStyle Hidden -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',$runner,
        '-Name',"${Name}_b",'-Windows','-Normal','-SkipRemoteCleanup','-PlatId','2002','-SensorId','2',
        '-CameraInput',"$root/tools/p6b_inputs/pair_late_b.json",'-Seconds','16','-Rate','20','-RawDump') -RedirectStandardOutput "$out/b.runner.out.log" -RedirectStandardError "$out/b.runner.err.log"
    $bHandle=$b.Handle
    foreach($p in @($a,$b)){while(-not $p.WaitForExit(1000)){};if($p.ExitCode -ne 0){throw "Instance runner failed: $($p.ExitCode)"}}
    @{firstPid=$a.Id;secondPid=$b.Id;launchDelaySec=6;ddsIdentities=@('2001/2','2002/2');rendererFixture=$false} | ConvertTo-Json | Set-Content -Encoding UTF8 "$out/processes.json"
} finally {
    foreach($p in @($a,$b)){if($p){$p.Refresh();if(-not $p.HasExited){Stop-Process -Id $p.Id -Force}}}
}
