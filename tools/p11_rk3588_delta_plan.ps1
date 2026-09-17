[CmdletBinding()]
param(
    [string]$RepoRoot = '',
    [string]$ConfigRoot = '',
    [string]$BoardManifest = '',
    [string]$OutputDirectory = '',
    [Int64]$RemoteFreeBytes = 0,
    [Int64]$SafetyBytes = 268435456,
    [string[]]$PreserveBoardPath = @(
        'NetworkConfig_precise.ini',
        'NetworkConfig_search.ini',
        'DDS/ZRDDS_QOS_RK3588_192.168.1.116.xml'
    )
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
if (-not $ConfigRoot) { $ConfigRoot = Join-Path $RepoRoot 'HwaSim_IR\Bin\Config' }
if (-not $BoardManifest) { $BoardManifest = Join-Path $RepoRoot 'logs\p11\rk3588\inventory-current\board_deployment_manifest.sha256' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $RepoRoot 'logs\p11\rk3588\delta-plan-current' }
$ConfigRoot = (Resolve-Path -LiteralPath $ConfigRoot).Path
$BoardManifest = (Resolve-Path -LiteralPath $BoardManifest).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8NoBom = New-Object Text.UTF8Encoding($false)

function Get-UnixRelativePath {
    param([string]$Root, [string]$Path)
    $rootUri = New-Object Uri(($Root.TrimEnd('\') + '\'))
    $pathUri = New-Object Uri($Path)
    return [Uri]::UnescapeDataString($rootUri.MakeRelativeUri($pathUri).ToString())
}

function Assert-SafeRelativePath {
    param([string]$Path)
    if (-not $Path -or $Path.StartsWith('/') -or $Path.StartsWith('\') -or
        $Path -match '(^|/|\\)\.\.(/|\\|$)' -or $Path.Contains("'") -or $Path.Contains('`')) {
        throw "Unsafe manifest path: $Path"
    }
}

$old = @{}
foreach ($line in [IO.File]::ReadAllLines($BoardManifest)) {
    if ($line -notmatch '^([0-9a-fA-F]{64})  (.+)$') { throw "Malformed board manifest line: $line" }
    $relative = $Matches[2] -replace '\\', '/'
    Assert-SafeRelativePath $relative
    if ($old.ContainsKey($relative)) { throw "Duplicate board manifest path: $relative" }
    $old[$relative] = $Matches[1].ToLowerInvariant()
}

$new = @{}
$sizes = @{}
$files = Get-ChildItem -LiteralPath $ConfigRoot -Recurse -File |
    Where-Object { $_.Name -notin @('deployment_manifest.sha256', 'deployment_version.env') } |
    Sort-Object FullName
$index = 0
foreach ($file in $files) {
    $relative = Get-UnixRelativePath -Root $ConfigRoot -Path $file.FullName
    Assert-SafeRelativePath $relative
    if ($new.ContainsKey($relative)) { throw "Duplicate local Config path: $relative" }
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $new[$relative] = $hash
    $sizes[$relative] = [Int64]$file.Length
    $index++
    if (($index % 500) -eq 0) { Write-Progress -Activity 'Hashing P11 Config without copying it' -Status "$index / $($files.Count)" -PercentComplete (100.0 * $index / $files.Count) }
}
Write-Progress -Activity 'Hashing P11 Config without copying it' -Completed

# Machine identity and board-bound QoS remain byte-for-byte from the verified
# live deployment.  A missing board-bound file is not invented from a desktop
# profile, and a present one remains part of the candidate manifest.
foreach ($preserve in $PreserveBoardPath) {
    $relative = $preserve -replace '\\', '/'
    Assert-SafeRelativePath $relative
    if ($old.ContainsKey($relative)) {
        $new[$relative] = $old[$relative]
        if (-not $sizes.ContainsKey($relative)) { $sizes[$relative] = [Int64]0 }
    }
    elseif ($new.ContainsKey($relative)) {
        $new.Remove($relative)
        $sizes.Remove($relative)
    }
}

$manifestLines = New-Object System.Collections.Generic.List[string]
foreach ($relative in ($new.Keys | Sort-Object)) {
    $manifestLines.Add("$($new[$relative])  $relative")
}

$changed = New-Object System.Collections.Generic.List[string]
$added = New-Object System.Collections.Generic.List[string]
$unchanged = New-Object System.Collections.Generic.List[string]
foreach ($relative in ($new.Keys | Sort-Object)) {
    if (-not $old.ContainsKey($relative)) {
        $added.Add($relative)
        $changed.Add($relative)
    }
    elseif ($old[$relative] -ne $new[$relative]) {
        $changed.Add($relative)
    }
    else {
        $unchanged.Add($relative)
    }
}
$removed = New-Object System.Collections.Generic.List[string]
foreach ($relative in ($old.Keys | Sort-Object)) {
    if (-not $new.ContainsKey($relative)) { $removed.Add($relative) }
}

$changedBytes = [Int64]0
foreach ($relative in $changed) { $changedBytes += $sizes[$relative] }
$totalBytes = [Int64]0
foreach ($relative in $new.Keys) { $totalBytes += $sizes[$relative] }
# During staging the compressed transfer archive and the unpacked replacement
# files coexist.  Treat the archive as no smaller than the uncompressed delta
# for a conservative preflight instead of relying on compression ratio.
$requiredBytes = (2 * $changedBytes) + $SafetyBytes
$spaceKnown = $RemoteFreeBytes -gt 0
$spacePass = -not $spaceKnown -or $RemoteFreeBytes -ge $requiredBytes

[IO.File]::WriteAllText((Join-Path $OutputDirectory 'candidate_manifest.sha256'), ($manifestLines -join "`n") + "`n", $utf8NoBom)
$changedText = if ($changed.Count) { ($changed -join "`n") + "`n" } else { '' }
$addedText = if ($added.Count) { ($added -join "`n") + "`n" } else { '' }
$removedText = if ($removed.Count) { ($removed -join "`n") + "`n" } else { '' }
$unchangedText = if ($unchanged.Count) { ($unchanged -join "`n") + "`n" } else { '' }
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'delta_files.txt'), $changedText, $utf8NoBom)
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'added_files.txt'), $addedText, $utf8NoBom)
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'removed_files.txt'), $removedText, $utf8NoBom)
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'unchanged_files.txt'), $unchangedText, $utf8NoBom)

