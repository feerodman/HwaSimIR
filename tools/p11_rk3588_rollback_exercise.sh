#!/bin/sh

# Destructive-in-the-small, recoverable rollback exercise for an already
# accepted P11 deployment.  It temporarily activates the exact immutable
# `before_<stage>` snapshot, starts it, and then restores and starts P11.
set -eu

usage='usage: p11_rk3588_rollback_exercise.sh ROOT ACTIVE_STAGE_ID RUN_ID ROLLBACK_SUFFIX ROLLBACK_ELF_SHA256 ROLLBACK_CONFIG_MANIFEST_SHA256 ACTIVE_ELF_SHA256 ACTIVE_CONFIG_MANIFEST_SHA256 ROLLBACK_PROTOCOL_BAND RESTORED_PROTOCOL_BAND [DURATION_SEC] [LOOP_WAIT_SEC]'
root=${1:?$usage}
stage_id=${2:?$usage}
run_id=${3:?$usage}
rollback_suffix=${4:?$usage}
rollback_elf_sha256=${5:?$usage}
rollback_config_manifest_sha256=${6:?$usage}
active_elf_sha256=${7:?$usage}
active_config_manifest_sha256=${8:?$usage}
rollback_protocol_band=${9:?$usage}
shift 9
restored_protocol_band=${1:?$usage}
duration_sec=${2:-15}
loop_wait_sec=${3:-240}

[ "$root" = /userdata/HwaSimIR ] || { echo '[P11Rollback][FATAL] unsafe root' >&2; exit 2; }
case "$stage_id" in p11-[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]-[0-9][0-9][0-9][0-9][0-9][0-9]) ;; *) echo '[P11Rollback][FATAL] unsafe stage id' >&2; exit 2;; esac
case "$run_id" in p11rb-[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]-[0-9][0-9][0-9][0-9][0-9][0-9]) ;; *) echo '[P11Rollback][FATAL] unsafe run id' >&2; exit 2;; esac
case "$rollback_suffix" in .before_p11-[0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9]-[0-9][0-9][0-9][0-9][0-9][0-9]) ;; *) echo '[P11Rollback][FATAL] unsafe rollback suffix' >&2; exit 2;; esac
for value in "$rollback_elf_sha256" "$rollback_config_manifest_sha256" "$active_elf_sha256" "$active_config_manifest_sha256"; do
    case "$value" in *[!0-9a-fA-F]*|'') echo '[P11Rollback][FATAL] invalid expected SHA-256' >&2; exit 2;; esac
    [ "${#value}" -eq 64 ] || { echo '[P11Rollback][FATAL] invalid expected SHA-256 length' >&2; exit 2; }
done
case "$rollback_protocol_band" in 0|1|2|3) ;; *) echo '[P11Rollback][FATAL] invalid rollback protocol band' >&2; exit 2;; esac
case "$restored_protocol_band" in 0|1|2|3) ;; *) echo '[P11Rollback][FATAL] invalid restored protocol band' >&2; exit 2;; esac
[ "$rollback_suffix" = .before_p11-20260916-053343 ] || { echo '[P11Rollback][FATAL] rollback suffix is not audited P10' >&2; exit 2; }
[ "$rollback_elf_sha256" = 840b649b7ed21f3b4a8b16a83bba25aee17319efa2932f8ff6705d14d0fe00ba ] || { echo '[P11Rollback][FATAL] rollback ELF is not audited P10' >&2; exit 2; }
[ "$rollback_config_manifest_sha256" = 38f6a89d49826bc00d1555c391a27538933e1c15d5dc03656a117cd350b7e9ca ] || { echo '[P11Rollback][FATAL] rollback config is not audited P10' >&2; exit 2; }
[ "$rollback_protocol_band" -eq 1 ] || { echo '[P11Rollback][FATAL] audited P10 requires NIR protocol band 1' >&2; exit 2; }
case "$restored_protocol_band" in 0|1) ;; *) echo '[P11Rollback][FATAL] restored P11 must use SWIR or NIR protocol band' >&2; exit 2;; esac
case "$duration_sec" in *[!0-9]*|'') echo '[P11Rollback][FATAL] invalid duration' >&2; exit 2;; esac
[ "$duration_sec" -ge 8 ] && [ "$duration_sec" -le 60 ] || { echo '[P11Rollback][FATAL] duration outside 8..60' >&2; exit 2; }
case "$loop_wait_sec" in *[!0-9]*|'') echo '[P11Rollback][FATAL] invalid loop wait' >&2; exit 2;; esac
[ "$loop_wait_sec" -ge 60 ] && [ "$loop_wait_sec" -le 600 ] || { echo '[P11Rollback][FATAL] loop wait outside 60..600' >&2; exit 2; }

