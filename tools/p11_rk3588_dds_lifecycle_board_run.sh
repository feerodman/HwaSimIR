#!/bin/sh

# Board-side owner for one DDS-only lifecycle case.  The controller owns this
# process over foreground SSH; every child PID and the intentional termination
# of a retained renderer are therefore auditable and bounded.
set -eu

case_dir=$1
expected_elf=$2
expected_manifest=$3
exit_on_stop=$4
timeout_sec=$5
presentation_mode=${6:-HeadlessOffscreen}
app_root=/userdata/HwaSimIR

case "$case_dir" in
    "$app_root"/logs/P11_dds_lifecycle_*/pause_swir/board|\
    "$app_root"/logs/P11_dds_lifecycle_*/pause_mwir/board|\
    "$app_root"/logs/P11_dds_lifecycle_*/receiver_restart/board|\
    "$app_root"/logs/P11_dds_lifecycle_*/retained_reinit/board) ;;
    *) echo "[P11DdsLifecycleBoard][FATAL] unsafe_case_dir=$case_dir" >&2; exit 2 ;;
esac
case "$exit_on_stop" in true|false) ;; *) exit 2;; esac
case "$timeout_sec" in *[!0-9]*|'') exit 2;; esac
[ "$timeout_sec" -ge 20 ] || exit 2
case "$presentation_mode" in HeadlessOffscreen|VisibleWindow) ;; *) exit 2;; esac

mkdir -p "$case_dir/input_audit"
printf '%s\n' $$ > "$case_dir/runner.pid"
actual_elf=$(sha256sum "$app_root/HwaSim_IR" | awk '{print $1}')
actual_manifest=$(sha256sum "$app_root/Config/deployment_manifest.sha256" | awk '{print $1}')
[ "$actual_elf" = "$expected_elf" ]
[ "$actual_manifest" = "$expected_manifest" ]
[ -x "$app_root/run_precise.sh" ]
! pgrep -x HwaSim_IR >/dev/null

export HwaSimIRCommandTransportInput=dds
export HwaSimIRDdsVideoEnable=true
export HwaSimIRDdsVideoCodec=auto
export HwaSimIRDdsVideoQosFile=Config/DDS/ZRDDS_QOS_RK3588_192.168.1.116.xml
export HwaSimIRLocalRecordingEnable=false
export HwaSimIRExitOnStop="$exit_on_stop"
export HwaInputAuditDirectory="$case_dir/input_audit"
export TcpSendVideo=false
export TcpSendAnnotation=false
export TcpSendRealtimeData=false
export TcpForwardInitControl=false
export RenderPresentationMode="$presentation_mode"
export H264Encoder=mpp
export H264FallbackToJpeg=false
export EnablePerfLog=true
export Stage5RadianceLogComponents=true
export Stage5RadianceComponentLogEveryFrames=120
export M1CompareOnly=false
export M1EnableRuntime=true
export M1EnableNIRRuntime=true
export M1EnableSWIRRuntime=true
export M1EnableMWIRRuntime=true
export NaturalSolarEnable=true
export EnableAGC=true
export AnnotationOverlayInSensorImage=false

{
    echo 'schema=hwasimir.p11.rk3588.dds-lifecycle-board-environment.v1'
    echo 'transport=DDS'
    echo 'domain=150'
    echo "exitOnStop=$exit_on_stop"
    echo "presentationMode=$presentation_mode"
    echo "elfSha256=$actual_elf"
    echo "configManifestSha256=$actual_manifest"
    env | LC_ALL=C sort | grep -E '^(Hwa|Tcp|Render|H264|Enable|Stage5|M1|Natural|Annotation)' || true
} > "$case_dir/runtime_environment.txt"

launcher_pid=
cleanup()
{
    status=$?
    trap - EXIT INT TERM HUP
    if [ -n "$launcher_pid" ] && kill -0 "$launcher_pid" 2>/dev/null; then
        kill -TERM "$launcher_pid" 2>/dev/null || true
        wait "$launcher_pid" 2>/dev/null || true
    fi
    still_running=$(pgrep -x HwaSim_IR >/dev/null && echo 1 || echo 0)
    {
        echo "exitStatus=$status"
        echo "hwasimirStillRunning=$still_running"
        echo "boardUtc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    } > "$case_dir/board_end.txt"
    [ "$still_running" = 0 ] || exit 41
    exit "$status"
}
trap cleanup EXIT INT TERM HUP

cd "$app_root"
./run_precise.sh >"$case_dir/hwa.log" 2>&1 &
launcher_pid=$!
printf '%s\n' "$launcher_pid" > "$case_dir/launcher.pid"

waited=0
while ! grep -F '[RunPreflight] result=PASS' "$case_dir/hwa.log" >/dev/null 2>&1; do
    kill -0 "$launcher_pid" 2>/dev/null || { echo '[P11DdsLifecycleBoard][FATAL] early_exit' >&2; exit 31; }
    [ "$waited" -lt 60 ] || { echo '[P11DdsLifecycleBoard][FATAL] preflight_timeout' >&2; exit 32; }
    sleep 1
    waited=$((waited + 1))
done
renderer_pid=$(pgrep -x HwaSim_IR | head -1)
[ -n "$renderer_pid" ]
printf '%s\n' "$renderer_pid" > "$case_dir/hwa.pid"
echo "[P11DdsLifecycleBoard] result=READY rendererPid=$renderer_pid transport=DDS domain=150 exitOnStop=$exit_on_stop"

if [ "$exit_on_stop" = true ]; then
    set +e
    wait "$launcher_pid"
    status=$?
    set -e
    launcher_pid=
    printf '%s\n' "$status" > "$case_dir/hwa_exit.txt"
    [ "$status" -eq 0 ] || exit "$status"
else
    elapsed=0
    while [ ! -f "$case_dir/controller.done" ]; do
        kill -0 "$launcher_pid" 2>/dev/null || { echo '[P11DdsLifecycleBoard][FATAL] retained_renderer_exited' >&2; exit 33; }
        [ "$elapsed" -lt "$timeout_sec" ] || { echo '[P11DdsLifecycleBoard][FATAL] controller_done_timeout' >&2; exit 34; }
        sleep 1
        elapsed=$((elapsed + 1))
    done
    # All protocol STOPs have already drained.  Terminate only the owned
    # launcher; its trap terminates the one retained renderer and restores the
    # board performance policy.
    kill -TERM "$launcher_pid" 2>/dev/null || true
    wait "$launcher_pid" 2>/dev/null || true
    launcher_pid=
    printf '%s\n' 0 > "$case_dir/hwa_exit.txt"
    printf '%s\n' 1 > "$case_dir/controller_terminated_after_drain.txt"
fi

exit 0
