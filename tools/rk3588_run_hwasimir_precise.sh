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

case "$qos_file" in
    /*) qos_resolved=$qos_file ;;
    *) qos_resolved=$app_root/$qos_file ;;
esac

fatal()
{
    echo "[RunPreflight][FATAL] $*" >&2
    exit 10
}

if ! ps -ef | grep '[X]org :0' >/dev/null 2>&1; then
    fatal 'component=Xorg display=:0 reason=not_running'
fi
[ -e /dev/mali0 ] || fatal 'component=Mali reason=device_missing path=/dev/mali0'
[ -x "$app_root/HwaSim_IR" ] || fatal "component=deployment reason=binary_missing path=$app_root/HwaSim_IR"
[ -f "$network_config" ] || fatal "component=deployment reason=network_config_missing path=$network_config"
[ -f "$qos_resolved" ] || fatal "component=DDS reason=qos_file_not_found path=$qos_resolved"
[ -x "$performance_tool" ] || fatal "component=performance_policy reason=tool_missing path=$performance_tool"
[ -f "$performance_policy" ] || fatal "component=performance_policy reason=config_missing path=$performance_policy"
[ -f "$manifest_file" ] || fatal "component=deployment reason=manifest_missing path=$manifest_file"
[ -f "$version_file" ] || fatal "component=deployment reason=version_file_missing path=$version_file"

if ss -H -lunp 2>/dev/null | awk -v port=":$udp_port" '$4 ~ port"$" { found=1 } END { exit !found }'; then
    echo "[RunPreflight][FATAL] component=UDP localPort=$udp_port reason=already_in_use" >&2
    ss -lunp 2>/dev/null | grep ":$udp_port" >&2 || true
    exit 14
fi

if ! (cd "$app_root/Config" && sha256sum -c deployment_manifest.sha256 >/tmp/hwasimir_manifest_check.log 2>&1); then
    cat /tmp/hwasimir_manifest_check.log >&2
    fatal "component=deployment reason=config_manifest_mismatch manifest=$manifest_file"
fi

expected_elf=$(awk -F= '$1=="ElfSha256" {print $2; exit}' "$version_file")
expected_manifest=$(awk -F= '$1=="ConfigManifestSha256" {print $2; exit}' "$version_file")
expected_launcher=$(awk -F= '$1=="LauncherSha256" {print $2; exit}' "$version_file")
expected_performance_tool=$(awk -F= '$1=="PerformanceToolSha256" {print $2; exit}' "$version_file")
actual_elf=$(sha256sum "$app_root/HwaSim_IR" | awk '{print $1}')
actual_manifest=$(sha256sum "$manifest_file" | awk '{print $1}')
actual_launcher=$(sha256sum "$app_root/run_precise.sh" | awk '{print $1}')
actual_performance_tool=$(sha256sum "$performance_tool" | awk '{print $1}')
[ -n "$expected_elf" ] && [ "$actual_elf" = "$expected_elf" ] || fatal "component=deployment reason=elf_hash_mismatch expected=$expected_elf actual=$actual_elf"
[ -n "$expected_manifest" ] && [ "$actual_manifest" = "$expected_manifest" ] || fatal "component=deployment reason=manifest_hash_mismatch expected=$expected_manifest actual=$actual_manifest"
[ -n "$expected_launcher" ] && [ "$actual_launcher" = "$expected_launcher" ] || fatal "component=deployment reason=launcher_hash_mismatch expected=$expected_launcher actual=$actual_launcher"
[ -n "$expected_performance_tool" ] && [ "$actual_performance_tool" = "$expected_performance_tool" ] || fatal "component=deployment reason=performance_tool_hash_mismatch expected=$expected_performance_tool actual=$actual_performance_tool"
git_commit=$(awk -F= '$1=="GitCommit" {print $2; exit}' "$version_file")
source_identity=$(awk -F= '$1=="SourceIdentity" {print $2; exit}' "$version_file")
build_id=$(awk -F= '$1=="BuildId" {print $2; exit}' "$version_file")
echo "[DeploymentVersion] result=PASS gitCommit=$git_commit sourceIdentity=$source_identity elfSha256=$actual_elf buildId=$build_id runtimeConfigSha256=$(sha256sum "$app_root/Config/HwaSimIRRuntime.ini" | awk '{print $1}') configManifestSha256=$actual_manifest versionFile=$version_file"

export PANDA3D_ROOT=${PANDA3D_ROOT:-/opt/panda3d-aarch64}
export PRC_DIR=${PRC_DIR:-$PANDA3D_ROOT/etc}
export PANDA_PRC_DIR=${PANDA_PRC_DIR:-$PANDA3D_ROOT/etc}
export DISPLAY=${DISPLAY:-:0}
export XAUTHORITY=${XAUTHORITY:-/root/.Xauthority}
export ZRDDS_HOME=${ZRDDS_HOME:-/usr/ZRDDS/ZRDDS-2.4.5}
export LD_LIBRARY_PATH=$ZRDDS_HOME/lib:$PANDA3D_ROOT/lib:/usr/lib/aarch64-linux-gnu/mali:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export HWASIMIR_PERF_POLICY_FILE=$performance_policy
unset EGL_PLATFORM
unset LIBGL_ALWAYS_SOFTWARE
unset MESA_LOADER_DRIVER_OVERRIDE

echo "[RunPreflight] result=PASS channel=$channel display=$DISPLAY maliDevice=/dev/mali0 qos=$qos_resolved networkConfig=$network_config udpPort=$udp_port"
"$performance_tool" apply
process_nice=$(awk -F= '$1=="ProcessNice" {gsub(/[[:space:]]/, "", $2); print $2; exit}' "$performance_policy")
[ -n "$process_nice" ] || process_nice=0
case "$process_nice" in
    -[0-9]|-1[0-9]|-20|[0-9]|1[0-9]) ;;
    *) fatal "component=performance_policy reason=invalid_process_nice value=$process_nice allowed=-20..19" ;;
esac
echo "[BoardPerformancePolicy] result=PASS processNice=$process_nice action=launch_hwasimir policySource=$performance_policy"

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
