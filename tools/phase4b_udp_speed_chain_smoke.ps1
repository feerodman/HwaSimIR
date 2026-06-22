param(
    [int]$Seconds = 10,
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage5\aero_speed_chain_summary.csv"
}
$outDir = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$runner = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"

function Convert-ToDouble {
    param($Value)
    if ($null -eq $Value) { return 0.0 }
    $text = [string]$Value
    $parsed = 0.0
    if ([double]::TryParse($text, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$parsed)) {
        return $parsed
    }
    return 0.0
}

function Convert-ToInt {
    param($Value)
    return [int](Convert-ToDouble $Value)
}

function Normalize-TargetType {
    param($Value)
    if ($null -eq $Value) { return "" }
    $text = [string]$Value
    $match = [regex]::Match($text, "0[xX]([0-9A-Fa-f]{2})")
    if (-not $match.Success) { return "" }
    return "0x" + $match.Groups[1].Value.ToLowerInvariant()
}

function Get-TaggedRows {
    param([string]$Text, [string]$Tag)
    $rows = @()
    $pattern = "(?m)^\[" + [regex]::Escape($Tag) + "\]\s.*$"
    foreach ($match in [regex]::Matches($Text, $pattern)) {
        $line = $match.Value
        $dict = @{ rawLine = $line }
        foreach ($kv in [regex]::Matches($line, "(?<key>[A-Za-z0-9_.]+)=(?<value>\S+)")) {
            $dict[$kv.Groups["key"].Value] = $kv.Groups["value"].Value
        }
        $rows += [pscustomobject]$dict
    }
    return $rows
}

function New-ExactMap {
    param([object[]]$Rows)
    $map = @{}
    foreach ($row in $Rows) {
        $key = "$(Convert-ToInt $row.sourceSeq)|$(Convert-ToInt $row.targetID)|$(Normalize-TargetType $row.targetType)"
        if (-not $map.ContainsKey($key)) {
            $map[$key] = $row
        }
    }
    return $map
}

function New-CoarseMap {
    param([object[]]$Rows)
    $map = @{}
    foreach ($row in $Rows) {
        $key = "$(Convert-ToInt $row.targetID)|$(Normalize-TargetType $row.targetType)"
        $map[$key] = $row
    }
    return $map
}

function Resolve-Row {
    param($BaseRow, $ExactMap, $CoarseMap)
    $exactKey = "$(Convert-ToInt $BaseRow.sourceSeq)|$(Convert-ToInt $BaseRow.targetID)|$(Normalize-TargetType $BaseRow.targetType)"
    if ($ExactMap.ContainsKey($exactKey)) { return $ExactMap[$exactKey] }
    return $null
}

function Assert-AnyNonZero {
    param([object[]]$Rows, [string]$Field, [string]$Name)
    $nonZero = @($Rows | Where-Object { (Convert-ToDouble $_.$Field) -gt 0.0 })
    if ($nonZero.Count -eq 0) {
        throw "Phase4B speed chain failed at ${Name}: no non-zero $Field"
    }
}

function Test-ValidFlagToken {
    param($Value)
    if ($null -eq $Value) { return $false }
    $text = ([string]$Value).ToLowerInvariant()
    return $text -eq "0" -or $text -eq "1" -or $text -eq "true" -or $text -eq "false"
}

Write-Host "Running Phase4B UDP speed chain smoke seconds=$Seconds ..."
$runOutput = & $runner -Seconds $Seconds `
    -ApplyAeroToRadiance "false" `
    -AeroDebugLog "true" `
    -Stage5LogComponents "false" `
    -Stage5ComponentLogEveryFrames 120