$summary = [ordered]@{
    schema = 'hwasimir.p11.rk3588.delta-plan.v1'
    created_utc = [DateTime]::UtcNow.ToString('o')
    config_root = $ConfigRoot
    board_manifest = $BoardManifest
    board_manifest_sha256 = (Get-FileHash -LiteralPath $BoardManifest -Algorithm SHA256).Hash.ToLowerInvariant()
    candidate_manifest_sha256 = (Get-FileHash -LiteralPath (Join-Path $OutputDirectory 'candidate_manifest.sha256') -Algorithm SHA256).Hash.ToLowerInvariant()
    board_file_count = $old.Count
    candidate_file_count = $new.Count
    added_file_count = $added.Count
    changed_file_count = $changed.Count
    removed_file_count = $removed.Count
    unchanged_file_count = $unchanged.Count
    candidate_bytes = $totalBytes
    delta_payload_bytes = $changedBytes
    delta_staging_multiplier = 2
    safety_bytes = $SafetyBytes
    projected_required_bytes = $requiredBytes
    remote_free_bytes = if ($spaceKnown) { $RemoteFreeBytes } else { $null }
    projected_space_pass = $spacePass
    remote_strategy = 'verified_hard_link_snapshot_plus_delta_rename'
    full_config_copy_required = $false
    preserved_board_paths = @($PreserveBoardPath)
}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'delta_plan.json'), ($summary | ConvertTo-Json -Depth 4) + "`n", $utf8NoBom)
Write-Host "[P11 RK3588 DeltaPlan] result=$($(if($spacePass){'PASS'}else{'FAIL'})) files=$($new.Count) changed=$($changed.Count) removed=$($removed.Count) deltaBytes=$changedBytes requiredBytes=$requiredBytes fullConfigCopy=0 output=$OutputDirectory"
if (-not $spacePass) { throw "Insufficient remote space: free=$RemoteFreeBytes required=$requiredBytes" }
