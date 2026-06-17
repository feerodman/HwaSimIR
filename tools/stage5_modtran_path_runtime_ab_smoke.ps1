param(
    [double]$Scale = 1.0,
    [double]$Blend = 0.25,
    [switch]$SkipRuntime
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$lutPath = Join-Path $rootPath "HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\band_lut.csv"
$phase2Smoke = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"
$runtimeCheck = Join-Path $PSScriptRoot "runtime_config_check.ps1"
$logDir = Join-Path $rootPath "logs\stage5"
$unitCsv = Join-Path $logDir "modtran_path_runtime_ab_summary.csv"
$runtimeCsv = Join-Path $logDir "modtran_path_runtime_ab_runtime_summary.csv"

foreach ($path in @($lutPath, $phase2Smoke, $runtimeCheck)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "缺少依赖路径: $path"
    }
}
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

function Get-Double {
    param($Row, [string]$Name, [double]$Default = [double]::NaN)
    $value = $Row.$Name
    if ([string]::IsNullOrWhiteSpace($value)) { return $Default }
    return [double]::Parse([string]$value, [Globalization.CultureInfo]::InvariantCulture)
}

function Get-NormalizedBlackbody {
    param([double]$TemperatureK)
    $lowUm = 3.0
    $highUm = 5.0
    $samples = 12
    $c1 = 1.191042e8
    $c2 = 1.4387752e4
    function Integrate {
        param([double]$temp)
        $sum = 0.0
        for ($i = 0; $i -lt $samples; $i++) {
            $t = ([double]$i + 0.5) / [double]$samples
            $lambda = $lowUm + ($highUm - $lowUm) * $t
            $den = [Math]::Pow($lambda, 5.0) * ([Math]::Exp($c2 / ($lambda * $temp)) - 1.0)
            if ($den -gt 0.0) { $sum += $c1 / $den }
        }
        return $sum / [double]$samples
    }
    $reference = Integrate 760.0
    if ($reference -le 0.0) { return 0.0 }
    $value = (Integrate $TemperatureK) / $reference
    return [Math]::Max(0.0, [Math]::Min(1.5, $value))
}

function Get-LegacyPath {
    param([double]$TauUp, [double]$EnvTempK = 288.15, [double]$HumidityPercent = 50.0)
    $humidityBoost = [Math]::Max(0.0, [Math]::Min(100.0, $HumidityPercent)) / 100.0
    $envRadiance = Get-NormalizedBlackbody $EnvTempK
    return (1.0 - $TauUp) * (0.04 + $envRadiance * (0.16 + 0.12 * $humidityBoost))
}

function Get-Distance {
    param($Row, [double]$RangeKm, [double]$ObsAltKm, [double]$TgtAltKm, [double]$VisibilityKm)
    $obs = ((Get-Double $Row "observer_alt_km") - $ObsAltKm) / 20.0
    $tgt = ((Get-Double $Row "target_alt_km") - $TgtAltKm) / 20.0
    $range = ((Get-Double $Row "range_km") - $RangeKm) / 50.0
    $vis = ((Get-Double $Row "visibility_km") - $VisibilityKm) / 50.0
    $sza = ((Get-Double $Row "solar_zenith_deg") - 45.0) / 90.0
    return $obs * $obs + $tgt * $tgt + $range * $range + $vis * $vis + $sza * $sza
}

