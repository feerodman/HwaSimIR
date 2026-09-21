#!/bin/sh
set -eu

root=${1:-/userdata/HwaSimIR}
run_id=${2:-p14rb-20260921-final}

[ "$root" = /userdata/HwaSimIR ] || {
    echo '[P14Rollback][FATAL] unsafe root' >&2
    exit 2
}
case "$run_id" in
    p14rb-[A-Za-z0-9_.-]*) ;;
    *) echo '[P14Rollback][FATAL] unsafe run id' >&2; exit 2 ;;
esac

final_elf_sha=80267b01054d9fe8e8ef2c3fd8f3e726003794dafc6cc1086958e9713f18841c
rollback_elf_sha=c6cf72bd263164c60414ebbdec55ceb36296dad489a7080fc7b7af6e1fad3853
config_manifest_sha=9b344f42964dbf4e05d7502ba815bea6d4c6e37276bebd3aa63a8f702ec600c9
sprite_sha=d20ed42cc01cfa4fa0e6f1a7b5fc1ef123380c97ee60250ffd196c08079063d0
precip_sha=39aa314e0f86c2e50c10fdcfed717198a58271b5e6c2252c1f44ae2b2c90ee51
rollback_elf=$root/HwaSim_IR.before_20260921-044023
rollback_config=$root/Config.before_20260921-044516
saved_elf=$root/HwaSim_IR.restore_$run_id
saved_config=$root/Config.restore_$run_id
marker=$root/.p14_rollback_in_progress_$run_id
evidence=$root/logs/$run_id
phase=preflight

sha()
{
    sha256sum "$1" | awk '{print $1}'
}

verify_config()
{
    config=$1
    expected_elf=$2
    [ "$(sha "$config/deployment_manifest.sha256")" = "$config_manifest_sha" ]
    [ "$(sha "$config/GameVFX/sprite.frag")" = "$sprite_sha" ]
    [ "$(sha "$config/Weather/precipitation.frag")" = "$precip_sha" ]
    [ "$(awk -F= '$1=="ElfSha256" {print $2; exit}' "$config/deployment_version.env")" = "$expected_elf" ]
    [ "$(awk -F= '$1=="ConfigManifestSha256" {print $2; exit}' "$config/deployment_version.env")" = "$config_manifest_sha" ]
    (cd "$config" && sha256sum -c deployment_manifest.sha256 >/dev/null)
}

verify_active()
{
    label=$1
    expected_elf=$2
    [ "$(sha "$root/HwaSim_IR")" = "$expected_elf" ]
    verify_config "$root/Config" "$expected_elf"
    build_id=$(readelf -n "$root/HwaSim_IR" | awk '/Build ID:/ {print $3;exit}')
    echo "[P14RollbackIdentity] phase=$label elfSha256=$expected_elf buildId=$build_id configManifestSha256=$config_manifest_sha spriteSha256=$sprite_sha precipitationSha256=$precip_sha result=PASS"
}

run_startup()
{
    label=$1
    log=$2
    set +e
    (
        cd "$root"
        env RenderPresentationMode=HeadlessOffscreen \
            P5MaterialView=0 \
            HwaSimIRExitOnStop=false \
            HwaSimIRLocalRecordingEnable=false \
            timeout -s TERM -k 5s 15s ./run_precise.sh
    ) >"$log" 2>&1
    status=$?
    set -e
    [ "$status" -eq 0 ] || [ "$status" -eq 124 ] || [ "$status" -eq 143 ]
    for wait_index in 1 2 3 4 5; do
        pgrep -x HwaSim_IR >/dev/null || break
        sleep 1
    done
    ! pgrep -x HwaSim_IR >/dev/null
    grep -q '\[RunPreflight\] result=PASS' "$log"
    grep -q '\[DeploymentVersion\] result=PASS' "$log"
    grep -Eq '\[GpuBackend\].*glVendor=ARM.*glRenderer=Mali-LODX.*hardwareGpu=1' "$log"
    grep -q '\[DdsVideo\] initialized=1' "$log"
    ! grep -Eq '\[RunPreflight\]\[FATAL\]|\[StartupFatal\]|Assertion failed:|hardwareGpu=0|llvmpipe|GL_INVALID_OPERATION|Could not bind framebuffer' "$log"
    echo "[P14RollbackStartup] phase=$label processExit=$status preflight=PASS deploymentIdentity=PASS hardwareGpu=PASS ddsVideo=PASS result=PASS log=$log"
}

restore_final()
{
    original_status=$?
    trap - EXIT INT TERM HUP
    pkill -TERM -x HwaSim_IR 2>/dev/null || true
    if [ -e "$saved_elf" ]; then
        if [ -e "$root/HwaSim_IR" ]; then
            [ ! -e "$rollback_elf" ] || { echo '[P14RollbackRecovery][FATAL] ambiguous rollback ELF' >&2; exit 91; }
            mv "$root/HwaSim_IR" "$rollback_elf"
        fi
        mv "$saved_elf" "$root/HwaSim_IR"
    fi
    if [ -e "$saved_config" ]; then
        if [ -e "$root/Config" ]; then
            [ ! -e "$rollback_config" ] || { echo '[P14RollbackRecovery][FATAL] ambiguous rollback Config' >&2; exit 91; }
            mv "$root/Config" "$rollback_config"
        fi
        mv "$saved_config" "$root/Config"
    fi
    rmdir "$marker" 2>/dev/null || true
    if [ "$phase" != done ]; then
        echo "[P14RollbackRecovery] interruptedPhase=$phase originalStatus=$original_status finalRestored=1" >&2
    fi
    exit "$original_status"
}
trap restore_final EXIT INT TERM HUP

! pgrep -x HwaSim_IR >/dev/null
[ ! -e "$marker" ]
[ ! -e "$saved_elf" ]
[ ! -e "$saved_config" ]
[ "$(sha "$root/HwaSim_IR")" = "$final_elf_sha" ]
[ "$(sha "$rollback_elf")" = "$rollback_elf_sha" ]
verify_config "$root/Config" "$final_elf_sha"
verify_config "$rollback_config" "$rollback_elf_sha"
mkdir -p "$evidence"
mkdir "$marker"
echo "[P14RollbackBegin] runId=$run_id rollbackSnapshot=$rollback_elf rollbackConfig=$rollback_config snapshotRetained=1"
verify_active final_before "$final_elf_sha"

phase=activate_rollback
mv "$root/HwaSim_IR" "$saved_elf"
mv "$root/Config" "$saved_config"
mv "$rollback_elf" "$root/HwaSim_IR"
mv "$rollback_config" "$root/Config"
verify_active rollback_active "$rollback_elf_sha"
run_startup rollback_active "$evidence/rollback_startup.log"

phase=restore_final
mv "$root/HwaSim_IR" "$rollback_elf"
mv "$root/Config" "$rollback_config"
mv "$saved_elf" "$root/HwaSim_IR"
mv "$saved_config" "$root/Config"
verify_active final_restored "$final_elf_sha"
run_startup final_restored "$evidence/final_restored_startup.log"

phase=done
rmdir "$marker"
trap - EXIT INT TERM HUP
echo "[P14RollbackFinal] runId=$run_id result=PASS rollbackActivated=1 rollbackStartup=PASS finalRestored=1 finalStartup=PASS rollbackSnapshotRetained=1"
