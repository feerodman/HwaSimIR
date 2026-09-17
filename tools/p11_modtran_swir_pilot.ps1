param(
    [string]$CaseRoot = "D:\HwaSimIR\logs\p11\modtran\pilot",
    [string]$PcModBin = "F:\Programs\PcModWin5\Bin",
    [int]$StartIndex = 0,
    [int]$MaxCases = 0
)

$ErrorActionPreference = "Stop"
$expectedRoots = @(
    [IO.Path]::GetFullPath("D:\HwaSimIR\logs\p11\modtran\pilot"),
    [IO.Path]::GetFullPath("D:\HwaSimIR\logs\p11\modtran\swir_ground_grid"),
    [IO.Path]::GetFullPath("D:\HwaSimIR\logs\p11\modtran\mwir_ground_grid"),
    [IO.Path]::GetFullPath("D:\HwaSimIR\logs\p11\modtran\humidity_pilot"),
    [IO.Path]::GetFullPath("D:\HwaSimIR\logs\p11\modtran\humidity_grid"),
    [IO.Path]::GetFullPath("D:\HwaSimIR\logs\p11\modtran\solar_heating_ground_grid")
)
$resolvedRoot = [IO.Path]::GetFullPath($CaseRoot)
if ($expectedRoots -notcontains $resolvedRoot) { throw "Refusing unexpected CaseRoot: $resolvedRoot" }
$expectedBin = [IO.Path]::GetFullPath("F:\Programs\PcModWin5\Bin")
$resolvedBin = [IO.Path]::GetFullPath($PcModBin)
if ($resolvedBin -ne $expectedBin) { throw "Refusing unexpected PcModBin: $resolvedBin" }
$exe = Join-Path $resolvedBin "Mod5.2.1.0.exe"
$manifest = Join-Path $resolvedRoot "case_manifest.csv"
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "MODTRAN executable not found: $exe" }
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { throw "Pilot manifest not found: $manifest" }

$fixedNames = @("modin", "tape5", "tape6", "tape7", "tape8", "tape7.scn", "specflux")
$tempBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$backupRoot = [IO.Path]::Combine($tempBase, "hwasimir-p11-modtran-backup-" + [Guid]::NewGuid().ToString("N"))
if (-not ([IO.Path]::GetFullPath($backupRoot).StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase))) {
    throw "Refusing backup outside temp: $backupRoot"
}
New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
$existing = @{}
$beforeState = @{}
foreach ($name in $fixedNames) {
    $path = Join-Path $resolvedBin $name
    $existing[$name] = Test-Path -LiteralPath $path
    if ($existing[$name]) {
        $item = Get-Item -LiteralPath $path
        $beforeState[$name] = [pscustomobject]@{
            exists = $true
            size_bytes = $item.Length
            sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            last_write_time_utc = $item.LastWriteTimeUtc.ToString("o")
        }
        Copy-Item -LiteralPath $path -Destination (Join-Path $backupRoot $name) -Force
    }
    else {
        $beforeState[$name] = [pscustomobject]@{ exists = $false; size_bytes = 0; sha256 = ""; last_write_time_utc = "" }
    }
}

