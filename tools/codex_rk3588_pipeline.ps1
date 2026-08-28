[CmdletBinding()]
param(
    [string]$RepoRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$VmHost = '192.168.203.128',
    [string]$VmUser = 'linaro',
    [string]$VmProjectRoot = '/home/linaro/userdata/HwaSimIR',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardRoot = '/userdata/HwaSimIR',
    [string]$WindowsHost = '192.168.1.188',
    [string]$SshKey = '',
    [int]$RunSeconds = 35,
    [switch]$SkipSourceSync,
    [switch]$SkipBuild,
    [switch]$SkipDeploy,
    [switch]$DdsH264Smoke
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2

function Invoke-Native {
    param([string]$FilePath, [string[]]$Arguments)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed ($LASTEXITCODE): $FilePath $($Arguments -join ' ')"
    }
}

function New-SshArguments {
    $args = @('-o', 'ConnectTimeout=10', '-o', 'ServerAliveInterval=10', '-o', 'StrictHostKeyChecking=accept-new')
    if ($SshKey) {
        $args += @('-i', $SshKey, '-o', 'BatchMode=yes')
    }
    return $args
}

function Invoke-Ssh {
    param([string]$Target, [string]$Command)
    $args = @(New-SshArguments) + @($Target, $Command)
    Invoke-Native -FilePath 'ssh.exe' -Arguments $args
}

function Invoke-Scp {
    param([string]$Source, [string]$Target, [switch]$Recursive)
    $args = @(New-SshArguments)
    if ($Recursive) { $args += '-r' }
    $args += @($Source, $Target)
    Invoke-Native -FilePath 'scp.exe' -Arguments $args
}

function Find-LatestReleaseExe {
    param([string]$Name)
    $candidate = Get-ChildItem -LiteralPath $RepoRoot -Recurse -File -Filter $Name |
        Where-Object { $_.FullName -match '(?i)[\\/](release|x64[\\/]release)[\\/]' } |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $candidate) {
        throw "Release executable not found: $Name"
    }
    return $candidate
}

function Set-IniValue {
    param([string[]]$Lines, [string]$Section, [string]$Key, [string]$Value)
    $currentSection = ''
    $replaced = $false
    $result = foreach ($line in $Lines) {
        if ($line -match '^\s*\[([^]]+)\]\s*$') {
            $currentSection = $Matches[1]
        }
        if ($currentSection -eq $Section -and $line -match ('^\s*' + [regex]::Escape($Key) + '\s*=')) {
            $replaced = $true
            "$Key=$Value"
        }
        else {
            $line
        }
    }
    if (-not $replaced) {
        throw "Missing INI key: [$Section] $Key"
    }
    return @($result)
}

function Get-LogMetricAverage {
    param([string[]]$Lines, [string]$Name)
    $values = @()
    foreach ($line in $Lines) {
        $value = Get-LogMetricValue $line $Name
        if ($null -ne $value) { $values += $value }
    }
    if ($values.Count -eq 0) { return $null }
    return ($values | Measure-Object -Average).Average
}

function Get-LogMetricValue {
    param([string]$Line, [string]$Name)
    $match = [regex]::Match($Line, '(?:^| )' + [regex]::Escape($Name) + '=([-+0-9.]+)')
    if (-not $match.Success) { return $null }
    return [double]$match.Groups[1].Value
}

$vmTarget = "$VmUser@$VmHost"
$boardTarget = "$BoardUser@$BoardHost"
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logDir = Join-Path $RepoRoot "logs\codex-rk3588-pipeline-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$videoProcess = $null
$boardConfigBackup = "$BoardRoot/Config/HwaSimIRRuntime.ini.before_codex_pipeline_$stamp"
$boardConfigWasBackedUp = $false