backup_suffix=$rollback_suffix
restore_suffix=.p11_restore_$run_id
marker=$root/.p11_rollback_in_progress
evidence=$root/logs/P11_rollback_$run_id
mkdir -p "$evidence"
[ ! -e "$marker" ] || { echo '[P11Rollback][FATAL] marker already exists' >&2; exit 3; }
mkdir "$marker"

components='HwaSim_IR run_precise.sh rk3588_hwasimir_performance_mode.sh Config'
phase=preflight
owned_launcher_pid=

# Startup-only evidence is not a rollback acceptance loop.  Keep this list in
# sync with tools/p11_finalize_delivery.py so a renderer assertion or a formal
# Stage6 failure can never be hidden behind successful preflight/GPU lines.
fatal_pattern='\[RunPreflight\]\[FATAL\]|\[StartupFatal\]|Assertion failed:|\[Stage6 RawAttachment\]\[ERROR\]|\[Stage6 [^]]*\]\[ERROR\]|\[P6LinearCapture\]\[ERROR\]|hardwareGpu=0|llvmpipe|GL_INVALID_OPERATION|GL error 0x502|Could not bind framebuffer|raw_buffer_unavailable|missing_float_buffer'

field_from_line()
{
    key=$1
    line=$2
    printf '%s\n' "$line" | sed -n "s/.*${key}=\([0-9][0-9]*\).*/\1/p"
}

validate_render_send_loop()
{
    release=$1
    protocol_band=$2
    log=$3
    ! grep -Eq "$fatal_pattern" "$log"
    grep -q '\[RunPreflight\] result=PASS' "$log"
    grep -q '\[DeploymentVersion\] result=PASS' "$log"
    grep -Eq '\[GpuBackend\].*glVendor=ARM.*glRenderer=Mali-LODX.*hardwareGpu=1' "$log"

    drain_line=$(grep -E '\[OutputRoundDrain\] reason=stop .*targetFrames=[0-9]+.*completedFrames=[0-9]+' "$log" | tail -1 || true)
    video_line=$(grep -E '\[DdsVideoPerf\].*sentSamples=[0-9]+.*sentBytes=[0-9]+' "$log" | tail -1 || true)
    [ -n "$drain_line" ] && [ -n "$video_line" ]
    target_frames=$(field_from_line targetFrames "$drain_line")
    completed_frames=$(field_from_line completedFrames "$drain_line")
    sent_samples=$(field_from_line sentSamples "$video_line")
    sent_bytes=$(field_from_line sentBytes "$video_line")
    write_errors=$(field_from_line writeErrors "$video_line")
    dropped_samples=$(field_from_line droppedSamples "$video_line")
    for count in "$target_frames" "$completed_frames" "$sent_samples" "$sent_bytes" "$write_errors" "$dropped_samples"; do
        case "$count" in ''|*[!0-9]*) return 1;; esac
    done
    [ "$target_frames" -gt 0 ]
    [ "$completed_frames" -eq "$target_frames" ]
    [ "$sent_samples" -eq "$completed_frames" ]
    [ "$sent_bytes" -gt 0 ]
    [ "$write_errors" -eq 0 ]
    [ "$dropped_samples" -eq 0 ]
    printf '[P11RollbackLoop] release=%s result=PASS protocolBand=%s targetFrames=%s completedFrames=%s sentSamples=%s sentBytes=%s writeErrors=%s droppedSamples=%s\n' \
        "$release" "$protocol_band" "$target_frames" "$completed_frames" "$sent_samples" "$sent_bytes" "$write_errors" "$dropped_samples"
}

