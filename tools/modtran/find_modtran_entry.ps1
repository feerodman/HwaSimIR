param(
    [string]$PcModWinRoot = "F:\Programs\PcModWin5",
    [string]$ModtranExe = ""
)

$ErrorActionPreference = "Stop"

function Format-EntryCandidate {
    param([System.IO.FileInfo]$Item)

    $name = $Item.Name.ToLowerInvariant()
    $score = 0
    $notes = New-Object System.Collections.Generic.List[string]

    if ($name -match 'modtran|mod5|modwin') {
        $score += 3
        $notes.Add("name mentions modtran/mod5/modwin") | Out-Null
    }
    if ($name -match 'pcmodwin|win|gui') {
        $score -= 1
        $notes.Add("may be GUI") | Out-Null
    }
    if ($Item.Extension -match '\.bat|\.cmd') {
        $score += 1
        $notes.Add("script wrapper") | Out-Null
        $batchText = Get-Content -LiteralPath $Item.FullName -Raw -Encoding ASCII -ErrorAction SilentlyContinue
        if ($batchText -match '(?im)^\s*pause\s*$') {
            $score -= 2
            $notes.Add("contains PAUSE") | Out-Null
        }
        if ($batchText -match 'Mod5\.2\.1\.0\.exe') {
            $notes.Add("wraps Mod5.2.1.0.exe") | Out-Null
        }
    }

    [PSCustomObject]@{
        Path          = $Item.FullName
        Name          = $Item.Name
        Extension     = $Item.Extension
        SizeBytes     = $Item.Length
        LastWriteTime = $Item.LastWriteTime
        Score         = $score
        Notes         = $notes -join "; "
    }
}

if (-not [string]::IsNullOrWhiteSpace($ModtranExe)) {
    $candidate = $ModtranExe
    if (-not [System.IO.Path]::IsPathRooted($candidate)) {
        $candidate = Join-Path (Join-Path $PcModWinRoot "bin") $candidate
    }
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Specified -ModtranExe does not exist: $candidate"
    }
    Write-Host "User-specified MODTRAN executable candidate:"
    Get-Item -LiteralPath $candidate | ForEach-Object { Format-EntryCandidate $_ } | Format-Table -AutoSize
    Write-Host ""
    Write-Host "No executable was run."
    exit 0
}

$root = Resolve-Path -LiteralPath $PcModWinRoot
$bin = Join-Path $root.Path "bin"
if (-not (Test-Path -LiteralPath $bin -PathType Container)) {
    throw "PcModWin bin directory not found: $bin"
}

$items = @()
foreach ($pattern in @("*.exe", "*.bat", "*.cmd")) {
    $items += @(Get-ChildItem -LiteralPath $bin -Filter $pattern -File -ErrorAction SilentlyContinue)
}

Write-Host "PcModWin5/MODTRAN command entry candidates"
Write-Host "Bin: $bin"
Write-Host "No executable was run."
Write-Host ""

if ($items.Count -eq 0) {
    Write-Host "No *.exe, *.bat, or *.cmd files found under bin."
    exit 2
}

$items |
    ForEach-Object { Format-EntryCandidate $_ } |
    Sort-Object Score, Name -Descending |
    Format-Table -AutoSize

Write-Host ""
Write-Host "Pick the real command-line runner and pass it to:"
Write-Host '  powershell -ExecutionPolicy Bypass -File tools\run_modtran_cases.ps1 -SingleCase -CaseLimit 1 -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "<confirmed path>"'
