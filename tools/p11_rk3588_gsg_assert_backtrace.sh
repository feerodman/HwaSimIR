#!/bin/sh
set -eu

app_root=${1:-/userdata/HwaSimIR}
network_config=${2:-$app_root/Config/NetworkConfig_precise.ini}

if pgrep -x HwaSim_IR >/dev/null 2>&1; then
    echo "[GsgAssertBacktrace][FATAL] preexisting_hwasimir_process" >&2
    exit 4
fi
command -v gdb >/dev/null 2>&1 || {
    echo "[GsgAssertBacktrace][FATAL] gdb_unavailable" >&2
    exit 5
}

local_display=${HWASIMIR_LOCAL_DISPLAY:-:0}
xorg_pid=$(pgrep -o -f "[X]org[[:space:]]+$local_display([[:space:]]|$)" 2>/dev/null || true)
if [ -z "$xorg_pid" ]; then
    echo "[GsgAssertBacktrace][FATAL] xorg_not_running display=$local_display" >&2
    exit 6
fi
xorg_authority=$(tr '\000' ' ' < "/proc/$xorg_pid/cmdline" | awk '
    { for (i = 1; i <= NF; ++i) if ($i == "-auth" && i < NF) { print $(i + 1); exit } }')
test -r "$xorg_authority"
test -x "$app_root/HwaSim_IR"
test -f "$network_config"

export DISPLAY=$local_display
export XAUTHORITY=$xorg_authority
export PANDA3D_ROOT=${PANDA3D_ROOT:-/opt/panda3d-aarch64}
export PRC_DIR=${PRC_DIR:-$PANDA3D_ROOT/etc}
export PANDA_PRC_DIR=${PANDA_PRC_DIR:-$PANDA3D_ROOT/etc}
export ZRDDS_HOME=${ZRDDS_HOME:-/usr/ZRDDS/ZRDDS-2.4.5}
export LD_LIBRARY_PATH=$ZRDDS_HOME/lib:$PANDA3D_ROOT/lib:/usr/lib/aarch64-linux-gnu/mali:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
unset EGL_PLATFORM
unset LIBGL_ALWAYS_SOFTWARE
unset MESA_LOADER_DRIVER_OVERRIDE

# Mirror the strict acceptance startup route up to its first idle do_frame.
# No protocol stimulus is required for the startup assertion itself.
export HwaSimIRDdsVideoEnable=true
export HwaSimIRDdsVideoCodec=auto
export HwaSimIRDdsVideoQosFile=Config/DDS/ZRDDS_QOS_RK3588_192.168.1.116.xml
export HwaSimIRLocalRecordingEnable=false
export HwaSimIRExitOnStop=true
export TcpSendVideo=false
export RenderPresentationMode=HeadlessOffscreen
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

cleanup()
{
    status=$?
    trap - EXIT INT TERM HUP
    residual=$(pgrep -x HwaSim_IR 2>/dev/null || true)
    if [ -n "$residual" ]; then
        echo "[GsgAssertBacktrace][ERROR] ownedResidual=$residual action=term" >&2
        kill -TERM $residual 2>/dev/null || true
        sleep 2
    fi
    final_residual=$(pgrep -x HwaSim_IR 2>/dev/null || true)
    if [ -n "$final_residual" ]; then
        echo "[GsgAssertBacktrace][FATAL] ownedResidualAfterTerm=$final_residual" >&2
        exit 7
    fi
    exit "$status"
}
trap cleanup EXIT INT TERM HUP

echo "[GsgAssertBacktrace] phase=begin display=$DISPLAY authority=$XAUTHORITY elf=$(sha256sum "$app_root/HwaSim_IR" | awk '{print $1}')"
cd "$app_root"
set +e
timeout -s INT -k 5s 90s gdb -q -batch \
    -ex 'set pagination off' \
    -ex 'set confirm off' \
    -ex 'set breakpoint pending on' \
    -ex 'set multiple-symbols all' \
    -ex 'start' \
    -ex 'break _ZL10copy_imagePhPKhmiii' \
    -ex 'continue' \
    -ex 'printf "[GsgAssertBacktrace] phase=copy_image_hit externalFormat=0x%x numComponents=%d componentWidth=%d imageBytes=%lu\\n", $x3, $x4, $x5, $x2' \
    -ex 'frame 0' \
    -ex 'info args' \
    -ex 'info registers x0 x1 x2 x3 x4 x5 x6 x7 x8 x19 x20 x21 x22 x23 x24 x25 x26 x27 x28 x29 x30' \
    -ex 'bt 16' \
    -ex 'frame 1' \
    -ex 'printf "[GsgAssertBacktrace] phase=copy_image_caller_frame1\\n"' \
    -ex 'info args' \
    -ex 'info locals' \
    -ex 'frame 2' \
    -ex 'printf "[GsgAssertBacktrace] phase=copy_image_caller_frame2\\n"' \
    -ex 'info args' \
    -ex 'info locals' \
    -ex 'frame 0' \
    -ex 'finish' \
    -ex 'printf "[GsgAssertBacktrace] phase=copy_image_return result=%d\\n", $x0' \
    -ex 'bt 20' \
    -ex 'quit' \
    --args "$app_root/HwaSim_IR" --channel precise --network-config "$network_config"
gdb_status=$?
set -e
echo "[GsgAssertBacktrace] phase=end gdbStatus=$gdb_status"
