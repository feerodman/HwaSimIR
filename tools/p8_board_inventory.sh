#!/bin/sh
set -eu
cd /userdata/HwaSimIR
printf '%s\n' '[P8BoardInventory] clocks_read_only_no_adjustment'
uname -a
sha256sum HwaSim_IR run_precise.sh Config/deployment_manifest.sha256 Config/RK3588PerformancePolicy.conf Config/HwaSimIRRuntime.ini
pgrep -a -x HwaSim_IR || true
for device in /sys/devices/system/cpu/cpufreq/policy*; do
    echo "DEVICE=$device UNIT=kHz"
    for field in scaling_available_frequencies scaling_available_governors scaling_governor scaling_min_freq scaling_max_freq scaling_cur_freq cpuinfo_min_freq cpuinfo_max_freq; do
        if [ -r "$device/$field" ]; then printf '%s=' "$field"; cat "$device/$field"; fi
    done
done
for device in /sys/class/devfreq/fb000000.gpu /sys/class/devfreq/dmc; do
    echo "DEVICE=$device UNIT=Hz"
    for field in available_frequencies available_governors governor min_freq max_freq cur_freq; do
        if [ -r "$device/$field" ]; then printf '%s=' "$field"; cat "$device/$field"; fi
    done
done
for zone in /sys/class/thermal/thermal_zone*; do
    printf '%s ' "$zone"; cat "$zone/type" "$zone/temp"
    for trip in "$zone"/trip_point_*_temp; do [ ! -r "$trip" ] || { printf '%s=' "$trip"; cat "$trip"; }; done
done
df -h /userdata/HwaSimIR
free -m
grep -n 'UdpEnable\|EnableUdp\|ProtocolTransport\|TcpSendVideo\|DdsProtocol\|tcpv4' Config/NetworkConfig_precise.ini Config/HwaSimIRRuntime.ini Config/DDS/ZRDDS_PROTOCOL_QOS.xml || true
