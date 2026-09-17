#!/bin/sh

# Board-side owner for one P11 formal SWIR/MWIR acceptance run.  The Windows
# orchestrator starts this script through a foreground SSH process, so the
# launcher, renderer and sampler all have one auditable owner and trap.
set -eu

case_dir=$1
band=$2
protocol_band=$3
duration_sec=$4
expected_elf=$5
expected_manifest=$6
presentation_mode=${7:-HeadlessOffscreen}
linear_diagnostic_seqs=${8:-180,900,1800,2700,3540}
scenario_environment_file=${9:-}
scenario_environment_sha256=${10:-}
app_root=/userdata/HwaSimIR

case "$case_dir" in
    "$app_root"/logs/P11_acceptance_*/SWIR/board|"$app_root"/logs/P11_acceptance_*/MWIR/board) ;;
    *) echo "[P11BoardRun][FATAL] unsafe_case_dir=$case_dir" >&2; exit 2 ;;
esac
case "$band:$protocol_band" in
    SWIR:0|MWIR:2) ;;
    *) echo "[P11BoardRun][FATAL] invalid_band=$band protocolBand=$protocol_band" >&2; exit 2 ;;
esac
case "$duration_sec" in *[!0-9]*|'') echo "[P11BoardRun][FATAL] invalid_duration=$duration_sec" >&2; exit 2;; esac
[ "$duration_sec" -ge 10 ] || { echo "[P11BoardRun][FATAL] duration_too_short=$duration_sec" >&2; exit 2; }
case "$presentation_mode" in
    HeadlessOffscreen|VisibleWindow) ;;
    *) echo "[P11BoardRun][FATAL] invalid_presentation_mode=$presentation_mode" >&2; exit 2 ;;
esac
case "$linear_diagnostic_seqs" in
    ''|*[!0-9,]*|,*|*,|*,,*) echo "[P11BoardRun][FATAL] invalid_linear_diagnostic_seqs=$linear_diagnostic_seqs" >&2; exit 2 ;;
esac

mkdir -p "$case_dir/input_audit" "$case_dir/linear"
runner_pid_file="$case_dir/runner.pid"
launcher_pid_file="$case_dir/launcher.pid"
sampler_pid_file="$case_dir/sampler.pid"
printf '%s\n' $$ > "$runner_pid_file"

actual_elf=$(sha256sum "$app_root/HwaSim_IR" | awk '{print $1}')
actual_manifest=$(sha256sum "$app_root/Config/deployment_manifest.sha256" | awk '{print $1}')
[ "$actual_elf" = "$expected_elf" ] || {
    echo "[P11BoardRun][FATAL] elf_hash_mismatch expected=$expected_elf actual=$actual_elf" >&2
    exit 3
}
[ "$actual_manifest" = "$expected_manifest" ] || {
    echo "[P11BoardRun][FATAL] manifest_hash_mismatch expected=$expected_manifest actual=$actual_manifest" >&2
    exit 3
}
[ -x "$app_root/run_precise.sh" ]
[ -x "$app_root/rk3588_hwasimir_perf_sample.sh" ]
! pgrep -x HwaSim_IR >/dev/null || {
    echo "[P11BoardRun][FATAL] stale_hwasimir_process" >&2
    exit 4
}

{
    echo "schema=hwasimir.p11.rk3588.board-clock.v1"
    echo "band=$band"
    echo "protocolBand=$protocol_band"
    echo "boardUtc=$(date -u +%Y-%m-%dT%H:%SZ)"
    echo "boardRealtimeNs=$(date +%s%N)"
    echo "boardUptimeSec=$(cut -d' ' -f1 /proc/uptime)"
    echo "elfSha256=$actual_elf"
    echo "configManifestSha256=$actual_manifest"
} > "$case_dir/board_clock_begin.txt"
cp "$app_root/Config/deployment_version.env" "$case_dir/deployment_version.env"

export HwaSimIRDdsVideoEnable=true
export HwaSimIRDdsVideoCodec=auto
export HwaSimIRDdsVideoQosFile=Config/DDS/ZRDDS_QOS_RK3588_192.168.1.116.xml
export HwaSimIRLocalRecordingEnable=false
export HwaSimIRExitOnStop=true
export HwaInputAuditDirectory="$case_dir/input_audit"
export LinearDiagnosticPath="$case_dir/linear/raw_radiance"
export LinearDiagnosticSeqs="$linear_diagnostic_seqs"
export TcpSendVideo=false
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
export NaturalSolarEnableOpticalShadow=true
export NaturalSolarEnableSolarThermal=true
export NaturalSolarDebugLog=true
export EnableAGC=true
export AnnotationOverlayInSensorImage=false

