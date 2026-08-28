#!/bin/sh
set -eu

app_root=${HWASIMIR_ROOT:-/userdata/HwaSimIR}
channel=${HWASIMIR_CHANNEL:-precise}
network_config=${HWASIMIR_NETWORK_CONFIG:-$app_root/Config/NetworkConfig_${channel}.ini}
udp_port=${HWASIMIR_UDP_PORT:-8888}
qos_file=${HwaSimIRDdsVideoQosFile:-Config/DDS/ZRDDS_QOS_PROFILES.xml}

case "$qos_file" in
    /*) qos_resolved=$qos_file ;;
    *) qos_resolved=$app_root/$qos_file ;;
esac

if ! ps -ef | grep '[X]org :0' >/dev/null 2>&1; then
    echo '[RunPreflight][FATAL] component=Xorg display=:0 reason=not_running' >&2
    exit 10
fi
if [ ! -x "$app_root/HwaSim_IR" ]; then
    echo "[RunPreflight][FATAL] component=deployment reason=binary_missing path=$app_root/HwaSim_IR" >&2
    exit 11
fi
if [ ! -f "$network_config" ]; then
    echo "[RunPreflight][FATAL] component=deployment reason=network_config_missing path=$network_config" >&2
    exit 12
fi
if [ ! -f "$qos_resolved" ]; then
    echo "[RunPreflight][FATAL] component=DDS reason=qos_file_not_found path=$qos_resolved" >&2
    exit 13
fi
if ss -H -lunp 2>/dev/null | awk -v port=":$udp_port" '$4 ~ port"$" { found=1 } END { exit !found }'; then
    echo "[RunPreflight][FATAL] component=UDP localPort=$udp_port reason=already_in_use" >&2
    ss -lunp 2>/dev/null | grep ":$udp_port" >&2 || true
    exit 14
fi

export PANDA3D_ROOT=${PANDA3D_ROOT:-/opt/panda3d-aarch64}
export PRC_DIR=${PRC_DIR:-$PANDA3D_ROOT/etc}
export PANDA_PRC_DIR=${PANDA_PRC_DIR:-$PANDA3D_ROOT/etc}
export DISPLAY=${DISPLAY:-:0}
export XAUTHORITY=${XAUTHORITY:-/root/.Xauthority}
export ZRDDS_HOME=${ZRDDS_HOME:-/usr/ZRDDS/ZRDDS-2.4.5}
export LD_LIBRARY_PATH=$ZRDDS_HOME/lib:$PANDA3D_ROOT/lib:/usr/lib/aarch64-linux-gnu/mali:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
unset EGL_PLATFORM
unset LIBGL_ALWAYS_SOFTWARE
unset MESA_LOADER_DRIVER_OVERRIDE

echo "[RunPreflight] result=PASS channel=$channel display=$DISPLAY qos=$qos_resolved networkConfig=$network_config udpPort=$udp_port"
cd "$app_root"
exec ./HwaSim_IR --channel "$channel" --network-config "$network_config" "$@"
