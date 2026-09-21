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

function Get-IniValue {
    param(
        [string]$Text,
        [string]$Section,
        [string]$Key
    )
    if (-not $Text) { return $null }
    $currentSection = ""
    foreach ($line in ($Text -split "`r?`n")) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^\[([^\]]+)\]$') {
            $currentSection = $Matches[1]
            continue
        }
        if ($currentSection -eq $Section -and $trimmed -match ('^' + [Regex]::Escape($Key) + '\s*=\s*(.*?)\s*$')) {
            return $Matches[1]
        }
    }
    return $null
}

$checks = New-Object System.Collections.Generic.List[object]

$requiredPaths = @(
    "HwaSim_IR\HwaSim_IR.sln",
    "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj",
    "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp",
    "HwaSim_IR\HwaSim_IR\UdpCommThread.cpp",
    "HwaSim_IR\HwaSim_IR\TcpCommThread.cpp",
    "HwaSim_IR\Bin\Config\HwaSimIRRuntime.ini",
    "HwaSim_IR\Bin\Config\SensorWave\default_MWIR.json",
    "DataDrivenTestQT\DataDrivenTestQT.pro",
    "DataDrivenTestQT\main.cpp",
    "DataDrivenTestQT\mainwindow.cpp",
    "DataDrivenTestQT\NetworkConfig.ini",
    "DataDrivenTestQT\1.txt",
    "DDS\Protocol\RealtimeSampleValidity.h",
    "DDS\Protocol\tests\realtime_sample_validity_test.cpp",
    "DDS\HwaSimIRP14FirstValidFixture\main.cpp",
    "materials\MaterialDatabase.csv",
    "transmittance\transmittance_0.3_15.txt",
    "temperatures\Temperatures_Yemen_Summer.csv"
)

foreach ($relativePath in $requiredPaths) {
    $fullPath = Join-Path $rootPath $relativePath
    $checks.Add((Add-Check "required path" (Test-Path -LiteralPath $fullPath) $relativePath))
}

$hwa = Read-Text "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$hwaNetwork = Read-Text "HwaSim_IR\Bin\Config\NetworkConfig.ini"
$hwaRuntime = Read-Text "HwaSim_IR\Bin\Config\HwaSimIRRuntime.ini"
$qtMain = Read-Text "DataDrivenTestQT\main.cpp"
$qt = Read-Text "DataDrivenTestQT\mainwindow.cpp"
$qtNetwork = Read-Text "DataDrivenTestQT\NetworkConfig.ini"
$vcxproj = Read-Text "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj"
$qtpro = Read-Text "DataDrivenTestQT\DataDrivenTestQT.pro"
$qtActive = (($qt -split "`r?`n") | Where-Object { $_ -notmatch '^\s*//' }) -join "`n"

$msbuild = "C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe"
$qmake512 = "D:\Qt\Qt5.12.12\5.12.12\mingw73_64\bin\qmake.exe"
$mingwMake = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\mingw32-make.exe"

$configuredSimMode = Get-IniValue $hwaRuntime "RenderControl" "ConfiguredSimMode"
$configuredVideoFps = Get-IniValue $hwaRuntime "RenderControl" "ConfiguredVideoFps"
$minRealtimeFps = Get-IniValue $hwaRuntime "RenderControl" "MinRealtimeFps"
$asyncInputPolicy = Get-IniValue $hwaRuntime "RenderControl" "AsyncInputPolicy"
$presentationMode = Get-IniValue $hwaRuntime "RenderBackend" "PresentationMode"
$headlessWidth = Get-IniValue $hwaRuntime "RenderBackend" "HeadlessWidth"
$headlessHeight = Get-IniValue $hwaRuntime "RenderBackend" "HeadlessHeight"
$commandInput = Get-IniValue $hwaRuntime "CommandTransport" "Input"
$ddsVideoEnable = Get-IniValue $hwaRuntime "DdsVideo" "Enable"
$ddsDomain = Get-IniValue $hwaRuntime "DdsProtocol" "DomainId"

$qtPlatId = Get-IniValue $qtNetwork "Identity" "platID"
$qtSensorId = Get-IniValue $qtNetwork "Identity" "sensorID"
$qtSimMode = Get-IniValue $qtNetwork "RenderControl" "simMode"
$qtVideoFps = Get-IniValue $qtNetwork "RenderControl" "videoFps"
$qtSendStepMs = Get-IniValue $qtNetwork "RenderControl" "sendStepMs"
$qtDdsDomain = Get-IniValue $qtNetwork "DdsProtocol" "DomainId"
$qtInputFile = Get-IniValue $qtNetwork "Demo" "InputFile"
$sensorBand = Get-IniValue $qtNetwork "SensorInit" "trackerSensorBand"
$sensorWidth = Get-IniValue $qtNetwork "SensorInit" "trackerSensorWidth"
$sensorHeight = Get-IniValue $qtNetwork "SensorInit" "trackerSensorHeight"
$originalPath = Join-Path $rootPath "DataDrivenTestQT\1.txt"
$originalSha = if (Test-Path -LiteralPath $originalPath) { (Get-FileHash -Algorithm SHA256 -LiteralPath $originalPath).Hash.ToLowerInvariant() } else { "missing" }
$originalRows = if (Test-Path -LiteralPath $originalPath) {
    @((Get-Content -LiteralPath $originalPath) | Where-Object { $_.Trim().Length -gt 0 }).Count - 1
} else { -1 }
$originalLines = if (Test-Path -LiteralPath $originalPath) { @(Get-Content -LiteralPath $originalPath) } else { @() }
$row659View = if ($originalLines.Count -gt 659) { ($originalLines[659] -split ',')[53] } else { "missing" }
$row660View = if ($originalLines.Count -gt 660) { ($originalLines[660] -split ',')[53] } else { "missing" }

