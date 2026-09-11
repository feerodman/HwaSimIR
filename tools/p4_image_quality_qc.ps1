[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string[]]$ImagePath,
    [int]$RoiX = 137,
    [int]$RoiY = 247,
    [int]$RoiWidth = 526,
    [int]$RoiHeight = 156,
    [string]$OutputCsv = ''
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function Get-GrayMetrics {
    param(
        [System.Drawing.Bitmap]$Bitmap,
        [System.Drawing.Rectangle]$Rectangle
    )

    [double]$sum = 0.0
    [double]$sum2 = 0.0
    [int64]$count = 0
    [int64]$black = 0
    [int64]$white = 0
    [double]$minimum = 255.0
    [double]$maximum = 0.0
    for ($y = $Rectangle.Top; $y -lt $Rectangle.Bottom; ++$y) {
        for ($x = $Rectangle.Left; $x -lt $Rectangle.Right; ++$x) {
            $pixel = $Bitmap.GetPixel($x, $y)
            $gray = 0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B
            $sum += $gray
            $sum2 += $gray * $gray
            ++$count
            if ($gray -le 1.0) { ++$black }
            if ($gray -ge 254.0) { ++$white }
            if ($gray -lt $minimum) { $minimum = $gray }
            if ($gray -gt $maximum) { $maximum = $gray }
        }
    }
    $mean = if ($count) { $sum / $count } else { 0.0 }
    $variance = if ($count) { [Math]::Max(0.0, $sum2 / $count - $mean * $mean) } else { 0.0 }
    return [pscustomobject]@{
        Count = $count
        Mean = $mean
        StdDev = [Math]::Sqrt($variance)
        Min = $minimum
        Max = $maximum
        BlackFraction = if ($count) { [double]$black / $count } else { 0.0 }
        WhiteFraction = if ($count) { [double]$white / $count } else { 0.0 }
    }
}

$rows = foreach ($candidate in $ImagePath) {
    $resolved = (Resolve-Path -LiteralPath $candidate).Path
    $bitmap = [System.Drawing.Bitmap]::FromFile($resolved)
    try {
        $full = New-Object System.Drawing.Rectangle(0, 0, $bitmap.Width, $bitmap.Height)
        $roi = [System.Drawing.Rectangle]::Intersect(
            $full,
            (New-Object System.Drawing.Rectangle($RoiX, $RoiY, $RoiWidth, $RoiHeight)))
        if ($roi.Width -le 0 -or $roi.Height -le 0) {
            throw "ROI does not intersect image: $resolved"
        }
        $allMetrics = Get-GrayMetrics -Bitmap $bitmap -Rectangle $full
        $roiMetrics = Get-GrayMetrics -Bitmap $bitmap -Rectangle $roi
        [pscustomobject]@{
            Image = $resolved
            Width = $bitmap.Width
            Height = $bitmap.Height
            Roi = "$($roi.X),$($roi.Y),$($roi.Width),$($roi.Height)"
            FullMean = [Math]::Round($allMetrics.Mean, 6)
            FullStdDev = [Math]::Round($allMetrics.StdDev, 6)
            FullBlackFraction = [Math]::Round($allMetrics.BlackFraction, 9)
            FullWhiteFraction = [Math]::Round($allMetrics.WhiteFraction, 9)
            RoiMean = [Math]::Round($roiMetrics.Mean, 6)
            RoiStdDev = [Math]::Round($roiMetrics.StdDev, 6)
            RoiMin = [Math]::Round($roiMetrics.Min, 6)
            RoiMax = [Math]::Round($roiMetrics.Max, 6)
            RoiBlackFraction = [Math]::Round($roiMetrics.BlackFraction, 9)
            RoiWhiteFraction = [Math]::Round($roiMetrics.WhiteFraction, 9)
        }
    } finally {
        $bitmap.Dispose()
    }
}

$rows | Format-Table -AutoSize
if ($OutputCsv) {
    $parent = Split-Path -Parent $OutputCsv
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $rows | Export-Csv -LiteralPath $OutputCsv -NoTypeInformation -Encoding UTF8
    Write-Host "[P4ImageQualityQC] result=PASS images=$($rows.Count) output=$OutputCsv"
}
