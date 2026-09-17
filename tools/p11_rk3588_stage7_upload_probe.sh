#!/bin/sh
set -eu

case_dir=${1:?usage: p11_rk3588_stage7_upload_probe.sh CASE_DIR}
app_root=${2:-/userdata/HwaSimIR}
volume_enabled=${3:-true}
cloud_layer_enabled=${4:-true}
precipitation_enabled=${5:-true}
weather_enabled=${6:-true}
sky_enabled=${7:-true}

if pgrep -x HwaSim_IR >/dev/null 2>&1; then
    echo "[Stage7Probe][FATAL] preexisting_hwasimir_process" >&2
    exit 4
fi

test -f "$case_dir/prc/Config.prc"
test -x "$app_root/run_precise.sh"

cleanup()
{
    status=$?
    trap - EXIT INT TERM HUP
    residual=$(pgrep -x HwaSim_IR 2>/dev/null || true)
    if [ -n "$residual" ]; then
        echo "[Stage7Probe][ERROR] ownedResidual=$residual action=term" >>"$case_dir/hwa_debug.log"
        kill -TERM $residual 2>/dev/null || true
        sleep 2
    fi
    final_residual=$(pgrep -x HwaSim_IR 2>/dev/null || true)
    if [ -n "$final_residual" ]; then
        echo "[Stage7Probe][FATAL] ownedResidualAfterTerm=$final_residual" >>"$case_dir/hwa_debug.log"
        exit 5
    fi
    exit "$status"
}
trap cleanup EXIT INT TERM HUP

cd "$app_root"
set +e
PANDA_PRC_DIR="$case_dir/prc" \
PANDA_PRC_PATH="$case_dir/prc" \
Stage7VolumetricCloudEnable="$volume_enabled" \
Stage7EnableCloudLayer="$cloud_layer_enabled" \
Stage7EnablePrecipitation="$precipitation_enabled" \
EnableStage7WeatherEffects="$weather_enabled" \
EnableStage7SkyHorizon="$sky_enabled" \
timeout -s TERM -k 6s 4s ./run_precise.sh >"$case_dir/hwa_debug.log" 2>&1
launcher_status=$?
set -e
printf '%s\n' "$launcher_status" >"$case_dir/launcher_status.txt"
sleep 1

echo "[Stage7Probe] result=COMPLETE launcherStatus=$launcher_status caseDir=$case_dir volume=$volume_enabled cloudLayer=$cloud_layer_enabled precipitation=$precipitation_enabled weather=$weather_enabled sky=$sky_enabled"
grep -n -E 'Failed to convert image|glgsg|GL_|Texture|texture|image|Preparing|upload' "$case_dir/hwa_debug.log" | tail -300 || true
