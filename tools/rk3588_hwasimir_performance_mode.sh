#!/bin/sh

set -eu

STATE_DIR="${HWASIMIR_PERF_STATE_DIR:-/run/hwasimir-performance-mode}"
GPU_DEVFREQ="${HWASIMIR_GPU_DEVFREQ:-/sys/class/devfreq/fb000000.gpu}"

require_root()
{
    if [ "$(id -u)" -ne 0 ]; then
        echo "[RK3588PerformanceMode] error=root_required" >&2
        exit 1
    fi
}

show_status()
{
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -f "$policy/scaling_governor" ] || continue
        echo "[RK3588PerformanceMode] device=$policy governor=$(cat "$policy/scaling_governor") currentHz=$(cat "$policy/scaling_cur_freq") maxHz=$(cat "$policy/cpuinfo_max_freq")"
    done
    if [ -f "$GPU_DEVFREQ/governor" ]; then
        echo "[RK3588PerformanceMode] device=$GPU_DEVFREQ governor=$(cat "$GPU_DEVFREQ/governor") currentHz=$(cat "$GPU_DEVFREQ/cur_freq")"
    fi
}

enable_mode()
{
    require_root
    mkdir -p "$STATE_DIR"
    index=0
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -f "$policy/scaling_governor" ] || continue
        state_file="$STATE_DIR/cpu_$index.governor"
        [ -f "$state_file" ] || cat "$policy/scaling_governor" > "$state_file"
        echo performance > "$policy/scaling_governor"
        index=$((index + 1))
    done
    if [ -f "$GPU_DEVFREQ/governor" ]; then
        [ -f "$STATE_DIR/gpu.governor" ] || cat "$GPU_DEVFREQ/governor" > "$STATE_DIR/gpu.governor"
        echo performance > "$GPU_DEVFREQ/governor"
    fi
    show_status
}

restore_mode()
{
    require_root
    index=0
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -f "$policy/scaling_governor" ] || continue
        state_file="$STATE_DIR/cpu_$index.governor"
        if [ -f "$state_file" ]; then
            cat "$state_file" > "$policy/scaling_governor"
        fi
        index=$((index + 1))
    done
    if [ -f "$STATE_DIR/gpu.governor" ] && [ -f "$GPU_DEVFREQ/governor" ]; then
        cat "$STATE_DIR/gpu.governor" > "$GPU_DEVFREQ/governor"
    fi
    rm -f "$STATE_DIR"/cpu_*.governor "$STATE_DIR/gpu.governor"
    rmdir "$STATE_DIR" 2>/dev/null || true
    show_status
}

case "${1:-status}" in
    enable)
        enable_mode
        ;;
    restore)
        restore_mode
        ;;
    status)
        show_status
        ;;
    *)
        echo "用法：$0 enable|status|restore" >&2
        exit 2
        ;;
esac
