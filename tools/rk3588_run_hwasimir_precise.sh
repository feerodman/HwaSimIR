#!/bin/sh
set -eu

app_root=${HWASIMIR_ROOT:-/userdata/HwaSimIR}
channel=${HWASIMIR_CHANNEL:-precise}
network_config=${HWASIMIR_NETWORK_CONFIG:-$app_root/Config/NetworkConfig_${channel}.ini}
udp_port=${HWASIMIR_UDP_PORT:-8888}
qos_file=${HwaSimIRDdsVideoQosFile:-Config/DDS/ZRDDS_QOS_PROFILES.xml}
performance_tool=$app_root/rk3588_hwasimir_performance_mode.sh
performance_policy=$app_root/Config/RK3588PerformancePolicy.conf
manifest_file=$app_root/Config/deployment_manifest.sha256
version_file=$app_root/Config/deployment_version.env
verify_full=false
launcher_begin_ns=$(date +%s%N)

if [ "${1:-}" = "--verify-full" ]; then
    verify_full=true
    shift
fi

elapsed_ms()
{
    now_ns=$(date +%s%N)
    awk -v begin="$launcher_begin_ns" -v now="$now_ns" 'BEGIN { printf "%.3f", (now - begin) / 1000000.0 }'
}

case "$qos_file" in
    /*) qos_resolved=$qos_file ;;
    *) qos_resolved=$app_root/$qos_file ;;
esac

fatal()
{
    echo "[RunPreflight][FATAL] $*" >&2
    exit 10
}

local_display=${HWASIMIR_LOCAL_DISPLAY:-:0}
xorg_pid=$(pgrep -o -f "[X]org[[:space:]]+$local_display([[:space:]]|$)" 2>/dev/null || true)
if [ -z "$xorg_pid" ]; then
    fatal 'component=Xorg display=:0 reason=not_running'
fi
xorg_authority=$(tr '\000' ' ' < "/proc/$xorg_pid/cmdline" | awk '
    { for (i = 1; i <= NF; ++i) if ($i == "-auth" && i < NF) { print $(i + 1); exit } }')
local_xauthority=${HWASIMIR_LOCAL_XAUTHORITY:-$xorg_authority}
[ -n "$local_xauthority" ] || fatal "component=Xorg display=$local_display reason=authority_not_declared"
[ -r "$local_xauthority" ] || fatal "component=Xorg display=$local_display reason=authority_not_readable path=$local_xauthority"
inherited_display=${DISPLAY:-unset}
export DISPLAY=$local_display
export XAUTHORITY=$local_xauthority
echo "[RunDisplay] inheritedDisplay=$inherited_display effectiveLocalDisplay=$DISPLAY authorityPath=$XAUTHORITY xorgPid=$xorg_pid"
[ -e /dev/mali0 ] || fatal 'component=Mali reason=device_missing path=/dev/mali0'
[ -x "$app_root/HwaSim_IR" ] || fatal "component=deployment reason=binary_missing path=$app_root/HwaSim_IR"
[ -f "$network_config" ] || fatal "component=deployment reason=network_config_missing path=$network_config"
[ -f "$qos_resolved" ] || fatal "component=DDS reason=qos_file_not_found path=$qos_resolved"
[ -x "$performance_tool" ] || fatal "component=performance_policy reason=tool_missing path=$performance_tool"
[ -f "$performance_policy" ] || fatal "component=performance_policy reason=config_missing path=$performance_policy"
[ -f "$manifest_file" ] || fatal "component=deployment reason=manifest_missing path=$manifest_file"
[ -f "$version_file" ] || fatal "component=deployment reason=version_file_missing path=$version_file"

command_transport=${HwaSimIRCommandTransportInput:-$(awk '
    /^[[:space:]]*\[CommandTransport\][[:space:]]*$/ { section=1; next }
    /^[[:space:]]*\[/ { section=0 }
    section && /^[[:space:]]*Input[[:space:]]*=/ {
        sub(/^[^=]*=/, ""); gsub(/[[:space:]]/, ""); print tolower($0); exit
    }' "$app_root/Config/HwaSimIRRuntime.ini")}
[ -n "$command_transport" ] || command_transport=dds
case "$command_transport" in
    udp|both)
        if ss -H -lunp 2>/dev/null | awk -v port=":$udp_port" '$4 ~ port"$" { found=1 } END { exit !found }'; then
            echo "[RunPreflight][FATAL] component=UDP localPort=$udp_port reason=already_in_use commandTransport=$command_transport" >&2
            ss -lunp 2>/dev/null | grep ":$udp_port" >&2 || true
            exit 14
        fi
        ;;
    dds) ;;
    *) fatal "component=CommandTransport reason=invalid_input value=$command_transport" ;;
esac

if [ "$verify_full" = true ]; then
    verify_begin_ns=$(date +%s%N)
    if ! (cd "$app_root/Config" && sha256sum -c deployment_manifest.sha256 >/tmp/hwasimir_manifest_check.log 2>&1); then
        cat /tmp/hwasimir_manifest_check.log >&2
        fatal "component=deployment reason=config_manifest_mismatch manifest=$manifest_file"
    fi
    verify_end_ns=$(date +%s%N)
    verify_ms=$(awk -v begin="$verify_begin_ns" -v now="$verify_end_ns" 'BEGIN { printf "%.3f", (now - begin) / 1000000.0 }')
    echo "[RunIntegrity] mode=full result=PASS elapsedMs=$verify_ms manifest=$manifest_file"
else
    echo "[RunIntegrity] mode=quick result=PASS fullConfigHashScan=0 hint=use_--verify-full_for_explicit_scan"
fi

expected_elf=$(awk -F= '$1=="ElfSha256" {print $2; exit}' "$version_file")
expected_manifest=$(awk -F= '$1=="ConfigManifestSha256" {print $2; exit}' "$version_file")
expected_launcher=$(awk -F= '$1=="LauncherSha256" {print $2; exit}' "$version_file")
expected_performance_tool=$(awk -F= '$1=="PerformanceToolSha256" {print $2; exit}' "$version_file")
actual_manifest=$(sha256sum "$manifest_file" | awk '{print $1}')
[ -n "$expected_manifest" ] && [ "$actual_manifest" = "$expected_manifest" ] || fatal "component=deployment reason=manifest_hash_mismatch expected=$expected_manifest actual=$actual_manifest"
git_commit=$(awk -F= '$1=="GitCommit" {print $2; exit}' "$version_file")
source_identity=$(awk -F= '$1=="SourceIdentity" {print $2; exit}' "$version_file")
build_id=$(awk -F= '$1=="BuildId" {print $2; exit}' "$version_file")
echo "[DeploymentVersion] result=PASS verification=quick gitCommit=$git_commit sourceIdentity=$source_identity expectedElfSha256=$expected_elf buildId=$build_id expectedLauncherSha256=$expected_launcher expectedPerformanceToolSha256=$expected_performance_tool runtimeConfigSha256=$(sha256sum "$app_root/Config/HwaSimIRRuntime.ini" | awk '{print $1}') configManifestSha256=$actual_manifest versionFile=$version_file"

export PANDA3D_ROOT=${PANDA3D_ROOT:-/opt/panda3d-aarch64}
export PRC_DIR=${PRC_DIR:-$PANDA3D_ROOT/etc}
export PANDA_PRC_DIR=${PANDA_PRC_DIR:-$PANDA3D_ROOT/etc}
export ZRDDS_HOME=${ZRDDS_HOME:-/usr/ZRDDS/ZRDDS-2.4.5}
export LD_LIBRARY_PATH=$ZRDDS_HOME/lib:$PANDA3D_ROOT/lib:/usr/lib/aarch64-linux-gnu/mali:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export HWASIMIR_PERF_POLICY_FILE=$performance_policy
unset EGL_PLATFORM
unset LIBGL_ALWAYS_SOFTWARE
unset MESA_LOADER_DRIVER_OVERRIDE

echo "[RunPreflight] result=PASS channel=$channel commandTransport=$command_transport display=$DISPLAY authorityPath=$XAUTHORITY maliDevice=/dev/mali0 qos=$qos_resolved networkConfig=$network_config udpPort=$udp_port elapsedMs=$(elapsed_ms)"
policy_begin_ns=$(date +%s%N)
"$performance_tool" apply
policy_end_ns=$(date +%s%N)
policy_ms=$(awk -v begin="$policy_begin_ns" -v now="$policy_end_ns" 'BEGIN { printf "%.3f", (now - begin) / 1000000.0 }')
process_nice=$(awk -F= '$1=="ProcessNice" {gsub(/[[:space:]]/, "", $2); print $2; exit}' "$performance_policy")
[ -n "$process_nice" ] || process_nice=0
case "$process_nice" in
    -[0-9]|-1[0-9]|-20|[0-9]|1[0-9]) ;;
    *) fatal "component=performance_policy reason=invalid_process_nice value=$process_nice allowed=-20..19" ;;
esac
echo "[BoardPerformancePolicy] result=PASS processNice=$process_nice action=launch_hwasimir policySource=$performance_policy elapsedMs=$policy_ms launcherElapsedMs=$(elapsed_ms)"

restore_on_exit=$(awk -F= '$1=="RestoreOnExit" {gsub(/[[:space:]]/, "", $2); print tolower($2); exit}' "$performance_policy")
app_pid=''
cleanup()
{
    status=$?
    trap - EXIT INT TERM HUP
    if [ -n "$app_pid" ] && kill -0 "$app_pid" 2>/dev/null; then
        kill -TERM "$app_pid" 2>/dev/null || true
        wait "$app_pid" 2>/dev/null || true
    fi
    if [ "$restore_on_exit" = true ] || [ "$restore_on_exit" = 1 ] || [ "$restore_on_exit" = yes ]; then
        "$performance_tool" restore || echo "[BoardPerformancePolicy][ERROR] reason=restore_failed policySource=$performance_policy" >&2
    else
        echo "[BoardPerformancePolicy] result=PASS action=preserve_until_shutdown policySource=$performance_policy"
    fi
    exit "$status"
}
trap cleanup EXIT INT TERM HUP

cd "$app_root"
nice -n "$process_nice" ./HwaSim_IR --channel "$channel" --network-config "$network_config" "$@" &
app_pid=$!
wait "$app_pid"
app_status=$?
app_pid=''
exit "$app_status"
