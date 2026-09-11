#!/bin/sh

set -eu

output=${1:?usage: rk3588_hwasimir_perf_sample.sh OUTPUT_CSV APP_LOG [DURATION_SEC] [INTERVAL_MS]}
app_log=${2:?usage: rk3588_hwasimir_perf_sample.sh OUTPUT_CSV APP_LOG [DURATION_SEC] [INTERVAL_MS]}
duration_sec=${3:-90}
interval_ms=${4:-500}
gpu_root=${HWASIMIR_GPU_DEVFREQ:-/sys/class/devfreq/fb000000.gpu}
dmc_root=${HWASIMIR_DMC_DEVFREQ:-/sys/class/devfreq/dmc}
sample_cpu=${HWASIMIR_SAMPLE_CPU:-0}

case "$duration_sec:$interval_ms" in
    *[!0-9:]*|:*|*:) echo "invalid duration or interval" >&2; exit 2 ;;
esac
[ "$duration_sec" -gt 0 ] && [ "$interval_ms" -ge 200 ] && [ "$interval_ms" -le 5000 ] || {
    echo "duration must be positive and interval must be 200..5000 ms" >&2
    exit 2
}

mkdir -p "$(dirname "$output")"
# Diagnostics must not compete with the render/DDS/MPP path.  Keep this shell
# on one LITTLE core, at the lowest CPU and block-I/O priorities available.
renice 19 -p $$ >/dev/null 2>&1 || true
command -v ionice >/dev/null 2>&1 && ionice -c 3 -p $$ >/dev/null 2>&1 || true
command -v taskset >/dev/null 2>&1 && taskset -pc "$sample_cpu" $$ >/dev/null 2>&1 || true
cpu_snapshot="${TMPDIR:-/tmp}/hwasimir-perf-cpu-$$.stat"
cp /proc/stat "$cpu_snapshot"
trap 'rm -f "$cpu_snapshot"' EXIT INT TERM HUP

read_or_na()
{
    [ -r "$1" ] && tr -d '\r\n' < "$1" || printf '%s' NA
}

runtime_metrics()
{
    [ -r "$app_log" ] || {
        printf '%s' ',,,,,,,,,,,,,,'
        return
    }
    # Read the growing log once per interval and extract every metric in one
    # awk process.  The previous implementation launched tail/grep/awk dozens
    # of times and could not maintain the requested 200--500 ms cadence.
    tail -n 320 "$app_log" 2>/dev/null | awk '
        /^\[Perf\]/ { perf=$0 }
        /^\[RealtimeIngress\]/ { ingress=$0 }
        /^\[MppPerf\]/ { mpp=$0 }
        /^\[DdsVideoTiming\]/ { dds=$0 }
        function find(line, key,   n,a,i,p) {
            n=split(line,a," ")
            for(i=1;i<=n;i++) {
                split(a[i],p,"=")
                if(p[1]==key) return p[2]
            }
            return ""
        }
        END {
            printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s", \
                find(perf,"renderFps"), find(perf,"outputFps"), \
                find(perf,"renderMs"), find(perf,"irUpdateMs"), \
                find(perf,"readbackMs"), find(mpp,"mppEncodeMs"), \
                find(dds,"ddsWriteMs"), find(perf,"inputQueueDepth"), \
                find(perf,"inputQueueDepthMax"), \
                find(ingress,"inputOverwritten"), \
                find(ingress,"inputQueueOverflow"), \
                find(ingress,"sourceSeqGapCount"), \
                find(ingress,"inputBackpressureCount"), \
                find(ingress,"inputBackpressureWaitMs"), \
                find(ingress,"currentQueueDepth")
        }'
}

cpu_stat()
{
    awk '/^cpu / { total=0; for(i=2;i<=NF;i++) total+=$i; print total, $5+$6; exit }' /proc/stat
}

cpu_process_stat()
{
    pid=$(pgrep -n -x HwaSim_IR 2>/dev/null || true)
    if [ -n "$pid" ] && [ -r "/proc/$pid/stat" ]; then
        awk '{print $14+$15}' "/proc/$pid/stat"
    else
        printf '%s' 0
    fi
}

per_core_percentages()
{
    awk '
        FNR==NR && $1 ~ /^cpu[0-9]+$/ {
            total=0; for(i=2;i<=NF;i++) total+=$i
            old_total[$1]=total; old_idle[$1]=$5+$6; next
        }
        FNR!=NR && $1 ~ /^cpu[0-9]+$/ {
            total=0; for(i=2;i<=NF;i++) total+=$i
            idle=$5+$6; dt=total-old_total[$1]; di=idle-old_idle[$1]
            if(seen++) printf ","
            if(dt<=0) printf "0.000"; else printf "%.3f",100*(dt-di)/dt
        }
    ' "$cpu_snapshot" /proc/stat
}

rss_kb()
{
    pid=$(pgrep -n -x HwaSim_IR 2>/dev/null || true)
    if [ -n "$pid" ] && [ -r "/proc/$pid/status" ]; then
        awk '/^VmRSS:/ {print $2; exit}' "/proc/$pid/status"
    else
        printf '%s' 0
    fi
}

