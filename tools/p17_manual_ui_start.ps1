[CmdletBinding()]
param([string]$Name='p17_final_manual_mwir')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$out=Join-Path $repo "logs/p17/production/manual/$Name"
if(Test-Path $out){throw "Evidence already exists: $out"}
if(@(Get-Process -Name DataDrivenTestQT,HwaSim_IR_VideoDisplay -ErrorAction SilentlyContinue).Count){throw 'Another local production case is active'}
if(-not $env:HWASIMIR_SSH_PASSWORD){throw 'Set HWASIMIR_SSH_PASSWORD in the current process'}
$env:SSH_ASKPASS=(Resolve-Path (Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd')).Path
$env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='p17'
$sshArgs=@('-o','PubkeyAuthentication=no','-o','PreferredAuthentications=password','-o','StrictHostKeyChecking=accept-new')
& ssh.exe @sshArgs root@192.168.1.116 'test -x /userdata/HwaSimIR/HwaSim_IR && ! pgrep -x HwaSim_IR >/dev/null'
if($LASTEXITCODE -ne 0){throw 'Board is busy or renderer unavailable'}
New-Item -ItemType Directory -Path $out | Out-Null
$senderDir=Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release/release'
$receiverDir=Join-Path $repo 'HwaSim_IR_VideoDisplay/x64/Release'
Copy-Item -LiteralPath (Join-Path $senderDir 'NetworkConfig.ini') -Destination (Join-Path $out 'DataDrivenTestQT.NetworkConfig.ini')
Copy-Item -LiteralPath (Join-Path $receiverDir 'NetworkConfig.ini') -Destination (Join-Path $out 'VideoDisplay.NetworkConfig.ini')
$p17Path=[Environment]::GetEnvironmentVariable('Path','Process')
[Environment]::SetEnvironmentVariable('PATH',$null,'Process')
[Environment]::SetEnvironmentVariable('Path',$p17Path,'Process')
$env:QT_FORCE_STDERR_LOGGING='1';$env:P7RecordingRoot=Join-Path $out 'recording'
$env:P5DdsVideoPath=Join-Path $out 'received.h264';$env:P5DdsVideoSamples='4500'
$receiver=Start-Process -FilePath (Join-Path $receiverDir 'HwaSim_IR_VideoDisplay.exe') -WorkingDirectory $receiverDir -WindowStyle Normal -PassThru -RedirectStandardOutput (Join-Path $out 'receiver.out.log') -RedirectStandardError (Join-Path $out 'receiver.err.log')
$remote='cd /userdata/HwaSimIR && timeout -s TERM -k 15s 1200s env RenderPresentationMode=HeadlessOffscreen P5MaterialView=0 HwaSimIRExitOnStop=false HwaSimIRLocalRecordingEnable=false ./run_precise.sh'
$board=Start-Process -FilePath ssh.exe -WindowStyle Hidden -PassThru -ArgumentList ($sshArgs+@('root@192.168.1.116',$remote)) -RedirectStandardOutput (Join-Path $out 'board.log') -RedirectStandardError (Join-Path $out 'board.ssh.err.log')
$sender=Start-Process -FilePath (Join-Path $senderDir 'DataDrivenTestQT.exe') -WorkingDirectory $senderDir -WindowStyle Normal -PassThru -RedirectStandardOutput (Join-Path $out 'sender.out.log') -RedirectStandardError (Join-Path $out 'sender.err.log')
@{entryType='ordinary_UI_zero_arguments';senderPid=$sender.Id;receiverPid=$receiver.Id;boardSshPid=$board.Id;startedUtc=[DateTime]::UtcNow.ToString('o');input='DataDrivenTestQT/1.txt';inputSha256=(Get-FileHash (Join-Path $repo 'DataDrivenTestQT/1.txt')).Hash.ToLowerInvariant();senderSha256=(Get-FileHash (Join-Path $senderDir 'DataDrivenTestQT.exe')).Hash.ToLowerInvariant();receiverSha256=(Get-FileHash (Join-Path $receiverDir 'HwaSim_IR_VideoDisplay.exe')).Hash.ToLowerInvariant();note='No UI actions automated by this launcher; INIT/RESET/START/STOP must be performed in the visible ordinary UI.'}|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $out 'manual_launch.json')
Write-Output $out