$summaryLine = @($runOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
if ($summaryLine.Count -eq 0) {
    $runOutput | Out-String | Write-Host
    throw "Phase4B speed chain did not produce a summary path"
}
$summaryPath = $summaryLine[-1].Substring("summary=".Length)
$summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
$stimText = Get-Content -LiteralPath (Join-Path $summary.logDir "stim.err.log") -Raw
$hwaText = Get-Content -LiteralPath (Join-Path $summary.logDir "hwa.out.log") -Raw

$sendRows = Get-TaggedRows $stimText "AeroSpeedSend"
$recvRows = Get-TaggedRows $hwaText "AeroSpeedRecv"
$stateRows = Get-TaggedRows $hwaText "AeroSpeedState"
$aeroRows = Get-TaggedRows $hwaText "Stage5 AeroThermal"
$aeroRows = @($aeroRows | Where-Object {
    (Convert-ToInt $_.sourceSeq) -gt 0 -and
    (Convert-ToInt $_.targetID) -gt 0 -and
    -not [string]::IsNullOrWhiteSpace((Normalize-TargetType $_.targetType)) -and
    -not [string]::IsNullOrWhiteSpace([string]$_.selectedSpeedSource) -and
    (Convert-ToDouble $_.speedRawKmh) -gt 0.0 -and
    (Convert-ToDouble $_.mach) -gt 0.0 -and
    (Test-ValidFlagToken $_.valid)
})

if ($sendRows.Count -eq 0) { throw "Phase4B speed chain failed: no [AeroSpeedSend] rows" }
if ($recvRows.Count -eq 0) { throw "Phase4B speed chain failed: no [AeroSpeedRecv] rows" }
if ($stateRows.Count -eq 0) { throw "Phase4B speed chain failed: no [AeroSpeedState] rows" }
if ($aeroRows.Count -eq 0) { throw "Phase4B speed chain failed: no [Stage5 AeroThermal] rows" }
Assert-AnyNonZero $sendRows "speedRawKmh" "DataDrivenTestQT send"
Assert-AnyNonZero $recvRows "receivedSpeedRaw" "HwaSim_IR recv"
Assert-AnyNonZero $stateRows "selectedSpeedKmh" "HwaSim_IR TargetState"
Assert-AnyNonZero $aeroRows "mach" "Stage5 AeroThermal"

$sendExact = New-ExactMap $sendRows
$recvExact = New-ExactMap $recvRows
$stateExact = New-ExactMap $stateRows
$sendCoarse = New-CoarseMap $sendRows
$recvCoarse = New-CoarseMap $recvRows
$stateCoarse = New-CoarseMap $stateRows

$rows = New-Object System.Collections.Generic.List[object]
foreach ($aero in $aeroRows) {
    $send = Resolve-Row $aero $sendExact $sendCoarse
    $recv = Resolve-Row $aero $recvExact $recvCoarse
    $state = Resolve-Row $aero $stateExact $stateCoarse
    $sendSpeed = if ($send) { Convert-ToDouble $send.speedRawKmh } else { 0.0 }
    $recvSpeed = if ($recv) { Convert-ToDouble $recv.receivedSpeedRaw } else { 0.0 }
    $stateSpeed = if ($state) { Convert-ToDouble $state.selectedSpeedKmh } else { 0.0 }
    $breakpoint = "ok"
    if ($sendSpeed -le 0.0) { $breakpoint = "send_zero_or_missing" }
    elseif ($recvSpeed -le 0.0) { $breakpoint = "recv_zero_or_missing" }
    elseif ($stateSpeed -le 0.0) { $breakpoint = "state_zero_or_missing" }
    elseif ((Convert-ToDouble $aero.mach) -le 0.0) { $breakpoint = "stage5_mach_zero" }
    $rows.Add([pscustomobject]@{
        sourceSeq = Convert-ToInt $aero.sourceSeq
        targetID = Convert-ToInt $aero.targetID
        targetType = Normalize-TargetType $aero.targetType
        sendSpeedKmh = [math]::Round($sendSpeed, 6)
        recvSpeedKmh = [math]::Round($recvSpeed, 6)
        stateSpeedKmh = [math]::Round($stateSpeed, 6)
        selectedSpeedSource = [string]$aero.selectedSpeedSource
        altitudeM = [math]::Round((Convert-ToDouble $aero.altitudeM), 6)
        mach = [math]::Round((Convert-ToDouble $aero.mach), 9)
        bodyAeroDeltaK = [math]::Round((Convert-ToDouble $aero.bodyAeroDeltaK), 9)
        valid = [string]$aero.valid
        fallbackReason = [string]$aero.fallbackReason
        breakpoint = $breakpoint
    }) | Out-Null
}

$rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding UTF8
$okRows = @($rows | Where-Object { $_.breakpoint -eq "ok" })
if ($okRows.Count -eq 0) {
    $rows | Format-Table -AutoSize
    throw "Phase4B speed chain failed: no fully joined non-zero Send/Recv/State/Stage5 row, see $OutputPath"
}
$bad = @($rows | Where-Object { $_.breakpoint -ne "ok" })
if ($bad.Count -gt 0) {
    Write-Warning "Ignored $($bad.Count) unmatched/corrupted console rows; see breakpoint column in $OutputPath"
}

$okRows | Select-Object -First 12 | Format-Table -AutoSize
Write-Host "Phase4B UDP speed chain smoke passed: $OutputPath"
