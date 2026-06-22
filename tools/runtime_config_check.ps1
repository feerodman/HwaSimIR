param(
    [string]$RuntimeIni = ""
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($RuntimeIni)) {
    $RuntimeIni = Join-Path $root.Path "HwaSim_IR\Bin\Config\HwaSimIRRuntime.ini"
}
if (-not (Test-Path -LiteralPath $RuntimeIni)) {
    throw "Runtime ini not found: $RuntimeIni"
}

function ConvertTo-IniMap {
    param([string]$Path)

    $map = @{}
    $section = ""
    foreach ($rawLine in [IO.File]::ReadAllLines($Path)) {
        $line = $rawLine.Trim()
        if ($line.Length -eq 0 -or $line.StartsWith(";") -or $line.StartsWith("#")) {
            continue
        }
        if ($line.StartsWith("[") -and $line.EndsWith("]")) {
            $section = $line.Substring(1, $line.Length - 2).Trim().ToLowerInvariant()
            continue
        }
        $eq = $line.IndexOf("=")
        if ($eq -lt 0) {
            continue
        }
        $key = $line.Substring(0, $eq).Trim().ToLowerInvariant()
        $value = $line.Substring($eq + 1).Trim()
        foreach ($marker in @(";", "#")) {
            $comment = $value.IndexOf($marker)
            if ($comment -ge 0) {
                $value = $value.Substring(0, $comment).Trim()
            }
        }
        if ($section.Length -gt 0 -and $key.Length -gt 0) {
            $map["$section.$key"] = $value
        }
    }
    return $map
}

function Get-IniValue {
    param($Map, [string]$Section, [string]$Key, [string]$Default = "")
    $fullKey = ($Section.ToLowerInvariant() + "." + $Key.ToLowerInvariant())
    if ($Map.ContainsKey($fullKey)) {
        return [string]$Map[$fullKey]
    }
    return $Default
}

function Test-FalseValue {
    param([string]$Value)
    $v = $Value.Trim().ToLowerInvariant()
    return ($v -eq "0" -or $v -eq "false" -or $v -eq "no" -or $v -eq "off")
}

function Test-TrueValue {
    param([string]$Value)
    $v = $Value.Trim().ToLowerInvariant()
    return ($v -eq "1" -or $v -eq "true" -or $v -eq "yes" -or $v -eq "on")
}

function Assert-False {
    param($Map, [string]$Section, [string]$Key, [string]$Default = "false")
    $value = Get-IniValue $Map $Section $Key $Default
    if (-not (Test-FalseValue $value)) {
        throw "Production runtime config must keep [$Section] $Key=false/off/0, got '$value'"
    }
    [pscustomobject]@{ Check = "[$Section] $Key"; Status = "OK"; Value = $value }
}

function Assert-True {
    param($Map, [string]$Section, [string]$Key, [string]$Default = "true")
    $value = Get-IniValue $Map $Section $Key $Default
    if (-not (Test-TrueValue $value)) {
        throw "Production runtime config must keep [$Section] $Key=true/on/1, got '$value'"
    }
    [pscustomobject]@{ Check = "[$Section] $Key"; Status = "OK"; Value = $value }
}

function Assert-Equals {
    param($Map, [string]$Section, [string]$Key, [string]$Expected, [string]$Default = "")
    $value = Get-IniValue $Map $Section $Key $Default
    if ($value.Trim().ToLowerInvariant() -ne $Expected.Trim().ToLowerInvariant()) {
        throw "Production runtime config must keep [$Section] $Key=$Expected, got '$value'"
    }
    [pscustomobject]@{ Check = "[$Section] $Key"; Status = "OK"; Value = $value }
}

