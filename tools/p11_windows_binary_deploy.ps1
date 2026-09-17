[CmdletBinding()]
param(
    [ValidateSet('Deploy','Rollback')][string]$Mode = 'Deploy',
    [string]$EvidenceDirectory = '',
    [string]$ManifestPath = ''
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$source = [IO.Path]::GetFullPath((Join-Path $repo 'HwaSim_IR\HwaSim_IR\Bin'))
$destination = [IO.Path]::GetFullPath((Join-Path $repo 'HwaSim_IR\Bin'))
$expectedDestination = [IO.Path]::GetFullPath((Join-Path $repo 'HwaSim_IR\Bin'))
if ($destination -cne $expectedDestination -or -not $destination.StartsWith($repo, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe destination: $destination"
}

function Get-FileRecord([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [ordered]@{ path=$Path; exists=$false; bytes=0; sha256=$null }
    }
    return [ordered]@{
        path=$Path
        exists=$true
        bytes=(Get-Item -LiteralPath $Path).Length
        sha256=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

if ($Mode -eq 'Rollback') {
    if (-not $ManifestPath) { throw 'Rollback requires -ManifestPath' }
    $manifestFile = [IO.Path]::GetFullPath($ManifestPath)
    $manifest = Get-Content -LiteralPath $manifestFile -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($manifest.schema -ne 'hwasimir_p11_windows_binary_deploy_1') { throw 'Unexpected manifest schema' }
    foreach ($item in @($manifest.files)) {
        $target = [IO.Path]::GetFullPath([string]$item.destination)
        if (-not $target.StartsWith($destination + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe rollback target: $target"
        }
        if ([bool]$item.before.exists) {
            Copy-Item -LiteralPath ([string]$item.backup) -Destination $target -Force
        } elseif (Test-Path -LiteralPath $target -PathType Leaf) {
            Remove-Item -LiteralPath $target -Force
        }
        $after = Get-FileRecord $target
        if ([bool]$item.before.exists -and $after.sha256 -ne [string]$item.before.sha256) {
            throw "Rollback hash mismatch: $target"
        }
    }
    Write-Output "[P11WindowsDeploy] mode=Rollback result=PASS manifest=$manifestFile"
    exit 0
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $EvidenceDirectory) {
    $EvidenceDirectory = Join-Path $repo "logs\p11\build\windows-final-deploy-$stamp"
}
$evidence = [IO.Path]::GetFullPath($EvidenceDirectory)
if (-not $evidence.StartsWith($repo, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Evidence directory must remain inside the repository: $evidence"
}
$backup = Join-Path $evidence 'backup'
New-Item -ItemType Directory -Force -Path $backup | Out-Null

$names = @(
    'HwaSim_IR.exe', 'HwaSim_IR.pdb',
    'avcodec-62.dll', 'avdevice-62.dll', 'avfilter-11.dll', 'avformat-62.dll',
    'avutil-60.dll', 'swresample-6.dll', 'swscale-9.dll'
)
$records = @()
foreach ($name in $names) {
    $from = Join-Path $source $name
    if (-not (Test-Path -LiteralPath $from -PathType Leaf)) { throw "Missing build artifact: $from" }
    $to = Join-Path $destination $name
    $before = Get-FileRecord $to
    $backupPath = Join-Path $backup $name
    if ($before.exists) { Copy-Item -LiteralPath $to -Destination $backupPath -Force }
    Copy-Item -LiteralPath $from -Destination $to -Force
    $sourceRecord = Get-FileRecord $from
    $after = Get-FileRecord $to
    if ($sourceRecord.sha256 -ne $after.sha256) { throw "Deployment hash mismatch: $name" }
    $records += [ordered]@{
        name=$name
        source=$from
        destination=$to
        backup=$(if ($before.exists) { $backupPath } else { $null })
        before=$before
        after=$after
    }
}

$manifest = [ordered]@{
    schema='hwasimir_p11_windows_binary_deploy_1'
    status='PASS'
    generatedUtc=(Get-Date).ToUniversalTime().ToString('o')
    sourceDirectory=$source
    destinationDirectory=$destination
    files=$records
}
$manifestFile = Join-Path $evidence 'deployment_manifest.json'
[IO.File]::WriteAllText($manifestFile, ($manifest | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
Write-Output "[P11WindowsDeploy] mode=Deploy result=PASS manifest=$manifestFile"
