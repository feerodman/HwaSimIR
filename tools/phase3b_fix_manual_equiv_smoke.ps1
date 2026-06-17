param(
    [int]$Seconds = 30
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$phase2Smoke = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"
$runtimeCheck = Join-Path $PSScriptRoot "runtime_config_check.ps1"

if (-not (Test-Path -LiteralPath $phase2Smoke)) {
    throw "缺少依赖 smoke 脚本: $phase2Smoke"
}
if (-not (Test-Path -LiteralPath $runtimeCheck)) {
    throw "缺少生产配置检查脚本: $runtimeCheck"
}

function Get-NumericValues {
    param([string]$Text, [string]$Tag, [string]$Field)
    $pattern = "(?m)^\[" + [regex]::Escape($Tag) + "\].*?\b" +
        [regex]::Escape($Field) + "=([-+]?[0-9]+(?:\.[0-9]+)?)"
    return @([regex]::Matches($Text, $pattern) | ForEach-Object {
        [double]$_.Groups[1].Value
    })
}

function Get-Average {
    param([double[]]$Values)
    $usable = @($Values | Where-Object { $_ -ge 0.0 })
    if ($usable.Count -eq 0) { return 0.0 }
    return ($usable | Measure-Object -Average).Average
}

function Get-Maximum {
    param([double[]]$Values)
    if ($Values.Count -eq 0) { return 0.0 }
    return ($Values | Measure-Object -Maximum).Maximum
}

Write-Host "阶段 3B-Fix 手动等价 smoke"
Write-Host "步骤 1/3：检查生产运行配置"
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runtimeCheck

Write-Host "步骤 2/3：运行 $Seconds 秒 60 Hz 同步/录像 smoke"
$smokeOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $phase2Smoke -Seconds $Seconds 2>&1
$smokeText = ($smokeOutput | Out-String)
Write-Host $smokeText

if ($smokeText -notmatch "summary=(?<path>.+)") {
    throw "无法从 phase2a smoke 输出中定位 summary 路径"
}

$summaryPath = $Matches["path"].Trim()
if (-not (Test-Path -LiteralPath $summaryPath)) {
    throw "找不到 summary JSON: $summaryPath"
}
$summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
$hwaLog = Join-Path $summary.logDir "hwa.out.log"
$videoLog = Join-Path $summary.logDir "video.err.log"
if (-not (Test-Path -LiteralPath $hwaLog)) {
    throw "找不到 HwaSimIR 日志: $hwaLog"
}
$hwaText = Get-Content -LiteralPath $hwaLog -Raw -Encoding UTF8
$videoText = if (Test-Path -LiteralPath $videoLog) { Get-Content -LiteralPath $videoLog -Raw -Encoding UTF8 } else { "" }

$effectiveLines = @([regex]::Matches($hwaText, "(?m)^\[EffectiveRuntimeConfig\].*$") | ForEach-Object { $_.Value })
$warnLines = @([regex]::Matches($hwaText, "(?m)^\[EffectiveRuntimeConfig\]\[WARN\].*$") | ForEach-Object { $_.Value })
$lastEffective = if ($effectiveLines.Count -gt 0) { $effectiveLines[-1] } else { "" }
$modtranLookupValues = @(Get-NumericValues $hwaText "Perf" "stage5ModtranLookupMs")
$modtranLookupAvg = [math]::Round((Get-Average $modtranLookupValues), 6)
$modtranLookupMax = [math]::Round((Get-Maximum $modtranLookupValues), 6)
$componentValues = @(Get-NumericValues $hwaText "Perf" "stage5RadianceComponentMs")
$componentAvg = [math]::Round((Get-Average $componentValues), 6)

$effectiveOk = $lastEffective -match "DebugView=Off" -and
    $lastEffective -match "LogComponents=0" -and
    $lastEffective -match "EnableIRVerboseLog=0" -and
    $lastEffective -match "UseModtranPathRuntime=0" -and
    $lastEffective -match "UseModtranSkyRuntime=0" -and
    $lastEffective -match "UseModtranSolarRuntime=0" -and
    $lastEffective -match "Stage5ModtranCompareEffective=0" -and
    $lastEffective -match "JpegEncodeMode=rgb" -and
    $lastEffective -match "JpegQuality=100"

if (-not $effectiveOk) {
    throw "EffectiveRuntimeConfig 不是生产安全配置。最后一行: $lastEffective"
}
if ($modtranLookupMax -gt 0.001) {
    throw "生产默认不应执行 MODTRAN radiance lookup；max stage5ModtranLookupMs=$modtranLookupMax"
}

$manualChecklist = @(
    "确认手动启动的 HwaSim_IR.exe 是 Release: HwaSim_IR\\Bin\\HwaSim_IR.exe",
    "确认工作目录是 HwaSim_IR\\Bin，日志中的 runtimeConfigPath 指向 Config/HwaSimIRRuntime.ini",
    "确认没有旧 HwaSim_IR / VideoDisplay / DataDrivenTestQT 进程残留",
    "确认 [EffectiveRuntimeConfig] 中 DebugView=Off、LogComponents=0、EnableIRVerboseLog=0",
    "确认 UseModtranPathRuntime/UseModtranSkyRuntime/UseModtranSolarRuntime 均为 0",
    "确认 Stage5ModtranCompareEffective=0，且 [Perf] stage5ModtranLookupMs=0",
    "确认 JpegEncodeMode=rgb、JpegQuality=100、JpegPerfABTest=0、EnableH264Experimental=0",
    "确认 VideoDisplay saveMP4En 是异步录像且 RecorderPerf droppedFrames=0",
    "确认控制台没有 60 Hz 高频普通日志刷屏",
    "确认手动场景确实为 800x800、5 目标、videoFps=60，而不是 DebugView/overlay/smoke 配置"
)

$fixSummary = [pscustomobject]@{
    phase2SummaryPath = $summaryPath
    logDir = $summary.logDir
    outputMp4 = $summary.outputMp4
    sentFps = $summary.sentFps
    udpFps = $summary.udpFps
    renderFps = $summary.renderFps
    outputFps = $summary.outputFps
    videoReceiveFps = $summary.videoReceiveFps
    videoDisplayFps = $summary.videoDisplayFps
    latencyAvgMs = $summary.latencyAvgMs
    sourceSeqContinuous = $summary.sourceSeqContinuous
    inputQueueOverflow = $summary.inputQueueOverflow
    tcpOverwritten = $summary.tcpOverwritten
    recorderDroppedFrames = $summary.recorderDroppedFrames
    sourceSeqLagMax = $summary.sourceSeqLagMax
    inputQueueDepthMax = $summary.inputQueueDepthMax
    stage5ModtranLookupMsAvg = $modtranLookupAvg
    stage5ModtranLookupMsMax = $modtranLookupMax
    stage5RadianceComponentMsAvg = $componentAvg
    effectiveRuntimeConfigOk = $effectiveOk
    effectiveRuntimeConfigLast = $lastEffective
    effectiveRuntimeConfigWarnCount = $warnLines.Count
    effectiveRuntimeConfigWarnings = $warnLines
    manualDiffChecklist = $manualChecklist
}

$fixSummaryPath = Join-Path $summary.logDir "phase3b_fix_manual_equiv_summary.json"
$fixSummary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $fixSummaryPath -Encoding UTF8

Write-Host "步骤 3/3：手动等价测试摘要"
$fixSummary | Format-List
Write-Host "summary=$fixSummaryPath"
Write-Host "阶段 3B-Fix 手动等价 smoke 通过。" -ForegroundColor Green


