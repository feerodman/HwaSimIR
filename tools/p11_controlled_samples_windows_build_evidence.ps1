[CmdletBinding()]
param(
    [string]$OutputDirectory = 'logs/p11/work/controlled_samples_windows_build'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $root $OutputDirectory }
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Get-AuditFile([string]$RelativePath) {
    $path = Join-Path $root $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required build-evidence file missing: $path"
    }
    $item = Get-Item -LiteralPath $path
    [ordered]@{
        path = $item.FullName
        bytes = $item.Length
        lastWriteTime = $item.LastWriteTime.ToString('o')
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

$rendererSources = @(
    'HwaSim_IR\HwaSim_IR\HwaSimIR.cpp',
    'HwaSim_IR\HwaSim_IR\Annotation\AnnotationConfig.cpp',
    'HwaSim_IR\HwaSim_IR\IR\IREnginePlumeModel.cpp',
    'HwaSim_IR\HwaSim_IR\IR\IREnginePlumeModel.h'
)
$stimSources = @('DataDrivenTestQT\mainwindow.cpp')
$configAssets = @(
    'HwaSim_IR\Bin\Config\Annotation\annotation_profiles.json',
    'HwaSim_IR\Bin\Config\IRPlume\engine_plume_profiles.json',
    'HwaSim_IR\Bin\Config\TargetLib\Targets.json',
    'HwaSim_IR\Bin\Config\TargetLib\p11\controlled_samples\p11_controlled_samples.bam',
    'HwaSim_IR\Bin\Config\TargetLib\p11\controlled_samples\manifest.json'
)

$rendererBinary = Get-AuditFile 'HwaSim_IR\Bin\HwaSim_IR.exe'
$stimBinary = Get-AuditFile 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe'
$rendererSourceAudit = @($rendererSources | ForEach-Object { Get-AuditFile $_ })
$stimSourceAudit = @($stimSources | ForEach-Object { Get-AuditFile $_ })
$assetAudit = @($configAssets | ForEach-Object { Get-AuditFile $_ })

$rendererNewestSource = ($rendererSourceAudit | ForEach-Object { [datetime]$_.lastWriteTime } | Measure-Object -Maximum).Maximum
$stimNewestSource = ($stimSourceAudit | ForEach-Object { [datetime]$_.lastWriteTime } | Measure-Object -Maximum).Maximum
if ([datetime]$rendererBinary.lastWriteTime -lt $rendererNewestSource) {
    throw 'HwaSim_IR.exe predates a controlled-sample/plume source; rebuild required'
}
if ([datetime]$stimBinary.lastWriteTime -lt $stimNewestSource) {
    throw 'DataDrivenTestQT.exe predates the 0x66 stimulus source; rebuild required'
}

$summary = [ordered]@{
    result = 'PASS'
    scope = 'P11 controlled-sample and plume Windows Release build evidence; no runtime matrix executed'
    protocolLayoutChanged = $false
    renderer = $rendererBinary
    stimulus = $stimBinary
    rendererSources = $rendererSourceAudit
    stimulusSources = $stimSourceAudit
    runtimeConfigAndAssets = $assetAudit
}
$summaryPath = Join-Path $out 'windows_build_evidence.json'
[IO.File]::WriteAllText($summaryPath, ($summary | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
$summary | ConvertTo-Json -Depth 5 -Compress
