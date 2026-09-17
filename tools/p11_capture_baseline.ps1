param(
    [string]$OutputDirectory = "logs/p11/baseline/prechange"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory
} else {
    Join-Path $root $OutputDirectory
}
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Write-Utf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

function Relative-Path([string]$Path) {
    $full = [IO.Path]::GetFullPath($Path)
    $prefix = [IO.Path]::GetFullPath($root).TrimEnd('\') + '\'
    if ($full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        return $full.Substring($prefix.Length).Replace('\', '/')
    }
    return $full.Replace('\', '/')
}

$head = (& git -C $root rev-parse HEAD | Out-String).Trim()
$commit = (& git -C $root log -1 --format=fuller | Out-String).TrimEnd()
$statusRaw = (& git -C $root status --porcelain=v2 --branch | Out-String).TrimEnd()
$status = (($statusRaw -split "`r?`n") | Where-Object {
    $_ -notmatch '^\? (tools/p11_|logs/p11/)'
}) -join "`n"
$diff = (& git -C $root diff --binary --no-ext-diff | Out-String)
$cachedDiff = (& git -C $root diff --cached --binary --no-ext-diff | Out-String)

Write-Utf8 (Join-Path $out "head.txt") ($head + "`n`n" + $commit + "`n")
Write-Utf8 (Join-Path $out "status.txt") ($status + "`n")
Write-Utf8 (Join-Path $out "status_with_p11_task_files.txt") ($statusRaw + "`n")
Write-Utf8 (Join-Path $out "working_tree.patch") $diff
Write-Utf8 (Join-Path $out "index.patch") $cachedDiff

$scopes = @(
    "HwaSim_IR/HwaSim_IR",
    "HwaSim_IR/Bin/Config",
    "HwaSim_IR/Bin/HwaSim_IR.exe",
    "releases/windows",
    "Start_DataDrivenTestQT.cmd",
    "Start_VideoDisplay.cmd"
)
$files = [Collections.Generic.List[IO.FileInfo]]::new()
foreach ($scope in $scopes) {
    $path = Join-Path $root $scope
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        $files.Add((Get-Item -LiteralPath $path))
    } elseif (Test-Path -LiteralPath $path -PathType Container) {
        Get-ChildItem -LiteralPath $path -Recurse -File | ForEach-Object { $files.Add($_) }
    }
}

$manifest = foreach ($file in ($files | Sort-Object FullName -Unique)) {
    $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName
    [ordered]@{
        path = Relative-Path $file.FullName
        bytes = [Int64]$file.Length
        last_write_utc = $file.LastWriteTimeUtc.ToString("o")
        sha256 = $hash.Hash.ToLowerInvariant()
    }
}

$payload = [ordered]@{
    schema = "HwaSimIR.P11.BaselineManifest.v1"
    captured_utc = [DateTime]::UtcNow.ToString("o")
    git_head = $head
    dirty_status_sha256 = ([BitConverter]::ToString(
        [Security.Cryptography.SHA256]::Create().ComputeHash(
            [Text.Encoding]::UTF8.GetBytes($status)))).Replace("-", "").ToLowerInvariant()
    scopes = $scopes
    file_count = @($manifest).Count
    files = @($manifest)
}
$manifestPath = Join-Path $out "local_manifest.json"
Write-Utf8 $manifestPath (($payload | ConvertTo-Json -Depth 6) + "`n")
$manifestHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $manifestPath).Hash.ToLowerInvariant()
Write-Utf8 (Join-Path $out "local_manifest.sha256") ($manifestHash + "  local_manifest.json`n")

$summary = [ordered]@{
    result = "PASS"
    output_directory = (Relative-Path $out)
    git_head = $head
    git_status = $status -split "`r?`n"
    manifest_file_count = @($manifest).Count
    manifest_sha256 = $manifestHash
}
Write-Utf8 (Join-Path $out "baseline_summary.json") (($summary | ConvertTo-Json -Depth 4) + "`n")
$summary | ConvertTo-Json -Depth 4