$allCaseRows = @(Import-Csv -LiteralPath $manifest)
if ($StartIndex -lt 0 -or $StartIndex -ge $allCaseRows.Count) { throw "Invalid StartIndex=$StartIndex for $($allCaseRows.Count) cases" }
$lastIndexExclusive = if ($MaxCases -gt 0) { [Math]::Min($allCaseRows.Count, $StartIndex + $MaxCases) } else { $allCaseRows.Count }
$caseRows = @($allCaseRows[$StartIndex..($lastIndexExclusive - 1)])
$selectedIds = @{}
foreach ($selectedCase in $caseRows) { $selectedIds[$selectedCase.case_id] = $true }
$runManifestPath = Join-Path $resolvedRoot "run_manifest.csv"
$caseStatusPath = Join-Path $resolvedRoot "case_run_status.csv"
$runRows = @()
if (Test-Path -LiteralPath $runManifestPath -PathType Leaf) {
    $runRows = @(Import-Csv -LiteralPath $runManifestPath | Where-Object { -not $selectedIds.ContainsKey($_.case_id) })
}
$caseStatusRows = @()
if (Test-Path -LiteralPath $caseStatusPath -PathType Leaf) {
    $caseStatusRows = @(Import-Csv -LiteralPath $caseStatusPath | Where-Object { -not $selectedIds.ContainsKey($_.case_id) })
}
$engineItem = Get-Item -LiteralPath $exe
$engineHash = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
$engineVersion = $engineItem.VersionInfo.FileVersion
if ([string]::IsNullOrWhiteSpace($engineVersion)) { $engineVersion = "5.2.1.0 (executable filename)" }
try {
    foreach ($case in $caseRows) {
        $input = [IO.Path]::GetFullPath($case.input_file)
        if (-not $input.StartsWith($resolvedRoot, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing input outside pilot root: $input"
        }
        $caseDir = Split-Path -Parent $input
        Copy-Item -LiteralPath $input -Destination (Join-Path $resolvedBin "modin") -Force
        Copy-Item -LiteralPath $input -Destination (Join-Path $resolvedBin "tape5") -Force
        foreach ($name in @("tape6", "tape7", "tape8", "tape7.scn", "specflux")) {
            $path = Join-Path $resolvedBin $name
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
        }
        $stdout = Join-Path $caseDir "engine_stdout.txt"
        $stderr = Join-Path $caseDir "engine_stderr.txt"
        $process = Start-Process -FilePath $exe -WorkingDirectory $resolvedBin -WindowStyle Hidden -Wait -PassThru `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        $mapping = @{
            "tape6" = "MODOUT1.txt"; "tape7" = "MODOUT2.txt"; "tape8" = "MODOUT3.txt";
            "tape7.scn" = "tape7.scn"; "specflux" = "spectral_flux.flx"
        }
        foreach ($sourceName in $mapping.Keys) {
            $source = Join-Path $resolvedBin $sourceName
            if (Test-Path -LiteralPath $source -PathType Leaf) {
                $destination = Join-Path $caseDir $mapping[$sourceName]
                Copy-Item -LiteralPath $source -Destination $destination -Force
                $runRows += [pscustomobject]@{
                    scenario_id = $case.scenario_id
                    case_id = $case.case_id
                    mode = $case.mode
                    generated_at_utc = [DateTime]::UtcNow.ToString("o")
                    engine_path = $exe
                    engine_file_version = $engineVersion
                    engine_sha256 = $engineHash
                    output_kind = $sourceName
                    output_file = [IO.Path]::GetFullPath($destination)
                    output_sha256 = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
                    engine_exit_code = $process.ExitCode
                }
            }
        }
        $required = if ($case.mode -eq "SpectralFlux") { Join-Path $caseDir "spectral_flux.flx" } else { Join-Path $caseDir "MODOUT2.txt" }
        $requiredPresent = Test-Path -LiteralPath $required -PathType Leaf
        $caseStatusRows += [pscustomobject]@{
            case_id = $case.case_id
            mode = $case.mode
            generated_at_utc = [DateTime]::UtcNow.ToString("o")
            engine_exit_code = $process.ExitCode
            required_output = $required
            required_output_present = $requiredPresent
            status = if ($process.ExitCode -eq 0 -and $requiredPresent) { "SUCCESS" } elseif ($process.ExitCode -ne 0) { "ENGINE_NONZERO" } else { "REQUIRED_OUTPUT_MISSING" }
        }
        $runRows | Export-Csv -LiteralPath $runManifestPath -NoTypeInformation -Encoding UTF8
        $caseStatusRows | Export-Csv -LiteralPath $caseStatusPath -NoTypeInformation -Encoding UTF8
        if ($process.ExitCode -ne 0) { throw "MODTRAN failed for $($case.case_id), exit=$($process.ExitCode)" }
        if (-not $requiredPresent) { throw "MODTRAN produced no required output for $($case.case_id): $required" }
        Write-Output "Completed $($case.case_id)"
    }
    $runRows | Export-Csv -LiteralPath $runManifestPath -NoTypeInformation -Encoding UTF8
    [pscustomobject]@{
        checked_at_utc = [DateTime]::UtcNow.ToString("o")
        executable = $exe
        file_version = $engineVersion
        sha256 = $engineHash
        run_status = "SUCCESS_REAL_MODTRAN_OUTPUT"
        batch_start_index = $StartIndex
        batch_case_count = $caseRows.Count
        accumulated_success_case_count = @($caseStatusRows | Where-Object status -eq "SUCCESS").Count
        requested_manifest_case_count = $allCaseRows.Count
        license_evidence = "All $($caseRows.Count) requested batch component runs exited zero and produced fresh MODOUT/spectral_flux files."
    } | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $resolvedRoot "engine_and_license_evidence.json") -Encoding UTF8
}
finally {
    if ($runRows.Count -gt 0) { $runRows | Export-Csv -LiteralPath $runManifestPath -NoTypeInformation -Encoding UTF8 }
    if ($caseStatusRows.Count -gt 0) { $caseStatusRows | Export-Csv -LiteralPath $caseStatusPath -NoTypeInformation -Encoding UTF8 }
    foreach ($name in $fixedNames) {
        $path = Join-Path $resolvedBin $name
        $backup = Join-Path $backupRoot $name
        if ($existing[$name]) { Copy-Item -LiteralPath $backup -Destination $path -Force }
        elseif (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
    }
    if (Test-Path -LiteralPath $backupRoot) {
        $resolvedBackup = [IO.Path]::GetFullPath($backupRoot)
        if (-not $resolvedBackup.StartsWith($tempBase, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing cleanup outside temp: $resolvedBackup"
        }
        Remove-Item -LiteralPath $resolvedBackup -Recurse -Force
    }
    $restoreRows = @()
    foreach ($name in $fixedNames) {
        $path = Join-Path $resolvedBin $name
        $afterExists = Test-Path -LiteralPath $path -PathType Leaf
        $afterHash = if ($afterExists) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() } else { "" }
        $afterSize = if ($afterExists) { (Get-Item -LiteralPath $path).Length } else { 0 }
        $before = $beforeState[$name]
        $restoreRows += [pscustomobject]@{
            fixed_name = $name
            before_exists = $before.exists
            before_size_bytes = $before.size_bytes
            before_sha256 = $before.sha256
            before_last_write_time_utc = $before.last_write_time_utc
            after_exists = $afterExists
            after_size_bytes = $afterSize
            after_sha256 = $afterHash
            restored_exactly = (($before.exists -eq $afterExists) -and ($before.sha256 -eq $afterHash))
        }
    }
    $restoreRows | Export-Csv -LiteralPath (Join-Path $resolvedRoot "installation_restore_evidence.csv") -NoTypeInformation -Encoding UTF8
}
