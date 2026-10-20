param(
    [string]$Root='logs/p20m2/20261021_v1',
    [string]$Python='F:\Programs\anaconda3\python.exe',
    [switch]$RunEngine
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $repo
$tasks=@('g1_v2','g1_directional','g1_vertical','g1_clear','g1_nearfield','g1_nearfield_p1','g1_nearfield_p1_swir')
if($RunEngine) {
    foreach($task in $tasks) {
        & $Python tools/p20m_modtran.py run --resume --root "$Root/$task" --max-attempts 1
        if($LASTEXITCODE -ne 0){throw "Engine/resume infrastructure failed: $task"}
    }
}
& $Python tools/p20m2_review.py --output "$Root/g0"
if($LASTEXITCODE -ne 0){throw 'Original immutable evidence QC failed'}
& $Python tools/p20m2_source_audit.py --root $Root
if($LASTEXITCODE -ne 0){throw 'Source identity audit failed'}
& $Python tools/p20m2_physics_report.py --root $Root
if($LASTEXITCODE -ne 0){throw 'Development evidence collection failed'}
foreach($task in @('g1_nearfield','g1_nearfield_p1','g1_nearfield_p1_swir')) {
    & $Python tools/p20m2_nearfield.py analyze --root "$Root/$task"
    if($LASTEXITCODE -ne 0){throw "Near-field analysis failed: $task"}
}
& $Python tools/p20m2_replay_inventory.py --out "$Root/customer_replay_inventory.json"
if($LASTEXITCODE -ne 0){throw 'Original customer CSV inventory failed'}
& $Python tools/p20m2_acceptance.py --root $Root
# Exit 2 is deliberate: development evidence is not an accepted runtime package.
exit $LASTEXITCODE
