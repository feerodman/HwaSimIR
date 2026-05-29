param(
    [string]$PcModWinRoot = "F:\Programs\PcModWin5",
    [string]$Output = ""
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")
$rootPath = $root.Path
$processedDir = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\processed"
if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $processedDir "modout_units_doc_hits.txt"
}
elseif (-not [System.IO.Path]::IsPathRooted($Output)) {
    $Output = Join-Path $rootPath $Output
}

if (-not (Test-Path -LiteralPath $PcModWinRoot -PathType Container)) {
    throw "PcModWin root not found: $PcModWinRoot"
}
New-Item -ItemType Directory -Force -Path $processedDir | Out-Null

$keywords = @(
    "PTH_THRML",
    "SOL_SCAT",
    "TOTAL_RAD",
    "SOLAR",
    "WATTS",
    "CM2",
    "STER",
    "CM-1",
    "tape7",
    "MODOUT2",
    "irradiance",
    "radiance"
)
$extensions = @(".pdf", ".txt", ".hlp", ".chm", ".doc", ".html", ".htm")
$files = Get-ChildItem -LiteralPath $PcModWinRoot -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $extensions -contains $_.Extension.ToLowerInvariant() }

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("PcModWin5/MODTRAN local unit documentation search") | Out-Null
$lines.Add("root=$PcModWinRoot") | Out-Null
$lines.Add("generated_at=$(Get-Date -Format s)") | Out-Null
$lines.Add("keywords=$($keywords -join ', ')") | Out-Null
$lines.Add("") | Out-Null

$hitCount = 0
$searchedCount = 0
foreach ($file in $files) {
    $searchedCount += 1
    if ($file.Length -gt 52428800) {
        $lines.Add("SKIP large file: $($file.FullName) bytes=$($file.Length)") | Out-Null
        continue
    }
    try {
        $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
        $text = [System.Text.Encoding]::ASCII.GetString($bytes)
    }
    catch {
        $lines.Add("SKIP unreadable: $($file.FullName) reason=$($_.Exception.Message)") | Out-Null
        continue
    }

    foreach ($keyword in $keywords) {
        $escaped = [regex]::Escape($keyword)
        $matches = [regex]::Matches($text, $escaped, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        if ($matches.Count -eq 0) {
            continue
        }
        $hitCount += $matches.Count
        $lines.Add("FILE: $($file.FullName)") | Out-Null
        $lines.Add("  keyword=$keyword hits=$($matches.Count) size=$($file.Length)") | Out-Null
        $sampleCount = [Math]::Min(5, $matches.Count)
        for ($i = 0; $i -lt $sampleCount; $i++) {
            $match = $matches[$i]
            $start = [Math]::Max(0, $match.Index - 120)
            $length = [Math]::Min(260, $text.Length - $start)
            $snippet = $text.Substring($start, $length)
            $snippet = ($snippet -replace "\s+", " ").Trim()
            $lines.Add("  snippet[$i]=$snippet") | Out-Null
        }
        $lines.Add("") | Out-Null
    }
}

$lines.Insert(4, "files_searched=$searchedCount")
$lines.Insert(5, "total_keyword_hits=$hitCount")
$lines | Set-Content -LiteralPath $Output -Encoding UTF8

Write-Host "Wrote PcModWin unit doc hits to $Output"
Write-Host "files_searched=$searchedCount"
Write-Host "total_keyword_hits=$hitCount"
if ($hitCount -eq 0) {
    Write-Host "No direct documentation hits found. Keep MODOUT2 units as NEED_UNIT_CONFIRMATION."
}