# An image-matrix caller may supply a narrowly-scoped, content-addressed list
# of physics/display overrides.  The ordinary acceptance path supplies no
# ninth argument, so its runtime contract remains byte-for-byte equivalent.
# Do not source the file: parse literal KEY=VALUE records and reject every
# unknown key/value character so the evidence file cannot become shell code or
# alter DDS ownership, executable paths, output paths, or the TCP-disable gate.
if [ -n "$scenario_environment_file" ] || [ -n "$scenario_environment_sha256" ]; then
    case "$scenario_environment_file" in
        /tmp/p11_rk3588_dds_image_env_*.env) ;;
        *) echo "[P11BoardRun][FATAL] unsafe_scenario_environment_file=$scenario_environment_file" >&2; exit 2 ;;
    esac
    case "$scenario_environment_sha256" in
        ''|*[!0-9a-f]* ) echo "[P11BoardRun][FATAL] invalid_scenario_environment_sha256=$scenario_environment_sha256" >&2; exit 2 ;;
    esac
    [ "${#scenario_environment_sha256}" -eq 64 ] || {
        echo "[P11BoardRun][FATAL] invalid_scenario_environment_sha256_length=${#scenario_environment_sha256}" >&2
        exit 2
    }
    actual_scenario_environment_sha256=$(sha256sum "$scenario_environment_file" | awk '{print $1}')
    [ "$actual_scenario_environment_sha256" = "$scenario_environment_sha256" ] || {
        echo "[P11BoardRun][FATAL] scenario_environment_hash_mismatch expected=$scenario_environment_sha256 actual=$actual_scenario_environment_sha256" >&2
        exit 3
    }
    while IFS='=' read -r scenario_key scenario_value; do
        [ -n "$scenario_key" ] || continue
        case "$scenario_key" in
            EnableAGC|AnnotationOverlayInSensorImage|NaturalSolarEnable|NaturalSolarEnableOpticalShadow|NaturalSolarEnableSolarThermal|NaturalSolarDebugLog|M1SolarOverrideEnable|M1SolarOverrideAzimuthDeg|M1SolarOverrideElevationDeg|M1SunVisibility|M1FallbackUtcDate|ActiveIlluminatorEnable|ActiveIlluminatorBand|ActiveIlluminatorCenterWavelengthUm|ActiveIlluminatorBandwidthUm|ActiveIlluminatorIntensityMode|ActiveIlluminatorDebugLog|EnableAeroThermalModel|ApplyAeroToRadiance|AeroApplyOnlyBand|PostprocessAA) ;;
            *) echo "[P11BoardRun][FATAL] unsupported_scenario_environment_key=$scenario_key" >&2; exit 2 ;;
        esac
        case "$scenario_value" in
            ''|*[!A-Za-z0-9_.,/+:-]*) echo "[P11BoardRun][FATAL] unsafe_scenario_environment_value key=$scenario_key" >&2; exit 2 ;;
        esac
        export "$scenario_key=$scenario_value"
    done < "$scenario_environment_file"
    echo "[P11ScenarioEnvironment] result=PASS sha256=$actual_scenario_environment_sha256 path=$scenario_environment_file"
fi

{
    echo "schema=hwasimir.p11.rk3588.runtime-environment.v1"
    echo "band=$band"
    echo "protocolBand=$protocol_band"
    echo "presentationMode=$presentation_mode"
    env | LC_ALL=C sort | grep -E '^(Hwa|Linear|Tcp|Render|H264|Enable|Stage5|M1|Natural|Annotation|Active|Aero|ApplyAero|Postprocess)' || true
} > "$case_dir/runtime_environment.txt"

launcher_pid=
sampler_pid=
stop_owned_child()
{
    child_pid=$1
    [ -n "$child_pid" ] || return 0
    kill -0 "$child_pid" 2>/dev/null || { wait "$child_pid" 2>/dev/null || true; return 0; }
    kill -TERM "$child_pid" 2>/dev/null || true
    child_wait=0
    while kill -0 "$child_pid" 2>/dev/null && [ "$child_wait" -lt 10 ]; do
        sleep 1
        child_wait=$((child_wait + 1))
    done
    if kill -0 "$child_pid" 2>/dev/null; then
        kill -KILL "$child_pid" 2>/dev/null || true
    fi
    wait "$child_pid" 2>/dev/null || true
}
cleanup()
{
    status=$?
    trap - EXIT INT TERM HUP
    stop_owned_child "$launcher_pid"
    stop_owned_child "$sampler_pid"
    {
        echo "boardUtc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo "boardRealtimeNs=$(date +%s%N)"
        echo "exitStatus=$status"
        echo "hwasimirStillRunning=$(pgrep -x HwaSim_IR >/dev/null && echo 1 || echo 0)"
    } > "$case_dir/board_clock_end.txt"
    printf '%s\n' "$status" > "$case_dir/board_runner_exit.txt"
    exit "$status"
}
trap cleanup EXIT INT TERM HUP

"$app_root/rk3588_hwasimir_perf_sample.sh" "$case_dir/performance.csv" "$case_dir/hwa.log" $((duration_sec + 45)) 500 >"$case_dir/performance_sampler.log" 2>&1 &
sampler_pid=$!
printf '%s\n' "$sampler_pid" > "$sampler_pid_file"

cd "$app_root"
./run_precise.sh >"$case_dir/hwa.log" 2>&1 &
launcher_pid=$!
printf '%s\n' "$launcher_pid" > "$launcher_pid_file"
set +e
wait "$launcher_pid"
renderer_status=$?
set -e
launcher_pid=
printf '%s\n' "$renderer_status" > "$case_dir/hwa_exit.txt"
if kill -0 "$sampler_pid" 2>/dev/null; then
    kill -TERM "$sampler_pid" 2>/dev/null || true
fi
wait "$sampler_pid" 2>/dev/null || true
sampler_pid=
exit "$renderer_status"
