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
    return Get-Content -LiteralPath $path -Raw -Encoding UTF8
}

function Get-JsonNumber {
    param(
        [string]$Text,
        [string]$Key
    )
    $match = [regex]::Match($Text, '"' + [regex]::Escape($Key) + '"\s*:\s*([-+0-9.eE]+)')
    if (-not $match.Success) {
        return $null
    }
    return [double]$match.Groups[1].Value
}

$checks = New-Object System.Collections.Generic.List[object]

$requiredPaths = @(
    "ConsoleApplication1_LLA\ConsoleApplication1\IR\IRTypes.h",
    "ConsoleApplication1_LLA\ConsoleApplication1\IR\IRTypes.cpp",
    "ConsoleApplication1_LLA\ConsoleApplication1\IR\IRConfig.h",
    "ConsoleApplication1_LLA\ConsoleApplication1\IR\IRConfig.cpp"
)

foreach ($relativePath in $requiredPaths) {
    $fullPath = Join-Path $rootPath $relativePath
    $checks.Add((Add-Check "IR module file" (Test-Path -LiteralPath $fullPath) $relativePath))
}

$vcxproj = Read-Text "ConsoleApplication1_LLA\ConsoleApplication1\ConsoleApplication1.vcxproj"
$filters = Read-Text "ConsoleApplication1_LLA\ConsoleApplication1\ConsoleApplication1.vcxproj.filters"
$hwa = Read-Text "ConsoleApplication1_LLA\ConsoleApplication1\HwaSimIR.cpp"
$types = Read-Text "ConsoleApplication1_LLA\ConsoleApplication1\IR\IRTypes.cpp"

$checks.Add((Add-Check "VS project includes IRConfig.cpp" ($vcxproj -match 'IR\\IRConfig\.cpp') "ConsoleApplication1.vcxproj"))
$checks.Add((Add-Check "VS project includes IRTypes.cpp" ($vcxproj -match 'IR\\IRTypes\.cpp') "ConsoleApplication1.vcxproj"))
$checks.Add((Add-Check "VS filters include IR folder" (($filters -match 'IR\\IRConfig\.h') -and ($filters -match 'IR\\IRTypes\.cpp')) "ConsoleApplication1.vcxproj.filters"))
$checks.Add((Add-Check "HwaSimIR logs Stage1 profile" ($hwa -match '\[Stage1\] Sensor profile') "startup/init/runtime profile logging"))
$checks.Add((Add-Check "Protocol 0 maps SWIR" ($types -match 'case 0:\s*return IRBand::ShortWaveInfrared') "trackerSensorBand=0"))
$checks.Add((Add-Check "Protocol 2 maps MWIR" ($types -match 'case 2:\s*return IRBand::MidWaveInfrared') "trackerSensorBand=2"))
$checks.Add((Add-Check "Protocol 3 maps LWIR" ($types -match 'case 3:\s*return IRBand::LongWaveInfrared') "trackerSensorBand=3"))
$checks.Add((Add-Check "Protocol 4 maps VIS" ($types -match 'case 4:\s*return IRBand::Visible') "trackerSensorBand=4"))

$sensorFiles = @{
    "VIS"  = "default_LLLTV.json";
    "NIR"  = "default_NVG.json";
    "SWIR" = "default_SWIR.json";
    "MWIR" = "default_MWIR.json";
    "LWIR" = "default_LWIR.json";
}

$sensorRows = New-Object System.Collections.Generic.List[object]
foreach ($band in @("VIS", "NIR", "SWIR", "MWIR", "LWIR")) {
    $file = $sensorFiles[$band]
    $relativePath = "ConsoleApplication1_LLA\Bin\Config\SensorWave\$file"
    $text = Read-Text $relativePath
    $ok = $null -ne $text
    if ($ok) {
        $low = Get-JsonNumber $text "SpectralResponseRangeLow"
        $high = Get-JsonNumber $text "SpectralResponseRangeHigh"
        $width = Get-JsonNumber $text "Width"
        $height = Get-JsonNumber $text "Height"
        $adc = Get-JsonNumber $text "ADCBitNumber"
        $display = Get-JsonNumber $text "DisplayBits"
        $netd = Get-JsonNumber $text "NoiseEquivalentTemperatureDifference"
        $ok = ($null -ne $low) -and ($null -ne $high) -and ($null -ne $width) -and ($null -ne $height) -and ($null -ne $adc) -and ($null -ne $display) -and ($null -ne $netd)
        $sensorRows.Add([PSCustomObject]@{
            Band = $band
            File = $file
            RangeUm = "$low-$high"
            Size = "${width}x${height}"
            ADCBits = $adc
            DisplayBits = $display
            NETD = $netd
        })
    }
    $checks.Add((Add-Check "SensorWave profile $band" $ok $relativePath))
}

Write-Host "Stage 1 sensor/config check"
Write-Host "Workspace: $rootPath"
Write-Host ""
$checks | Format-Table -AutoSize

Write-Host ""
Write-Host "Sensor profile matrix:"
$sensorRows | Format-Table -AutoSize

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
Write-Host "All Stage 1 checks passed."
