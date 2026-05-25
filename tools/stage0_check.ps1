param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path

function Add-Check {
    param(
        [string]$Name,
        [bool]$Ok,
        [string]$Detail
    )

    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Read-Text {
    param([string]$RelativePath)
    $path = Join-Path $rootPath $RelativePath
    if (-not (Test-Path -LiteralPath $path)) {
        return $null
    }
    return Get-Content -LiteralPath $path -Raw
}

function Match-Value {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Default = "not found"
    )
    if ($Text -and ($Text -match $Pattern)) {
        return $Matches[1]
    }
    return $Default
}

$checks = New-Object System.Collections.Generic.List[object]

$requiredPaths = @(
    "ConsoleApplication1_LLA\ConsoleApplication1.sln",
    "ConsoleApplication1_LLA\ConsoleApplication1\ConsoleApplication1.vcxproj",
    "ConsoleApplication1_LLA\ConsoleApplication1\HwaSimIR.cpp",
    "ConsoleApplication1_LLA\ConsoleApplication1\UdpCommThread.cpp",
    "ConsoleApplication1_LLA\ConsoleApplication1\TcpCommThread.cpp",
    "ConsoleApplication1_LLA\Bin\Config\SensorWave\default_MWIR.json",
    "DataDrivenTestQT\DataDrivenTestQT.pro",
    "DataDrivenTestQT\mainwindow.cpp",
    "DataDrivenTestQT\1.txt",
    "materials\MaterialDatabase.csv",
    "transmittance\transmittance_0.3_15.txt",
    "temperatures\Temperatures_Yemen_Summer.csv"
)

foreach ($relativePath in $requiredPaths) {
    $fullPath = Join-Path $rootPath $relativePath
    $checks.Add((Add-Check "required path" (Test-Path -LiteralPath $fullPath) $relativePath))
}

$hwa = Read-Text "ConsoleApplication1_LLA\ConsoleApplication1\HwaSimIR.cpp"
$qt = Read-Text "DataDrivenTestQT\mainwindow.cpp"
$vcxproj = Read-Text "ConsoleApplication1_LLA\ConsoleApplication1\ConsoleApplication1.vcxproj"
$qtpro = Read-Text "DataDrivenTestQT\DataDrivenTestQT.pro"
$qtActive = (($qt -split "`r?`n") | Where-Object { $_ -notmatch '^\s*//' }) -join "`n"

$udpLocalPort = Match-Value $hwa 'uint16_t\s+localPort\s*=\s*(\d+)'
$udpRemotePort = Match-Value $hwa 'uint16_t\s+remotePort\s*=\s*(\d+)'
$tcpPort = Match-Value $hwa 'uint16_t\s+serverPort\s*=\s*(\d+)'
$syncMode = if ($hwa -match 'SetRenderMode\s*\(\s*true\s*,\s*0\s*\)') { "sync UDP-driven" } else { "not confirmed" }

$qtLocalIp = Match-Value $qtActive 'm_localIpEdit\s*=\s*new\s+QLineEdit\("([^"]+)"\)'
$qtLocalPort = Match-Value $qtActive 'm_localPortEdit\s*=\s*new\s+QLineEdit\("([^"]+)"\)'
$qtRemoteIp = Match-Value $qtActive 'm_remoteIpEdit\s*=\s*new\s+QLineEdit\("([^"]+)"\)'
$qtRemotePort = Match-Value $qtActive 'm_remotePortEdit\s*=\s*new\s+QLineEdit\("([^"]+)"\)'
$sensorBand = Match-Value $qtActive 'trackerSensorBand\s*=\s*(\d+)'
$sensorWidth = Match-Value $qtActive 'trackerSensorWidth\s*=\s*(\d+)'
$sensorHeight = Match-Value $qtActive 'trackerSensorHeight\s*=\s*(\d+)'
$targetCount = Match-Value $qtActive 'targetNumValid\s*=\s*(\d+)'
$timeStep = Match-Value $qtActive 'm_timeStep\s*=\s*new\s+QLineEdit\("([^"]+)"\)'
$videoFps = Match-Value $qtActive 'videoFps\s*=\s*(\d+)'

$checks.Add((Add-Check "HwaSimIR UDP port" ($udpLocalPort -eq "8888") "local=$udpLocalPort remote=$udpRemotePort"))
$checks.Add((Add-Check "HwaSimIR TCP video port" ($tcpPort -eq "5555") "server=127.0.0.1:$tcpPort"))
$checks.Add((Add-Check "HwaSimIR render mode" ($syncMode -eq "sync UDP-driven") $syncMode))
$checks.Add((Add-Check "DataDrivenTestQT UDP defaults" (($qtLocalIp -eq "127.0.0.1") -and ($qtLocalPort -eq "9999") -and ($qtRemoteIp -eq "127.0.0.1") -and ($qtRemotePort -eq "8888")) "local=${qtLocalIp}:${qtLocalPort} remote=${qtRemoteIp}:${qtRemotePort}"))
$checks.Add((Add-Check "DataDrivenTestQT sensor baseline" (($sensorBand -eq "2") -and ($sensorWidth -eq "640") -and ($sensorHeight -eq "512")) "band=$sensorBand size=${sensorWidth}x${sensorHeight} videoFps=$videoFps"))
$checks.Add((Add-Check "DataDrivenTestQT realtime baseline" (($targetCount -eq "3") -and ($timeStep -eq "25")) "targetNumValid=$targetCount timeStepMs=$timeStep"))
$checks.Add((Add-Check "Visual Studio toolset" ($vcxproj -match '<PlatformToolset>v140</PlatformToolset>') "v140 expected"))
$checks.Add((Add-Check "Qt project modules" (($qtpro -match 'network') -and ($qtpro -match 'widgets')) "Qt core/gui/network/widgets expected"))

Write-Host "Stage 0 baseline check"
Write-Host "Workspace: $rootPath"
Write-Host ""
$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Failed checks: $($failed.Count)"
    if ($Strict) {
        exit 1
    }
    exit 2
}

Write-Host ""
Write-Host "All Stage 0 checks passed."
