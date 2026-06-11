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

function Test-WorkspacePath {
    param([string]$RelativePath)
    return Test-Path -LiteralPath (Join-Path $rootPath $RelativePath)
}

$checks = New-Object System.Collections.Generic.List[object]

$requiredPaths = @(
    "temperatures\Temperatures_Yemen_Summer.csv",
    "transmittance\transmittance_0.3_15.txt",
    "HwaSim_IR\HwaSim_IR\IRSimulation.h",
    "HwaSim_IR\HwaSim_IR\IRSimulation.cpp",
    "HwaSim_IR\HwaSim_IR\HwaSimIR.h",
    "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
)

foreach ($relativePath in $requiredPaths) {
    $checks.Add((Add-Check "Stage3 file/resource" (Test-WorkspacePath $relativePath) $relativePath))
}

$xlsxCount = @(Get-ChildItem -LiteralPath (Join-Path $rootPath "temperatures") -Filter "*.xlsx" -File -ErrorAction SilentlyContinue).Count
$checks.Add((Add-Check "Solar position workbook exists" ($xlsxCount -gt 0) "temperatures/*.xlsx"))

$irHeader = Read-Text "HwaSim_IR\HwaSim_IR\IRSimulation.h"
$irSource = Read-Text "HwaSim_IR\HwaSim_IR\IRSimulation.cpp"
$hwaHeader = Read-Text "HwaSim_IR\HwaSim_IR\HwaSimIR.h"
$hwaSource = Read-Text "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$weatherCsv = Read-Text "temperatures\Temperatures_Yemen_Summer.csv"

$checks.Add((Add-Check "Weather profile class exists" ($irHeader -match 'class IRWeatherProfile' -and $irHeader -match 'IRWeatherSample') "IRSimulation.h"))
$checks.Add((Add-Check "Runtime environment expanded" ($irHeader -match 'humidityPercent' -and $irHeader -match 'sunAzimuthDeg' -and $irHeader -match 'simulationHour') "IRRuntimeEnvironment"))
$checks.Add((Add-Check "Weather CSV loader implemented" ($irSource -match 'IRWeatherProfile::load' -and $irSource -match 'sampleForHour') "IRSimulation.cpp"))
$checks.Add((Add-Check "Visibility affects transmittance" ($irSource -match 'visibilityMeters' -and $irSource -match 'visibilityScale') "IRAtmosphereModel"))
$checks.Add((Add-Check "HwaSimIR loads weather profile" ($hwaSource -match 'Temperatures_Yemen_Summer\.csv' -and $hwaHeader -match 'm_irWeatherProfile') "InitInfraredSimulation"))
$checks.Add((Add-Check "UDP overrides profile" ($hwaSource -match 'envTemp' -and $hwaSource -match 'envVisibility' -and $hwaSource -match 'envHumidity' -and $hwaSource -match 'envWindV') "BuildRuntimeEnvironment"))
$checks.Add((Add-Check "Realtime time drives hour" ($hwaSource -match 'CurrentSimulationHour' -and $hwaSource -match 'm_realTimeSceneData\.time') "CurrentSimulationHour"))
$checks.Add((Add-Check "Stage3 environment logging" ($hwaSource -match '\[Stage3\] Environment') "LogActiveIREnvironment"))

$hourlyRows = @()
if ($null -ne $weatherCsv) {
    $lines = $weatherCsv -split "`r?`n"
    $inHourly = $false
    foreach ($line in $lines) {
        if ($line -match '^Time\s*,') {
            $inHourly = $true
            continue
        }
        if ($inHourly -and $line.Trim().Length -gt 0) {
            $parts = $line -split ','
            if ($parts.Count -ge 4) {
                $hourlyRows += [PSCustomObject]@{
                    Hour = [double]$parts[0]
                    TempC = [double]$parts[1]
                    SunAz = [double]$parts[2]
                    SunEl = [double]$parts[3]
                }
            }
        }
    }
}
$checks.Add((Add-Check "Weather CSV has hourly rows" ($hourlyRows.Count -ge 24) "Temperatures_Yemen_Summer.csv"))

Write-Host "Stage 3 atmosphere/environment check"
Write-Host "Workspace: $rootPath"
Write-Host ""
$checks | Format-Table -AutoSize

if ($hourlyRows.Count -gt 0) {
    Write-Host ""
    Write-Host "Weather profile samples:"
    $hourlyRows | Select-Object -First 6 | Format-Table -AutoSize
}

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
Write-Host "All Stage 3 checks passed."