function Invoke-LutQuery {
    param([string]$Band, [double]$RangeKm, [double]$ObsAltKm, [double]$TgtAltKm, [double]$VisibilityKm, $Rows)
    $result = [ordered]@{
        band = $Band
        rangeKm = $RangeKm
        obsAltKm = $ObsAltKm
        tgtAltKm = $TgtAltKm
        visibilityKm = $VisibilityKm
        legacyPath = 0.0
        modtranPathRaw = 0.0
        modtranPathScaled = 0.0
        suggestedScaleToLegacy = 0.0
        ratioRawToLegacy = 0.0
        tauUp = 1.0
        valid = 0
        fallbackReason = "unknown"
        interpolationMode = "none"
    }
    if ($RangeKm -le 0.0 -or $VisibilityKm -le 0.0 -or $RangeKm + 1.0e-6 -lt [Math]::Abs($ObsAltKm - $TgtAltKm)) {
        $result.fallbackReason = "invalid_geometry"
        return [pscustomobject]$result
    }
    $bandRows = @($Rows | Where-Object { $_.band -eq $Band })
    if ($bandRows.Count -eq 0) {
        $result.fallbackReason = "missing_band"
        return [pscustomobject]$result
    }
    $bounds = [pscustomobject]@{
        minObs = ($bandRows | ForEach-Object { Get-Double $_ "observer_alt_km" } | Measure-Object -Minimum).Minimum
        maxObs = ($bandRows | ForEach-Object { Get-Double $_ "observer_alt_km" } | Measure-Object -Maximum).Maximum
        minTgt = ($bandRows | ForEach-Object { Get-Double $_ "target_alt_km" } | Measure-Object -Minimum).Minimum
        maxTgt = ($bandRows | ForEach-Object { Get-Double $_ "target_alt_km" } | Measure-Object -Maximum).Maximum
        minRange = ($bandRows | ForEach-Object { Get-Double $_ "range_km" } | Measure-Object -Minimum).Minimum
        maxRange = ($bandRows | ForEach-Object { Get-Double $_ "range_km" } | Measure-Object -Maximum).Maximum
        minVis = ($bandRows | ForEach-Object { Get-Double $_ "visibility_km" } | Measure-Object -Minimum).Minimum
        maxVis = ($bandRows | ForEach-Object { Get-Double $_ "visibility_km" } | Measure-Object -Maximum).Maximum
    }
    if ($ObsAltKm -lt $bounds.minObs -or $ObsAltKm -gt $bounds.maxObs -or
        $TgtAltKm -lt $bounds.minTgt -or $TgtAltKm -gt $bounds.maxTgt -or
        $RangeKm -lt $bounds.minRange -or $RangeKm -gt $bounds.maxRange -or
        $VisibilityKm -lt $bounds.minVis -or $VisibilityKm -gt $bounds.maxVis) {
        $result.fallbackReason = "out_of_lut_range"
        return [pscustomobject]$result
    }
    $best = $null
    $bestDistance = [double]::MaxValue
    foreach ($row in $bandRows) {
        $distance = Get-Distance $row $RangeKm $ObsAltKm $TgtAltKm $VisibilityKm
        if ($distance -lt $bestDistance) {
            $bestDistance = $distance
            $best = $row
        }
    }
    if ($null -eq $best) {
        $result.fallbackReason = "missing_band"
        return [pscustomobject]$result
    }
    $tau = [Math]::Max(1.0e-6, [Math]::Min(1.0, (Get-Double $best "tau_up_band" 1.0)))
    $raw = [Math]::Max(0.0, (Get-Double $best "path_radiance_band" 0.0))
    $legacy = Get-LegacyPath $tau
    $scaled = [Math]::Max(0.0, [Math]::Min(10.0, $raw * $Scale))
    $result.legacyPath = $legacy
    $result.modtranPathRaw = $raw
    $result.modtranPathScaled = $scaled
    $result.suggestedScaleToLegacy = if ($raw -gt 0.0) { $legacy / $raw } else { 0.0 }
    $result.ratioRawToLegacy = if ($legacy -gt 0.0) { $raw / $legacy } else { 0.0 }
    $result.tauUp = $tau
    $result.valid = 1
    $result.fallbackReason = if ($bestDistance -gt 1.0e-10) { "nearest_neighbor" } else { "none" }
    $result.interpolationMode = if ($bestDistance -gt 1.0e-10) { "nearest_neighbor" } else { "exact_match" }
    return [pscustomobject]$result
}

Write-Host "阶段 3C：MODTRAN MWIR path runtime 单元标定"
$allRows = Import-Csv -LiteralPath $lutPath | Where-Object {
    $_.atmosphere_model -eq "Mid-Latitude Summer" -and
    $_.aerosol_model -eq "Rural" -and
    $_.humidity_profile -eq "default"
}
$unitRows = New-Object System.Collections.Generic.List[object]
foreach ($range in @(5, 20, 50)) {
    foreach ($obs in @(3, 10, 20)) {
        foreach ($tgt in @(3, 10, 20)) {
            foreach ($vis in @(5, 15, 30)) {
                $unitRows.Add((Invoke-LutQuery "MWIR" $range $obs $tgt $vis $allRows))
            }
        }
    }
}
$unitRows.Add((Invoke-LutQuery "SWIR" 20 10 10 15 $allRows))
$unitRows.Add((Invoke-LutQuery "MWIR" 80 10 10 15 $allRows))
$unitRows | Export-Csv -LiteralPath $unitCsv -NoTypeInformation -Encoding UTF8

$validScales = @($unitRows | Where-Object { $_.valid -eq 1 -and $_.suggestedScaleToLegacy -gt 0 } | ForEach-Object { [double]$_.suggestedScaleToLegacy })
$scaleMin = if ($validScales.Count) { ($validScales | Measure-Object -Minimum).Minimum } else { 0.0 }
$scaleMax = if ($validScales.Count) { ($validScales | Measure-Object -Maximum).Maximum } else { 0.0 }
$scaleAvg = if ($validScales.Count) { ($validScales | Measure-Object -Average).Average } else { 0.0 }

Write-Host "单元标定 CSV: $unitCsv"
Write-Host ("suggestedScaleToLegacy 范围: min={0:E3} avg={1:E3} max={2:E3}" -f $scaleMin, $scaleAvg, $scaleMax)

if ($SkipRuntime) {
    Write-Host "已跳过 runtime A/B。"
    return
}

