$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $root
if(!$env:HWASIMIR_SSH_PASSWORD){throw 'Set HWASIMIR_SSH_PASSWORD for the authorized test board.'}
$env:SSH_ASKPASS=Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd'
$env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='p7'
function Remote([string]$command){
    & ssh.exe -o StrictHostKeyChecking=yes root@192.168.1.116 $command
    if($LASTEXITCODE -ne 0){throw "Rollback exercise SSH failed: $LASTEXITCODE"}
}
$switched=$false
try {
    # C3 and C4 have identical ELF and launcher bytes. Clone the immutable C3
    # resource snapshot; never write or rename the original rollback snapshot.
    Remote 'set -eu; cd /userdata/HwaSimIR; ! pgrep -x HwaSim_IR; test ! -e Config.p7_saved_C4; test ! -e Config.p7_clone_C3; test ! -e Config.p7_exercised_C3; test "$(sha256sum HwaSim_IR | cut -c1-64)" = 6e4aaae41a5d955961fb6464c09232c24f3a310221d46bbe2fdc5c06c6baef43; test "$(sha256sum Config/deployment_manifest.sha256 | cut -c1-64)" = 8258585a941d6ac68152af99bea3441c5964e2fbee29e30c889942504eb3f298; (cd Config.before_20260914-055336 && sha256sum -c deployment_manifest.sha256 >/dev/null); cp -al Config.before_20260914-055336 Config.p7_clone_C3; mv Config Config.p7_saved_C4; mv Config.p7_clone_C3 Config; echo C3_RESOURCE_ROLLBACK_SWITCH_PASS'
    $switched=$true
    & tools/p7_run_case.ps1 -Name D_rain_geometry_before -Normal -ProductionDefaults -CameraInput tools/p7_inputs/pair_rain_asset.json -Band 1 -Rate 30 -Seconds 8 -CaptureSeconds 8 -Weather 2 -DiagnosticJson tools/p7_inputs/frozen_common.json -RawDump -DumpSeq 90
} finally {
    if($switched){
        Remote 'set -eu; cd /userdata/HwaSimIR; pkill -TERM -x HwaSim_IR 2>/dev/null || true; for n in 1 2 3 4 5; do pgrep -x HwaSim_IR >/dev/null || break; sleep 1; done; ! pgrep -x HwaSim_IR; mv Config Config.p7_exercised_C3; mv Config.p7_saved_C4 Config; (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null); (cd Config.before_20260914-055336 && sha256sum -c deployment_manifest.sha256 >/dev/null); test "$(sha256sum Config/deployment_manifest.sha256 | cut -c1-64)" = 8258585a941d6ac68152af99bea3441c5964e2fbee29e30c889942504eb3f298; echo C4_FULL_RESTORE_AND_C3_SNAPSHOT_VERIFY_PASS'
    }
}
& tools/p7_run_case.ps1 -Name D_rain_geometry_after -Normal -ProductionDefaults -CameraInput tools/p7_inputs/pair_rain_asset.json -Band 1 -Rate 30 -Seconds 8 -CaptureSeconds 8 -Weather 2 -DiagnosticJson tools/p7_inputs/frozen_common.json -RawDump -DumpSeq 90