try {
    Write-Host '[Pipeline] Checking SSH and board network isolation'
    Invoke-Ssh $vmTarget 'printf VM_SSH_OK'
    $routeArguments = @(New-SshArguments)
    $route = & ssh.exe @routeArguments $boardTarget 'ip route'
    if ($LASTEXITCODE -ne 0) { throw 'Cannot access RK3588 through SSH' }
    $route | Set-Content -LiteralPath (Join-Path $logDir 'rk3588_ip_route.before.txt') -Encoding UTF8
    if ($route -match '(?m)^default\s') {
        throw 'RK3588 has a default route; stopped without changing network settings'
    }
    Invoke-Ssh $boardTarget "file $BoardRoot/HwaSim_IR && ldd $BoardRoot/HwaSim_IR && test -e /dev/mpp_service"

    $sourceRoot = Join-Path $RepoRoot 'HwaSim_IR\HwaSim_IR'
    if (-not $SkipSourceSync) {
        Write-Host '[Pipeline] Syncing source to the fixed VM project root (excluding .idea and build outputs)'
        $archive = Join-Path $logDir 'HwaSimIR-source.tgz'
        Invoke-Native 'tar.exe' @(
            '-C', $sourceRoot, '-czf', $archive,
            '--exclude=.idea', '--exclude=cmake-build-*', '--exclude=x64', '--exclude=build-*', '.'
        )
        Invoke-Scp $archive "$vmTarget`:$VmProjectRoot/.codex-source.tgz"
        Invoke-Ssh $vmTarget "mkdir -p $VmProjectRoot && tar -xzf $VmProjectRoot/.codex-source.tgz -C $VmProjectRoot"
        Invoke-Ssh $vmTarget "mkdir -p $VmProjectRoot/tools"
        Invoke-Scp (Join-Path $RepoRoot 'tools\rk3588_mpp_compile_check.sh') "$vmTarget`:$VmProjectRoot/tools/"
        Invoke-Scp (Join-Path $RepoRoot 'tools\rk3588_mpp_compile_check.cpp') "$vmTarget`:$VmProjectRoot/tools/"
    }

    $buildDir = "$VmProjectRoot/cmake-build-codex-rk3588"
    if (-not $SkipBuild) {
        Write-Host '[Pipeline] Running production MPP API check and full Release cross-build'
        Invoke-Ssh $vmTarget "chmod +x $VmProjectRoot/tools/rk3588_mpp_compile_check.sh && cd $VmProjectRoot && RKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp CXX=/usr/bin/aarch64-linux-gnu-g++ BUILD_DIR=$VmProjectRoot/cmake-build-codex-rk3588-mpp-check tools/rk3588_mpp_compile_check.sh"
        $cmakeOptions = @(
            '-DCMAKE_BUILD_TYPE=Release',
            '-DHWASIMIR_ENABLE_RKMPP=ON',
            '-DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp',
            '-DHWASIMIR_ENABLE_ZRDDS=ON',
            '-DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64',
            '-DHWASIMIR_ENABLE_FFMPEG=OFF',
            '-DPANDA3D_ROOT=/opt/panda3d-aarch64',
            '-DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4'
        ) -join ' '
        Invoke-Ssh $vmTarget "cmake -S $VmProjectRoot -B $buildDir -G Ninja $cmakeOptions && cmake --build $buildDir --parallel 4"
    }
    Invoke-Ssh $vmTarget "file $buildDir/HwaSim_IR && aarch64-linux-gnu-readelf -d $buildDir/HwaSim_IR | grep librockchip_mpp.so.1"

    if (-not $SkipDeploy) {
        Write-Host '[Pipeline] Deploying the AArch64 binary through Windows after a board-side backup'
        $localElf = Join-Path $logDir 'HwaSim_IR.aarch64'
        Invoke-Scp "$vmTarget`:$buildDir/HwaSim_IR" $localElf
        Invoke-Ssh $boardTarget "pkill -TERM -x HwaSim_IR || true; cd $BoardRoot && cp -p HwaSim_IR HwaSim_IR.before_codex_$stamp"
        Invoke-Scp $localElf "$boardTarget`:$BoardRoot/HwaSim_IR.codex_new"
        Invoke-Ssh $boardTarget "cd $BoardRoot && chmod +x HwaSim_IR.codex_new && mv HwaSim_IR.codex_new HwaSim_IR && file HwaSim_IR && ldd HwaSim_IR"
        Write-Host '[Pipeline] Deploying DDS runtime configuration and the validated RK3588 launcher'
        Invoke-Ssh $boardTarget "mkdir -p $BoardRoot/Config"
        Invoke-Scp (Join-Path $RepoRoot 'HwaSim_IR\Bin\Config\DDS') "$boardTarget`:$BoardRoot/Config/" -Recursive
        $boardBoundQos = Join-Path $RepoRoot 'tools\dds_d1_qos\ZRDDS_QOS_RK3588_192.168.1.116.xml'
        if (Test-Path -LiteralPath $boardBoundQos) {
            Invoke-Scp $boardBoundQos "$boardTarget`:$BoardRoot/Config/DDS/"
        }
        Invoke-Scp (Join-Path $RepoRoot 'tools\rk3588_run_hwasimir_precise.sh') "$boardTarget`:$BoardRoot/run_precise.sh"
        Invoke-Ssh $boardTarget "chmod +x $BoardRoot/run_precise.sh && test -f $BoardRoot/Config/DDS/ZRDDS_QOS_PROFILES.xml"
    }

    Invoke-Ssh $boardTarget "test -x $BoardRoot/HwaSim_IR && test -f $BoardRoot/Config/NetworkConfig_precise.ini && test -f $BoardRoot/Config/HwaSimIRRuntime.ini && test -f $BoardRoot/Config/DDS/ZRDDS_QOS_PROFILES.xml && test -d $BoardRoot/Config/Weather && test -d $BoardRoot/Config/TargetLib && test -d $BoardRoot/Config/SensorWave && test -d $BoardRoot/Config/IRRadiance && echo '[DeploymentManifest] result=PASS'"
    Invoke-Ssh $boardTarget "test -x $BoardRoot/run_precise.sh; test -f $BoardRoot/Config/DDS/ZRDDS_QOS_RK3588_192.168.1.116.xml; ps -ef | grep '[X]org :0' >/dev/null; if pgrep -x HwaSim_IR >/dev/null; then echo '[PipelinePreflight][FATAL] stale_hwasimir_process' >&2; exit 21; fi; if ss -H -lunp 2>/dev/null | awk '`$4 ~ /:8888`$/ { found=1 } END { exit !found }'; then echo '[PipelinePreflight][FATAL] udp_8888_in_use' >&2; ss -lunp | grep ':8888' >&2 || true; exit 22; fi; echo '[PipelinePreflight] process=PASS udp8888=PASS xorg=PASS qos=PASS'"

    Write-Host '[Pipeline] Creating temporary direct-link configs and starting all three endpoints'
    Invoke-Ssh $boardTarget "cp -p $BoardRoot/Config/HwaSimIRRuntime.ini $boardConfigBackup"
    $boardConfigWasBackedUp = $true
    $localBefore = Join-Path $logDir 'HwaSimIRRuntime.before.ini'
    Invoke-Scp "$boardTarget`:$BoardRoot/Config/HwaSimIRRuntime.ini" $localBefore
    $configLines = @(Get-Content -LiteralPath $localBefore)
    $settings = @(
        @('TcpOutput','Codec','auto'),
        @('TcpOutput','EnableH264Experimental','true'),
        @('TcpOutput','H264Encoder','mpp'),
        @('TcpOutput','H264BitrateKbps','4000'),
        @('TcpOutput','H264GopFrames','30'),
        @('TcpOutput','H264LowLatency','true'),
        @('TcpOutput','H264FallbackToJpeg','false'),
        @('TcpOutput','H264ForceKeyFrameOnStart','true'),
        @('TcpPayload','PacketVersion','3'),
        @('TcpPayload','SendVideo','true'),
        @('TcpPayload','SendAnnotation','true'),
        @('TcpPayload','SendRealtimeData','true'),
        @('TcpPayload','ForwardInitControl','true'),
        @('Performance','EnablePerfLog','1'),
        @('Performance','QuietPerfMode','true')
    )
    if ($DdsH264Smoke) {
        $settings += @(
            @('DdsVideo','Enable','true'),
            @('DdsVideo','Codec','auto'),
            @('DdsVideo','QosFile','Config/DDS/ZRDDS_QOS_RK3588_192.168.1.116.xml'),
            @('LocalRecording','Enable','false'),
            @('TcpPayload','SendVideo','false')
        )
    }
    foreach ($setting in $settings) {
        $configLines = @(Set-IniValue $configLines $setting[0] $setting[1] $setting[2])
    }
    $localTestConfig = Join-Path $logDir 'HwaSimIRRuntime.test.ini'
    [IO.File]::WriteAllLines($localTestConfig, $configLines, (New-Object Text.UTF8Encoding($false)))
    Invoke-Scp $localTestConfig "$boardTarget`:$BoardRoot/Config/HwaSimIRRuntime.ini"

    $videoExe = Find-LatestReleaseExe 'HwaSim_IR_VideoDisplay.exe'
    $stimExe = Find-LatestReleaseExe 'DataDrivenTestQT.exe'
    Get-FileHash -Algorithm SHA256 $videoExe.FullName, $stimExe.FullName |
        Format-List | Out-File (Join-Path $logDir 'windows_release_sha256.txt') -Encoding UTF8

    $videoConfig = Join-Path $logDir 'NetworkConfig_precise_video.ini'
    @"
[Identity]
channel=precise
platID=1001
sensorID=2

[Network]
ip=$WindowsHost
port=5555

[Recorder]
MaxRecordingQueueFrames=180
FlushTimeoutMs=10000
"@ | Set-Content -LiteralPath $videoConfig -Encoding UTF8

    $stimConfig = Join-Path $logDir 'NetworkConfig_precise_stim.ini'
    @"
[Identity]
channel=precise
platID=1001
sensorID=2

[RenderControl]
simMode=2
videoFps=60

[UDP]
localIp=$WindowsHost
localPort=9998
remoteIp=$BoardHost
remotePort=8888
"@ | Set-Content -LiteralPath $stimConfig -Encoding UTF8

    $env:QT_FORCE_STDERR_LOGGING = '1'
    $videoOut = Join-Path $logDir 'VideoDisplay.out.log'
    $videoErr = Join-Path $logDir 'VideoDisplay.err.log'
    $firstFramePng = Join-Path $logDir 'VideoDisplay.first-frame.png'
    if ($DdsH264Smoke) {
        $windowsQos = Join-Path $RepoRoot 'tools\dds_d1_qos\ZRDDS_QOS_WINDOWS_192.168.1.188.xml'
        if (-not (Test-Path -LiteralPath $windowsQos)) { throw "Windows bound DDS QoS missing: $windowsQos" }
        $autoExitMs = ($RunSeconds + 25) * 1000
        $videoLine = 'start "" /b "' + $videoExe.FullName + '" "--receive-transport=dds" "--dds-topic=HwaSimIR.Video.precise.H264" "--dds-codec=h264" "--dds-width=800" "--dds-height=800" "--dds-fps=60" "--dds-qos=' + $windowsQos + '" "--dds-dump-first-frame=' + $firstFramePng + '" "--acceptance-exit-ms=' + $autoExitMs + '" 1>"' + $videoOut + '" 2>"' + $videoErr + '"'
    }
    else {
        $videoLine = 'start "" /b "' + $videoExe.FullName + '" "--network-config=' + $videoConfig + '" "--channel=precise" 1>"' + $videoOut + '" 2>"' + $videoErr + '"'
    }
    & cmd.exe /d /s /c $videoLine
    Start-Sleep -Seconds 3
    $videoProcess = Get-Process HwaSim_IR_VideoDisplay -ErrorAction Stop |
        Sort-Object StartTime -Descending | Select-Object -First 1
    if (-not $DdsH264Smoke -and -not (netstat -ano -p tcp | Select-String "$WindowsHost`:5555 .*LISTENING .* $($videoProcess.Id)$")) {
        throw 'VideoDisplay is not listening on the expected address and port'
    }

    $boardLog = "$BoardRoot/logs/codex_pipeline_$stamp.log"
    $boardDdsEnable = if ($DdsH264Smoke) { 'true' } else { 'false' }
    $boardTcpVideo = if ($DdsH264Smoke) { 'false' } else { 'true' }
    Invoke-Ssh $boardTarget "cd $BoardRoot && mkdir -p logs && HwaSimIRDdsVideoEnable=$boardDdsEnable HwaSimIRDdsVideoCodec=auto HwaSimIRDdsVideoQosFile=Config/DDS/ZRDDS_QOS_RK3588_192.168.1.116.xml HwaSimIRLocalRecordingEnable=false HwaSimIRExitOnStop=true TcpSendVideo=$boardTcpVideo H264Encoder=mpp H264FallbackToJpeg=false RenderPresentationMode=HeadlessOffscreen nohup ./run_precise.sh </dev/null >$boardLog 2>&1 &"
    Start-Sleep -Seconds 5

    $stimArgs = @(
        "--network-config=$stimConfig", '--channel=precise', '--plat-id=1001', '--sensor-id=2',
        '--sim-mode=2', '--video-fps=60', '--phase1d-h264=1', '--save-mp4=0', "--duration-sec=$RunSeconds"
    )
    Push-Location (Join-Path $RepoRoot 'DataDrivenTestQT')
    try {
        # Windows PowerShell 5 wraps native stderr as non-terminating ErrorRecord
        # objects.  Qt intentionally writes qInfo to stderr, so temporarily keep
        # those diagnostic lines from tripping the script-wide Stop preference.
        $savedErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        & $stimExe.FullName @stimArgs 2>&1 | Tee-Object -FilePath (Join-Path $logDir 'DataDrivenTestQT.log')
        $stimExitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedErrorActionPreference
        if ($stimExitCode -ne 0) {
            throw "DataDrivenTestQT failed with exit code $stimExitCode"
        }
    }
    finally {
        $ErrorActionPreference = 'Stop'
        Pop-Location
    }
    Start-Sleep -Seconds 3
    if ($DdsH264Smoke) {
        Get-Process -Id $videoProcess.Id -ErrorAction Stop | Wait-Process -Timeout 20
    }
    Invoke-Scp "$boardTarget`:$boardLog" (Join-Path $logDir 'RK3588_HwaSim_IR.log')

    $boardLines = Get-Content -LiteralPath (Join-Path $logDir 'RK3588_HwaSim_IR.log')
    $videoLines = Get-Content -LiteralPath $videoErr
    if ($DdsH264Smoke) {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepoRoot 'tools\dds_d31_runtime_gate.ps1') -BoardLog (Join-Path $logDir 'RK3588_HwaSim_IR.log') -VideoDisplayLog $videoErr -FirstFramePng $firstFramePng
        if ($LASTEXITCODE -ne 0) { throw 'D3.1 hard runtime gate rejected the DDS smoke' }
    }
    $encodePass = [bool]($boardLines -match '\[H264EncodeSuccess\].*backend=mpp.*codec=h264_annexb.*keyFrame=true.*spsPps=true')
    $decodePass = [bool]($videoLines -match '\[H264DecodeSuccess\].*backend=ffmpeg.*codec=h264_annexb')
    $packetPass = $DdsH264Smoke -or [bool]($videoLines -match 'packetVersion=3 flags=0x7 codec=h264_annexb')
    $gpuPass = [bool]($boardLines -match '\[GpuBackend\].*glVendor=ARM.*glRenderer=Mali-LODX.*hardwareGpu=1') -and -not [bool]($boardLines -match 'llvmpipe|hardwareGpu=0')
    $startupPass = -not [bool]($boardLines -match '\[StartupFatal\]|绑定UDP端口失败|Address already in use')
    $glPass = -not [bool]($boardLines -match 'GL error 0x502|GL_INVALID_OPERATION')
    $ddsPass = $true
    $pixelPass = $true
    $ddsCountDetail = 'legacy TCP smoke'
    if ($DdsH264Smoke) {
        $senderMatches = [regex]::Matches(($boardLines -join "`n"), 'sentSamples=(\d+).*writeErrors=(\d+).*droppedSamples=(\d+)')
        $receiverMatches = [regex]::Matches(($videoLines -join "`n"), '\[DdsVideoReceiverPerf\] receivedSamples=(\d+).*ddsErrors=(\d+)')
        $diagMatches = [regex]::Matches(($videoLines -join "`n"), '\[DdsFrameDiag\].*min=([-+0-9.]+).*max=([-+0-9.]+).*mean=([-+0-9.]+).*stddev=([-+0-9.]+)')
        $ddsPass = $senderMatches.Count -gt 0 -and $receiverMatches.Count -gt 0
        if ($ddsPass) {
            $s = $senderMatches[$senderMatches.Count - 1]
            $r = $receiverMatches[$receiverMatches.Count - 1]
            $ddsPass = [int64]$s.Groups[1].Value -eq [int64]$r.Groups[1].Value -and [int64]$s.Groups[2].Value -eq 0 -and [int64]$s.Groups[3].Value -eq 0 -and [int64]$r.Groups[2].Value -eq 0
            $ddsCountDetail = "sent=$($s.Groups[1].Value) received=$($r.Groups[1].Value) writerErrors=$($s.Groups[2].Value) dropped=$($s.Groups[3].Value) readerErrors=$($r.Groups[2].Value)"
        }
        if ($diagMatches.Count -eq 0) { $pixelPass = $false }
        else {
            $diag = $diagMatches[$diagMatches.Count - 1]
            $pixelPass = [double]$diag.Groups[2].Value -gt [double]$diag.Groups[1].Value -and [double]$diag.Groups[4].Value -gt 0 -and (Test-Path -LiteralPath $firstFramePng)
        }
    }
    $firstH264Line = ($boardLines | Select-String '\[H264EncodeSuccess\]' | Select-Object -First 1).LineNumber
    $h264BoardLines = if ($firstH264Line) { @($boardLines | Select-Object -Skip ($firstH264Line - 1)) } else { @() }
    $fallbackSeen = [bool]($h264BoardLines -match '^\[CodecFallback\]|activeBackend=(jpeg|ffmpeg)|mpp_(init|cfg|encode).*failed')

    $activeBoardPerf = @($boardLines | Where-Object {
        if ($_ -notmatch '^\[Perf\].*mode=async') { return $false }
        $udpFps = Get-LogMetricValue $_ 'udpFps'
        $udpFrames = Get-LogMetricValue $_ 'udpFrames'
        return $null -ne $udpFps -and $null -ne $udpFrames -and $udpFps -ge 50.0 -and $udpFrames -gt 300
    })
    $h264VideoPerf = @($videoLines | Where-Object { $_ -match '^\[VideoPerf\].*activeCodec=h264_annexb' })
    $sourceValues = @($h264VideoPerf | ForEach-Object { Get-LogMetricValue $_ 'sourceSeq' } | Where-Object { $null -ne $_ })
    $maxSourceSeq = if ($sourceValues.Count) { ($sourceValues | Measure-Object -Maximum).Maximum } else { 0 }
    $activeVideoPerf = @($h264VideoPerf | Where-Object {
        $sourceSeq = Get-LogMetricValue $_ 'sourceSeq'
        return $null -ne $sourceSeq -and $sourceSeq -gt 300 -and $sourceSeq -lt $maxSourceSeq
    })
    $outputAverage = Get-LogMetricAverage $activeBoardPerf 'outputFps'
    $displayAverage = Get-LogMetricAverage $activeVideoPerf 'displayFps'
    $fpsPass = $DdsH264Smoke -or ($null -ne $outputAverage -and $null -ne $displayAverage -and $outputAverage -ge 59.0 -and $displayAverage -ge 59.0)

    $summary = @(
        "H264EncodeSuccess=$encodePass",
        "H264DecodeSuccess=$decodePass",
        "PacketV3AllSections=$packetPass",
        "UnexpectedFallback=$fallbackSeen",
        "outputFpsAverage=$outputAverage",
        "displayFpsAverage=$displayAverage",
        "FpsTargetPass=$fpsPass",
        "GpuMaliPass=$gpuPass",
        "UdpStartupPass=$startupPass",
        "GlErrorPass=$glPass",
        "DdsExactCountsPass=$ddsPass ($ddsCountDetail)",
        "DecodedPixelContentPass=$pixelPass",
        "OverallPass=$($encodePass -and $decodePass -and $packetPass -and -not $fallbackSeen -and $fpsPass -and $gpuPass -and $startupPass -and $glPass -and $ddsPass -and $pixelPass)"
    )
    $summary | Tee-Object -FilePath (Join-Path $logDir 'summary.txt')
    if (-not ($encodePass -and $decodePass -and $packetPass -and -not $fallbackSeen -and $fpsPass -and $gpuPass -and $startupPass -and $glPass -and $ddsPass -and $pixelPass)) {
        throw 'RK3588 runtime acceptance failed; see summary.txt and raw logs'
    }
}
finally {
    if ($videoProcess) {
        Get-Process -Id $videoProcess.Id -ErrorAction SilentlyContinue | Stop-Process -Force
    }
    try { Invoke-Ssh $boardTarget 'pkill -TERM -x HwaSim_IR || true' } catch { Write-Warning $_ }
    if ($boardConfigWasBackedUp) {
        try { Invoke-Ssh $boardTarget "cp -p $boardConfigBackup $BoardRoot/Config/HwaSimIRRuntime.ini" } catch { Write-Warning $_ }
    }
    Write-Host "[Pipeline] Log directory: $logDir"
}
