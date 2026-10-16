param([ValidateSet('HD264','LD264')][string]$Topic='HD264',[string]$ReceiverBuild='build-p18-customer-receiver-v4',[string]$Suffix='',[ValidateRange(0,5000)][int]$PauseMs=0)
$ErrorActionPreference='Stop'
function Read-Sha256([string]$Path) {
    $stream=[System.IO.File]::OpenRead($Path)
    $hash=[System.Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','') }
    finally { $hash.Dispose(); $stream.Dispose() }
}
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$rxdir=Join-Path $repo "$ReceiverBuild/release"
$testdir=Join-Path $repo 'build-p18-contract-test/release'
$out=Join-Path $repo "logs/p18/tests/incremental_$Topic$Suffix"
if(Test-Path $out){throw 'Evidence directory exists'}
New-Item -ItemType Directory $out | Out-Null
$env:PATH="$rxdir;$env:PATH"
$args=@('--topic',"TX/$Topic",'--codec','h264','--pixel-format','rgb24','--width','320','--height','320','--fps','60','--domain','150',
    '--qos',"$rxdir/ZRDDS_PROTOCOL_QOS.xml",'--output',$out,'--idle-ms','6000','--timeout-ms','30000','--display','0','--ffmpeg',"$rxdir/ffmpeg.exe")
$rx=Start-Process "$rxdir/HwaSimIRCustomerTxVideoReceiverDemo.exe" -ArgumentList $args -WorkingDirectory $rxdir -WindowStyle Hidden -PassThru -RedirectStandardOutput "$out/receiver.log" -RedirectStandardError "$out/receiver.err.log"
try {
    Push-Location $testdir
    try { & ./customer_tx_contract_test.exe "$rxdir/ZRDDS_PROTOCOL_QOS.xml" "$repo/logs/p18/tests/diagnostic_color_120.h264" "TX/$Topic" "$PauseMs" *> "$out/sender.log"
        if($LASTEXITCODE){throw 'fixture publisher failed'} } finally {Pop-Location}
    if(!$rx.WaitForExit(45000)){throw 'receiver timeout'}
    if($rx.ExitCode){throw "receiver failed: $($rx.ExitCode)"}
    $a=Read-Sha256 "$repo/logs/p18/tests/diagnostic_color_120.h264"
    $b=Read-Sha256 "$out/received.h264"
    if($a -ne $b){throw 'reassembled stream differs'}
    $s=Get-Content "$out/summary.json" -Raw | ConvertFrom-Json
    if($s.decodedFrames -ne 120 -or $s.decodeErrors -ne 0 -or $s.failed){throw 'incremental decode failed'}
    & "$repo/.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffprobe.exe" -v error -count_frames -select_streams v:0 -show_entries stream=width,height,nb_read_frames -of json "$out/output.mp4" | Set-Content "$out/ffprobe.json"
    $probe=Get-Content "$out/ffprobe.json" -Raw|ConvertFrom-Json
    if($probe.streams[0].nb_read_frames -ne '120'){throw 'saved frame count mismatch'}
    Write-Host "PASS TX/${Topic}: 120 decoded and saved frames; cross-sample H264; SHA256 $a"
} finally {if(!$rx.HasExited){$rx.Kill()}}