thermal_mc()
{
    wanted=$1
    for zone in /sys/class/thermal/thermal_zone*; do
        [ -r "$zone/type" ] || continue
        [ "$(cat "$zone/type")" = "$wanted" ] || continue
        read_or_na "$zone/temp"
        return
    done
    printf '%s' NA
}

gpu_load()
{
    if [ -r "$gpu_root/load" ]; then
        value=$(cat "$gpu_root/load")
        printf '%s\n' "$value" | awk -F'[@% ]' '{print $1}'
    elif [ -r /sys/devices/platform/fb000000.gpu/utilisation ]; then
        read_or_na /sys/devices/platform/fb000000.gpu/utilisation
    else
        printf '%s' NA
    fi
}

policy_names=''
for policy in /sys/devices/system/cpu/cpufreq/policy*; do
    [ -r "$policy/scaling_governor" ] || continue
    name=$(basename "$policy")
    policy_names="$policy_names $name"
done

header='timestamp_iso,elapsed_ms,cpu_total_pct,hwasimir_cpu_pct,hwasimir_rss_kb,soc_temp_mc,gpu_temp_mc,gpu_load_pct'
for cpu_name in $(awk '$1 ~ /^cpu[0-9]+$/ {print $1}' /proc/stat); do
    header="$header,${cpu_name}_pct"
done
for name in $policy_names; do
    header="$header,${name}_governor,${name}_cur_khz,${name}_min_khz,${name}_max_khz"
done
header="$header,gpu_governor,gpu_cur_hz,gpu_min_hz,gpu_max_hz,dmc_governor,dmc_cur_hz,dmc_min_hz,dmc_max_hz,render_fps,output_fps,render_ms,ir_update_ms,readback_ms,mpp_encode_ms,dds_write_ms,input_queue_depth,input_queue_depth_max,input_overwritten,input_queue_overflow,source_seq_gap,input_backpressure_count,input_backpressure_wait_ms,current_queue_depth"
printf '%s\n' "$header" > "$output"

set -- $(cpu_stat)
prev_total=$1
prev_idle=$2
prev_proc=$(cpu_process_stat)
clk_tck=$(getconf CLK_TCK 2>/dev/null || echo 100)
start_epoch_ms=$(date +%s%3N)
end_epoch_ms=$((start_epoch_ms + duration_sec * 1000))

while :; do
    loop_start_ms=$(date +%s%3N)
    now_epoch_ms=$(date +%s%3N)
    [ "$now_epoch_ms" -le "$end_epoch_ms" ] || break
    set -- $(cpu_stat)
    total=$1
    idle=$2
    proc=$(cpu_process_stat)
    delta_total=$((total - prev_total))
    delta_idle=$((idle - prev_idle))
    delta_proc=$((proc - prev_proc))
    cpu_pct=$(awk -v t="$delta_total" -v i="$delta_idle" 'BEGIN { if(t<=0) print "0.000"; else printf "%.3f", 100*(t-i)/t }')
    proc_pct=$(awk -v t="$delta_total" -v p="$delta_proc" 'BEGIN { if(t<=0) print "0.000"; else printf "%.3f", 800*p/t }')
    prev_total=$total
    prev_idle=$idle
    prev_proc=$proc
    core_pct=$(per_core_percentages)
    cp /proc/stat "$cpu_snapshot"

    value="$(date -Iseconds),$((now_epoch_ms-start_epoch_ms)),$cpu_pct,$proc_pct,$(rss_kb),$(thermal_mc soc-thermal),$(thermal_mc gpu-thermal),$(gpu_load),$core_pct"
    for name in $policy_names; do
        root="/sys/devices/system/cpu/cpufreq/$name"
        value="$value,$(read_or_na "$root/scaling_governor"),$(read_or_na "$root/scaling_cur_freq"),$(read_or_na "$root/scaling_min_freq"),$(read_or_na "$root/scaling_max_freq")"
    done
    value="$value,$(read_or_na "$gpu_root/governor"),$(read_or_na "$gpu_root/cur_freq"),$(read_or_na "$gpu_root/min_freq"),$(read_or_na "$gpu_root/max_freq")"
    value="$value,$(read_or_na "$dmc_root/governor"),$(read_or_na "$dmc_root/cur_freq"),$(read_or_na "$dmc_root/min_freq"),$(read_or_na "$dmc_root/max_freq")"
    value="$value,$(runtime_metrics)"
    printf '%s\n' "$value" >> "$output"
    loop_end_ms=$(date +%s%3N)
    remaining_ms=$((interval_ms - (loop_end_ms - loop_start_ms)))
    if [ "$remaining_ms" -gt 0 ]; then
        sleep "$(awk -v ms="$remaining_ms" 'BEGIN { printf "%.3f", ms/1000 }')"
    fi
done

echo "[BoardPerformanceTimeline] result=PASS output=$output durationSec=$duration_sec intervalMs=$interval_ms"