Write-Host "阶段 3C：运行 Off / CompareOnly / BlendLegacy / ReplaceLegacy runtime A/B"
$cases = @(
    [pscustomobject]@{ name="Off"; seconds=10; usePath="false"; mode="Off"; scale=1.0; blend=1.0 },
    [pscustomobject]@{ name="CompareOnly"; seconds=10; usePath="false"; mode="CompareOnly"; scale=1.0; blend=1.0 },
    [pscustomobject]@{ name="BlendLegacy"; seconds=10; usePath="true"; mode="BlendLegacy"; scale=$Scale; blend=$Blend },
    [pscustomobject]@{ name="ReplaceLegacy"; seconds=10; usePath="true"; mode="ReplaceLegacy"; scale=$Scale; blend=1.0 }
)
$runtimeRows = New-Object System.Collections.Generic.List[object]
foreach ($case in $cases) {
    Write-Host ("运行 A/B 组: {0}" -f $case.name)
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $phase2Smoke `
        -Seconds $case.seconds `
        -UseModtranPathRuntime $case.usePath `
        -ModtranPathRuntimeMode $case.mode `
        -ModtranPathScale $case.scale `
        -ModtranPathBlend $case.blend 2>&1
    $text = ($output | Out-String)
    Write-Host $text
    if ($text -notmatch "summary=(?<path>.+)") {
        throw "无法定位 $($case.name) 的 summary 路径"
    }
    $summaryPath = $Matches["path"].Trim()
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    $hwaLog = Join-Path $summary.logDir "hwa.out.log"
    $hwaText = Get-Content -LiteralPath $hwaLog -Raw -Encoding UTF8
    $pathSources = @([regex]::Matches($hwaText, "(?m)^\[Stage5 ModtranPathAB\].*?\bpathRadianceSource=(?<source>\S+)") | ForEach-Object { $_.Groups["source"].Value })
    $lastSource = if ($pathSources.Count -gt 0) { $pathSources[-1] } else { "not_logged" }
    $perfFailures = @()
    if ([double]$summary.sentFps -lt 59.5) { $perfFailures += "sentFps" }
    if ([double]$summary.udpFps -lt 59.5) { $perfFailures += "udpFps" }
    if ([double]$summary.renderFps -lt 59.5) { $perfFailures += "renderFps" }
    if ([double]$summary.outputFps -lt 59.5) { $perfFailures += "outputFps" }
    if ([double]$summary.videoReceiveFps -lt 59.5) { $perfFailures += "videoReceiveFps" }
    if ([double]$summary.videoDisplayFps -lt 59.5) { $perfFailures += "videoDisplayFps" }
    if ([double]$summary.latencyAvgMs -gt 80.0) { $perfFailures += "latencyAvgMs" }
    if ([int]$summary.sourceSeqContinuous -ne 1) { $perfFailures += "sourceSeqContinuous" }
    if ([int]$summary.inputQueueOverflow -ne 0) { $perfFailures += "inputQueueOverflow" }
    if ([int]$summary.tcpOverwritten -ne 0) { $perfFailures += "tcpOverwritten" }
    if ([int]$summary.recorderDroppedFrames -ne 0) { $perfFailures += "recorderDroppedFrames" }
    if ([int]$summary.sourceSeqLagMax -gt 1) { $perfFailures += "sourceSeqLagMax" }
    if ([int]$summary.inputQueueDepthMax -gt 2) { $perfFailures += "inputQueueDepthMax" }
    $runtimeRows.Add([pscustomobject]@{
        case = $case.name
        mode = $case.mode
        useModtranPathRuntime = $case.usePath
        scale = $case.scale
        blend = $case.blend
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        videoReceiveFps = $summary.videoReceiveFps
        videoDisplayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        sourceSeqContinuous = $summary.sourceSeqContinuous
        sourceSeqLagMax = $summary.sourceSeqLagMax
        inputQueueDepthMax = $summary.inputQueueDepthMax
        inputQueueOverflow = $summary.inputQueueOverflow
        tcpOverwritten = $summary.tcpOverwritten
        recorderDroppedFrames = $summary.recorderDroppedFrames
        stage5ModtranLookupMs = $summary.stage5ModtranLookupMs
        stage5RadianceComponentMs = $summary.stage5RadianceComponentMs
        jpegMsAvg = $summary.jpegMsAvg
        readbackMsAvg = $summary.readbackMsAvg
        pathRadianceSource = $lastSource
        performancePassed = ($perfFailures.Count -eq 0)
        performanceFailures = ($perfFailures -join ";")
        summaryPath = $summaryPath
        logDir = $summary.logDir
    })
}
$runtimeRows | Export-Csv -LiteralPath $runtimeCsv -NoTypeInformation -Encoding UTF8
Write-Host "Runtime A/B CSV: $runtimeCsv"
$failedRows = @($runtimeRows | Where-Object { -not $_.performancePassed })
if ($failedRows.Count -gt 0) {
    Write-Warning ("阶段 3C A/B 存在未达 60 Hz 验收的组: " + (($failedRows | ForEach-Object { "$($_.case)[$($_.performanceFailures)]" }) -join ", "))
}
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runtimeCheck
Write-Host "阶段 3C MODTRAN path runtime A/B smoke 完成。"