$ini = ConvertTo-IniMap $RuntimeIni
$checks = @()
$checks += Assert-False $ini "Performance" "EnableIRVerboseLog" "0"
$checks += Assert-Equals $ini "Stage5Radiance" "DebugView" "Off" "Off"
$checks += Assert-False $ini "Stage5Radiance" "LogComponents" "false"
$checks += Assert-False $ini "Stage5Radiance" "UseSensorInputForDisplay" "false"
$checks += Assert-Equals $ini "Stage5Radiance" "SensorInputDisplayMode" "Manual" "Manual"
$checks += Assert-Equals $ini "Stage5Radiance" "SensorInputDisplayScale" "1.0" "1.0"
$checks += Assert-Equals $ini "Stage5Radiance" "SensorInputDisplayOffset" "0.0" "0.0"
$checks += Assert-Equals $ini "Stage5Radiance" "SensorInputDisplayClampMin" "0.0" "0.0"
$checks += Assert-Equals $ini "Stage5Radiance" "SensorInputDisplayClampMax" "1.0" "1.0"
$checks += Assert-Equals $ini "Stage5Radiance" "SensorInputDisplayGamma" "1.0" "1.0"
$checks += Assert-Equals $ini "Stage5Radiance" "SensorInputDisplayBand" "MWIR" "MWIR"
$checks += Assert-False $ini "Stage5ModtranRadiance" "EnableModtranRadianceDebug" "false"
$checks += Assert-False $ini "Stage5ModtranRadiance" "UseModtranPathRuntime" "false"
$checks += Assert-False $ini "Stage5ModtranRadiance" "UseModtranSkyRuntime" "false"
$checks += Assert-False $ini "Stage5ModtranRadiance" "UseModtranSolarRuntime" "false"
$checks += Assert-False $ini "Stage5ModtranRadiance" "CompareLegacy" "false"
$checks += Assert-Equals $ini "Stage5ModtranRadiance" "ModtranPathRuntimeBand" "MWIR" "MWIR"
$checks += Assert-Equals $ini "Stage5ModtranRadiance" "ModtranPathRuntimeMode" "Off" "Off"
$checks += Assert-Equals $ini "Stage5ModtranRadiance" "ModtranPathUnitMode" "Native" "Native"
$checks += Assert-Equals $ini "Stage5ModtranRadiance" "ModtranPathScale" "1.0" "1.0"
$checks += Assert-Equals $ini "Stage5ModtranRadiance" "ModtranPathOffset" "0.0" "0.0"
$checks += Assert-Equals $ini "Stage5ModtranRadiance" "ModtranPathClampMin" "0.0" "0.0"
$checks += Assert-Equals $ini "Stage5ModtranRadiance" "ModtranPathClampMax" "10.0" "10.0"
$checks += Assert-Equals $ini "Stage5ModtranRadiance" "ModtranPathBlend" "1.0" "1.0"
$checks += Assert-True $ini "Stage5ModtranRadiance" "ModtranPathABLog" "true"
$checks += Assert-False $ini "TcpOutput" "EnableH264Experimental" "false"
$checks += Assert-False $ini "TcpOutput" "JpegPerfABTest" "false"
$checks += Assert-Equals $ini "TcpOutput" "JpegEncodeMode" "rgb" "rgb"
$checks += Assert-Equals $ini "TcpOutput" "JpegQuality" "100" "100"
$checks += Assert-False $ini "Stage4" "LegacyEngineBodyHeating" "false"
$checks += Assert-True $ini "Stage5AeroThermal" "EnableAeroThermalModel" "true"
$checks += Assert-False $ini "Stage5AeroThermal" "ApplyAeroToRadiance" "false"
$checks += Assert-False $ini "Stage5AeroThermal" "AeroDebugLog" "false"
$checks += Assert-Equals $ini "Stage5AeroThermal" "RecoveryFactor" "0.85" "0.85"
$checks += Assert-Equals $ini "Stage5AeroThermal" "Gamma" "1.4" "1.4"
$checks += Assert-Equals $ini "Stage5AeroThermal" "BodyCoeff" "0.20" "0.20"
$checks += Assert-Equals $ini "Stage5AeroThermal" "NoseCoeff" "0.60" "0.60"
$checks += Assert-Equals $ini "Stage5AeroThermal" "EdgeCoeff" "0.45" "0.45"
$checks += Assert-Equals $ini "Stage5AeroThermal" "RearCoeff" "0.10" "0.10"
$checks += Assert-Equals $ini "Stage5AeroThermal" "HeatTauSec" "2.0" "2.0"
$checks += Assert-Equals $ini "Stage5AeroThermal" "CoolTauSec" "5.0" "5.0"
$checks += Assert-Equals $ini "Stage5AeroThermal" "ClampMachMin" "0.0" "0.0"
$checks += Assert-Equals $ini "Stage5AeroThermal" "ClampMachMax" "4.0" "4.0"
$checks += Assert-Equals $ini "Stage5AeroThermal" "ClampDeltaKMax" "250.0" "250.0"
$checks += Assert-Equals $ini "Stage5AeroThermal" "AeroApplyScale" "0.25" "0.25"
$checks += Assert-Equals $ini "Stage5AeroThermal" "AeroApplyClampBodyDeltaK" "40.0" "40.0"
$checks += Assert-Equals $ini "Stage5AeroThermal" "AeroApplyOnlyBand" "MWIR" "MWIR"

$legacyStage5Debug = Get-IniValue $ini "Stage5" "EnableRadianceDebug" "false"
if (Test-TrueValue $legacyStage5Debug) {
    throw "Production runtime config must keep legacy [Stage5] EnableRadianceDebug=false/off/0, got '$legacyStage5Debug'"
}
$checks += [pscustomobject]@{ Check = "[Stage5] EnableRadianceDebug"; Status = "OK"; Value = $legacyStage5Debug }

$checks | Format-Table -AutoSize
Write-Host "Runtime config production check passed: $RuntimeIni"