run_render_send_loop()
{
    release=$1
    protocol_band=$2
    log=$3
    run_status=0
    (
        cd "$root"
        HwaSimIRDdsVideoEnable=true \
        HwaSimIRDdsVideoCodec=auto \
        HwaSimIRDdsVideoQosFile=Config/DDS/ZRDDS_QOS_RK3588_192.168.1.116.xml \
        HwaSimIRLocalRecordingEnable=false \
        HwaSimIRExitOnStop=true \
        TcpSendVideo=false \
        TcpSendAnnotation=false \
        TcpSendRealtimeData=false \
        TcpForwardInitControl=false \
        H264Encoder=mpp \
        H264FallbackToJpeg=false \
        AsyncInputPolicy=OrderedQueue \
        EnablePerfLog=true \
        RenderPresentationMode=HeadlessOffscreen \
        timeout --signal=TERM "$((duration_sec + loop_wait_sec))s" ./run_precise.sh
    ) >"$log" 2>&1 &
    owned_launcher_pid=$!

    ready=0
    ready_wait=0
    while [ "$ready_wait" -lt 120 ]; do
        if grep -Eq "$fatal_pattern" "$log" 2>/dev/null; then
            break
        fi
        if grep -q '\[DdsVideo\] initialized=1' "$log" 2>/dev/null; then
            ready=1
            break
        fi
        kill -0 "$owned_launcher_pid" 2>/dev/null || break
        sleep 1
        ready_wait=$((ready_wait + 1))
    done
    if [ "$ready" -ne 1 ]; then
        wait "$owned_launcher_pid" 2>/dev/null || true
        owned_launcher_pid=
        echo "[P11RollbackLoop][FATAL] release=$release renderer_not_ready log=$log" >&2
        return 1
    fi
    echo "[P11RollbackLoopReady] release=$release result=PASS protocolBand=$protocol_band log=$log"

    set +e
    wait "$owned_launcher_pid"
    run_status=$?
    set -e
    owned_launcher_pid=
    [ "$run_status" -eq 0 ] || [ "$run_status" -eq 124 ] || [ "$run_status" -eq 143 ]
    ! pgrep -x HwaSim_IR >/dev/null
    validate_render_send_loop "$release" "$protocol_band" "$log"
}

