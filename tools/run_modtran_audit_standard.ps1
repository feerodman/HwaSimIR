param(
    [string]$CaseRoot = "D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\audit_mwir_standard_20260906",
    [string]$PcModBin = "F:\Programs\PcModWin5\Bin"
)

$ErrorActionPreference = "Stop"
$expectedRoot = [IO.Path]::GetFullPath("D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\audit_mwir_standard_20260906")
$resolvedRoot = [IO.Path]::GetFullPath($CaseRoot)
if ($resolvedRoot -ne $expectedRoot) {
    throw "Refusing unexpected CaseRoot: $resolvedRoot"
}
$resolvedBin = [IO.Path]::GetFullPath($PcModBin)
if ($resolvedBin -ne [IO.Path]::GetFullPath("F:\Programs\PcModWin5\Bin")) {
    throw "Refusing unexpected PcModBin: $resolvedBin"
}
$exe = Join-Path $resolvedBin "Mod5.2.1.0.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw "MODTRAN executable not found: $exe" }

$fixedNames = @("modin", "tape5", "tape6", "tape7", "tape8", "tape7.scn", "specflux")
$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$backupRoot = [IO.Path]::Combine($tempBase, "hwasimir-modtran-backup-" + [Guid]::NewGuid().ToString("N"))
if (-not ([IO.Path]::GetFullPath($backupRoot).StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase))) {
    throw "Refusing backup path outside temp: $backupRoot"
}
New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
$existing = @{}
foreach ($name in $fixedNames) {
    $path = Join-Path $resolvedBin $name
    $existing[$name] = Test-Path -LiteralPath $path
    if ($existing[$name]) {
        Copy-Item -LiteralPath $path -Destination (Join-Path $backupRoot $name) -Force
    }
}

$inputs = Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File -Filter "*.tp5" | Sort-Object FullName
$runRows = @()
$engineVersion = (Get-Item -LiteralPath $exe).VersionInfo.FileVersion
if ([string]::IsNullOrWhiteSpace($engineVersion)) {
    $engineVersion = "5.2.1.0 (from executable filename; PE version field empty)"
}
try {
    foreach ($inputItem in $inputs) {
        $input = $inputItem.FullName
        $caseDir = $inputItem.DirectoryName
        $caseId = $inputItem.BaseName
        Copy-Item -LiteralPath $input -Destination (Join-Path $resolvedBin "modin") -Force
        Copy-Item -LiteralPath $input -Destination (Join-Path $resolvedBin "tape5") -Force
        foreach ($name in @("tape6", "tape7", "tape8", "tape7.scn", "specflux")) {
            $path = Join-Path $resolvedBin $name
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
        }
        $stdoutPath = Join-Path $caseDir "engine_console.txt"
        Push-Location $resolvedBin
        try { & $exe *> $stdoutPath } finally { Pop-Location }
        if ($LASTEXITCODE -ne 0) { throw "MODTRAN failed for $caseId with exit code $LASTEXITCODE" }
        $mapping = @{
            "tape6" = "MODOUT1.txt"; "tape7" = "MODOUT2.txt"; "tape8" = "MODOUT3.txt";
            "tape7.scn" = "tape7.scn"; "specflux" = "spectral_flux.flx"
        }
        foreach ($sourceName in $mapping.Keys) {
            $source = Join-Path $resolvedBin $sourceName
            if (Test-Path -LiteralPath $source) {
                $destination = Join-Path $caseDir $mapping[$sourceName]
                Copy-Item -LiteralPath $source -Destination $destination -Force
                $runRows += [pscustomobject]@{
                    case_id = $caseId
                    engine_path = $exe
                    engine_file_version = $engineVersion
                    generated_at = (Get-Date).ToString("o")
                    output_kind = $sourceName
                    output_file = [IO.Path]::GetFullPath($destination)
                    output_sha256 = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
                }
            }
        }
        if (-not (Test-Path -LiteralPath (Join-Path $caseDir "MODOUT2.txt"))) {
            throw "MODTRAN produced no MODOUT2 for $caseId"
        }
        Write-Output "Completed $caseId"
    }
    $runRows | Export-Csv -LiteralPath (Join-Path $resolvedRoot "run_manifest.csv") -NoTypeInformation -Encoding UTF8
}
finally {
    foreach ($name in $fixedNames) {
        $path = Join-Path $resolvedBin $name
        $backup = Join-Path $backupRoot $name
        if ($existing[$name]) {
            Copy-Item -LiteralPath $backup -Destination $path -Force
        }
        elseif (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Force
        }
    }
    if (Test-Path -LiteralPath $backupRoot) {
        $resolvedBackup = [IO.Path]::GetFullPath($backupRoot)
        if (-not $resolvedBackup.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing cleanup outside temp: $resolvedBackup"
        }
        Remove-Item -LiteralPath $resolvedBackup -Recurse -Force
    }
}