$checks.Add((Add-Check "HwaSimIR DDS production timing" (($configuredSimMode -eq "2") -and ($configuredVideoFps -eq "60") -and ($minRealtimeFps -eq "60") -and ($asyncInputPolicy -eq "OrderedQueue")) "simMode=$configuredSimMode videoFps=$configuredVideoFps minRealtimeFps=$minRealtimeFps policy=$asyncInputPolicy"))
$checks.Add((Add-Check "HwaSimIR 800x800 headless backend" (($presentationMode -eq "HeadlessOffscreen") -and ($headlessWidth -eq "800") -and ($headlessHeight -eq "800")) "mode=$presentationMode size=${headlessWidth}x${headlessHeight}"))
$checks.Add((Add-Check "HwaSimIR DDS-only production input" (($commandInput -eq "dds") -and ($ddsVideoEnable -eq "true") -and ($ddsDomain -eq "150")) "input=$commandInput videoEnable=$ddsVideoEnable domain=$ddsDomain"))
$checks.Add((Add-Check "DataDrivenTestQT strict identity" (($qtPlatId -eq "1001") -and ($qtSensorId -eq "2") -and ($qtDdsDomain -eq "150")) "platID=$qtPlatId sensorID=$qtSensorId domain=$qtDdsDomain"))
$checks.Add((Add-Check "DataDrivenTestQT 60 Hz DDS timing" (($qtSimMode -eq "1") -and ($qtVideoFps -eq "60") -and ([Math]::Abs([double]$qtSendStepMs - (1000.0 / 60.0)) -lt 0.000001)) "simMode=$qtSimMode videoFps=$qtVideoFps sendStepMs=$qtSendStepMs"))
$checks.Add((Add-Check "DataDrivenTestQT sensor baseline" (($sensorBand -eq "2") -and ($sensorWidth -eq "800") -and ($sensorHeight -eq "800")) "band=$sensorBand size=${sensorWidth}x${sensorHeight}"))
$checks.Add((Add-Check "DataDrivenTestQT DDS production default" (($qtMain -match 'controlTransport\s*=\s*QStringLiteral\("dds"\)') -and ($qtInputFile -eq "1.txt")) "transport=dds inputFile=$qtInputFile"))
$checks.Add((Add-Check "DataDrivenTestQT one-target realtime contract" (($qtActive -match 'data\.targetNumValid\s*=\s*1') -and ($qtActive -match 'm_sendStepMs\s*=\s*settings\.value\("RenderControl/sendStepMs",\s*1000\.0/60\.0\)')) "targetNumValid=1 configDriven60Hz=1"))
$checks.Add((Add-Check "DataDrivenTestQT target selector binds protocol identity" (($qtActive -match 'm_targetTypeBox\s*=\s*new\s+QComboBox') -and ($qtActive -match 'm_targetTypeBox->addItem\([^\r\n]+,\s*0x55\)') -and ($qtActive -match 'm_targetTypeBox->addItem\([^\r\n]+,\s*0x66\)') -and ($qtActive -match 'data\.weaponState\.targetType\s*=\s*m_targetType') -and ($qtActive -match 'data\.targetState\[0\]\.targetType\s*=\s*m_targetType')) "QComboBox 0x55/0x66 + weaponState/targetState bindings"))
$checks.Add((Add-Check "DataDrivenTestQT demo visibility is explicit and default-off" (($qtActive -match 'forceVisibleForDemoCheck') -and ($qtActive -match 'm_forceVisibleForDemoCheck->setChecked\(false\)') -and ($qtActive -match 'm_targetTypeBox->setEnabled\(false\)')) "explicit UI override; frozen at INIT"))
$checks.Add((Add-Check "immutable original 1.txt identity" (($originalSha -eq "f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901") -and ($originalRows -eq 4318)) "sha256=$originalSha dataRows=$originalRows"))
$checks.Add((Add-Check "original 1.txt ViewValid boundary" (($row659View -eq "0") -and ($row660View -eq "1")) "dataRow659=$row659View dataRow660=$row660View"))
$checks.Add((Add-Check "Visual Studio toolset" ($vcxproj -match '<PlatformToolset>v140</PlatformToolset>') "v140 expected"))
$checks.Add((Add-Check "Qt project modules" (($qtpro -match 'network') -and ($qtpro -match 'widgets')) "Qt core/gui/network/widgets expected"))
$checks.Add((Add-Check "VS2015 MSBuild path" (Test-Path -LiteralPath $msbuild) $msbuild))
$checks.Add((Add-Check "Qt 5.12.12 qmake path" (Test-Path -LiteralPath $qmake512) $qmake512))
$checks.Add((Add-Check "Qt MinGW make path" (Test-Path -LiteralPath $mingwMake) $mingwMake))

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