hash_component()
{
    suffix=$1
    explicit_elf_sha256=$2
    explicit_manifest_sha256=$3
    config=$root/Config$suffix
    version=$config/deployment_version.env
    [ -f "$version" ]
    (cd "$config" && sha256sum -c deployment_manifest.sha256 >/dev/null)
    for pair in ElfSha256:HwaSim_IR LauncherSha256:run_precise.sh PerformanceToolSha256:rk3588_hwasimir_performance_mode.sh; do
        key=${pair%%:*}
        name=${pair#*:}
        expected=$(awk -F= -v wanted="$key" '$1==wanted {print $2; exit}' "$version")
        actual=$(sha256sum "$root/$name$suffix" | awk '{print $1}')
        [ -n "$expected" ] && [ "$expected" = "$actual" ]
        if [ "$name" = HwaSim_IR ]; then
            [ "$actual" = "$explicit_elf_sha256" ]
        fi
        printf '[P11RollbackIdentity] suffix=%s component=%s sha256=%s result=PASS\n' "${suffix:-active}" "$name" "$actual"
    done
    expected_manifest=$(awk -F= '$1=="ConfigManifestSha256" {print $2; exit}' "$version")
    actual_manifest=$(sha256sum "$config/deployment_manifest.sha256" | awk '{print $1}')
    [ -n "$expected_manifest" ] && [ "$expected_manifest" = "$actual_manifest" ]
    [ "$actual_manifest" = "$explicit_manifest_sha256" ]
    printf '[P11RollbackIdentity] suffix=%s component=Config manifest=%s result=PASS\n' "${suffix:-active}" "$actual_manifest"
}

restore_p11_component()
{
    name=$1
    active=$root/$name
    backup=$root/$name$backup_suffix
    saved=$root/$name$restore_suffix
    [ -e "$saved" ] || return 0
    if [ -e "$active" ]; then
        if [ -e "$backup" ]; then
            echo "[P11RollbackRecovery][FATAL] ambiguous component=$name" >&2
            return 1
        fi
        mv "$active" "$backup"
    fi
    mv "$saved" "$active"
    echo "[P11RollbackRecovery] component=$name result=PASS"
}

restore_p11()
{
    status=$?
    trap - EXIT INT TERM HUP
    if [ -n "$owned_launcher_pid" ]; then
        kill -TERM "$owned_launcher_pid" 2>/dev/null || true
    fi
    pkill -TERM -x HwaSim_IR 2>/dev/null || true
    for wait_index in 1 2 3 4 5; do
        pgrep -x HwaSim_IR >/dev/null || break
        sleep 1
    done
    recovery_status=0
    for name in $components; do
        restore_p11_component "$name" || recovery_status=1
    done
    rmdir "$marker" 2>/dev/null || true
    if [ "$phase" != done ]; then
        echo "[P11RollbackRecovery] interruptedPhase=$phase originalStatus=$status recoveryStatus=$recovery_status" >&2
    fi
    [ "$recovery_status" -eq 0 ] || exit 91
    exit "$status"
}
trap restore_p11 EXIT INT TERM HUP

! pgrep -x HwaSim_IR >/dev/null
for name in $components; do
    [ -e "$root/$name" ]
    [ -e "$root/$name$backup_suffix" ]
    [ ! -e "$root/$name$restore_suffix" ]
done
printf '[P11RollbackBegin] boardUtc='; date -u +%Y-%m-%dT%H:%M:%SZ
echo " rollbackSuffix=$rollback_suffix rollbackElfSha256=$rollback_elf_sha256 rollbackConfigManifestSha256=$rollback_config_manifest_sha256 rollbackProtocolBand=$rollback_protocol_band restoredProtocolBand=$restored_protocol_band"
hash_component '' "$active_elf_sha256" "$active_config_manifest_sha256"
hash_component "$backup_suffix" "$rollback_elf_sha256" "$rollback_config_manifest_sha256"

phase=switch_to_rollback
for name in $components; do
    mv "$root/$name" "$root/$name$restore_suffix"
    mv "$root/$name$backup_suffix" "$root/$name"
done
phase=rollback_active
hash_component '' "$rollback_elf_sha256" "$rollback_config_manifest_sha256"

rollback_log=$evidence/rollback_startup.log
run_render_send_loop rollback "$rollback_protocol_band" "$rollback_log"
echo "[P11RollbackStartup] release=rollback result=PASS evidence=render_send_positive protocolBand=$rollback_protocol_band log=$rollback_log"

phase=switch_back_to_p11
for name in $components; do
    mv "$root/$name" "$root/$name$backup_suffix"
    mv "$root/$name$restore_suffix" "$root/$name"
done
phase=p11_restored
hash_component '' "$active_elf_sha256" "$active_config_manifest_sha256"
hash_component "$backup_suffix" "$rollback_elf_sha256" "$rollback_config_manifest_sha256"

p11_log=$evidence/p11_restored_startup.log
run_render_send_loop p11_restored "$restored_protocol_band" "$p11_log"
echo "[P11RollbackStartup] release=p11_restored result=PASS evidence=render_send_positive protocolBand=$restored_protocol_band log=$p11_log"

phase=done
rmdir "$marker"
trap - EXIT INT TERM HUP
printf '[P11RollbackFinal] result=PASS boardUtc='; date -u +%Y-%m-%dT%H:%M:%SZ
echo " stageId=$stage_id rollbackSuffix=$rollback_suffix rollbackSnapshotRetained=1 p11Restored=1"
