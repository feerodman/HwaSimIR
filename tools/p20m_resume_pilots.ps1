param([switch]$Run)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Push-Location $repo
try {
    $failed = @()
    foreach ($task in @(
        @('prepare', 'logs/p20m/20261021_full_domain'),
        @('prepare-repair', 'logs/p20m/20261021_geometry_repair'),
        @('prepare-solar-repair', 'logs/p20m/20261021_solar_repair')
    )) {
        & python tools/p20m_modtran.py $task[0] --root $task[1]
        if ($LASTEXITCODE -ne 0) { throw "Immutable case generation failed: $($task[1])" }
        if ($Run) {
            & python tools/p20m_modtran.py run --resume --root $task[1]
            if ($LASTEXITCODE -ne 0) { throw "Runner failed: $($task[1])" }
        }
        & python tools/p20m_modtran.py qc --root $task[1] > (Join-Path $task[1] 'qc_console.txt')
        if ($LASTEXITCODE -ne 0) { $failed += $task[1] }
    }
    if ($failed.Count) {
        Write-Output "P20M pilot gate NOT PASSED: $($failed -join ', ')"
        Write-Output 'Full-domain release remains NOT_ACCEPTED; production data files unchanged.'
        exit 2
    }
    Write-Output 'Pilot spectrum checks passed; full-domain coverage and holdout are separate gates.'
}
finally { Pop-Location }
