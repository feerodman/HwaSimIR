#!/bin/sh

set -eu

APP_ROOT="${HWASIMIR_ROOT:-/userdata/HwaSimIR}"
STATE_DIR="${HWASIMIR_PERF_STATE_DIR:-/run/hwasimir-performance-policy}"
GPU_DEVFREQ="${HWASIMIR_GPU_DEVFREQ:-/sys/class/devfreq/fb000000.gpu}"
DMC_DEVFREQ="${HWASIMIR_DMC_DEVFREQ:-/sys/class/devfreq/dmc}"
POLICY_FILE="${HWASIMIR_PERF_POLICY_FILE:-$APP_ROOT/Config/RK3588PerformancePolicy.conf}"

require_root()
{
    if [ "$(id -u)" -ne 0 ]; then
        echo "[BoardPerformancePolicy][ERROR] reason=root_required policySource=$POLICY_FILE" >&2
        exit 1
    fi
}

read_setting()
{
    key=$1
    fallback=$2
    if [ ! -f "$POLICY_FILE" ]; then
        printf '%s\n' "$fallback"
        return
    fi
    value=$(awk -F= -v wanted="$key" '
        /^[[:space:]]*[#;]/ { next }
        {
            name=$1
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", name)
            if (name == wanted) {
                sub(/^[^=]*=/, "")
                gsub(/^[[:space:]]+|[[:space:]]+$/, "")
                print
                exit
            }
        }' "$POLICY_FILE")
    [ -n "$value" ] && printf '%s\n' "$value" || printf '%s\n' "$fallback"
}

read_value()
{
    path=$1
    [ -r "$path" ] && cat "$path" || printf '%s' unavailable
}

available_contains()
{
    path=$1
    wanted=$2
    [ "$wanted" -gt 0 ] 2>/dev/null || return 1
    [ ! -r "$path" ] && return 0
    grep -qw "$wanted" "$path"
}

write_verified()
{
    path=$1
    value=$2
    label=$3
    if [ ! -w "$path" ]; then
        echo "[BoardPerformancePolicy][ERROR] reason=sysfs_not_writable field=$label path=$path policySource=$POLICY_FILE" >&2
        return 1
    fi
    printf '%s\n' "$value" > "$path"
    actual=$(cat "$path")
    # cpufreq policy updates may settle after the sysfs write returns. Preserve
    # exact verification, but allow a bounded 200 ms for the readback to settle.
    attempts=0
    while [ "$actual" != "$value" ] && [ "$attempts" -lt 10 ]; do
        sleep 0.02
        actual=$(cat "$path")
        attempts=$((attempts + 1))
    done
    if [ "$actual" != "$value" ]; then
        echo "[BoardPerformancePolicy][ERROR] reason=readback_mismatch field=$label requested=$value actual=$actual path=$path policySource=$POLICY_FILE" >&2
        return 1
    fi
    if [ "$attempts" -gt 0 ]; then
        echo "[BoardPerformancePolicy] result=PASS field=$label requested=$value actual=$actual readbackRetries=$attempts"
    fi
}

save_original_state()
{
    mkdir -p "$STATE_DIR"
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -f "$policy/scaling_governor" ] || continue
        name=$(basename "$policy")
        [ -f "$STATE_DIR/$name.governor" ] || cat "$policy/scaling_governor" > "$STATE_DIR/$name.governor"
        [ -f "$STATE_DIR/$name.min" ] || cat "$policy/scaling_min_freq" > "$STATE_DIR/$name.min"
        [ -f "$STATE_DIR/$name.max" ] || cat "$policy/scaling_max_freq" > "$STATE_DIR/$name.max"
    done
    if [ -f "$GPU_DEVFREQ/governor" ]; then
        [ -f "$STATE_DIR/gpu.governor" ] || cat "$GPU_DEVFREQ/governor" > "$STATE_DIR/gpu.governor"
        [ -f "$STATE_DIR/gpu.min" ] || cat "$GPU_DEVFREQ/min_freq" > "$STATE_DIR/gpu.min"
        [ -f "$STATE_DIR/gpu.max" ] || cat "$GPU_DEVFREQ/max_freq" > "$STATE_DIR/gpu.max"
    fi
    if [ -f "$DMC_DEVFREQ/governor" ]; then
        [ -f "$STATE_DIR/dmc.governor" ] || cat "$DMC_DEVFREQ/governor" > "$STATE_DIR/dmc.governor"
        [ -f "$STATE_DIR/dmc.min" ] || cat "$DMC_DEVFREQ/min_freq" > "$STATE_DIR/dmc.min"
        [ -f "$STATE_DIR/dmc.max" ] || cat "$DMC_DEVFREQ/max_freq" > "$STATE_DIR/dmc.max"
    fi
    printf '%s\n' "$POLICY_FILE" > "$STATE_DIR/policy_source"
}

show_status()
{
    mode=$(read_setting PolicyMode status)
    process_nice=$(read_setting ProcessNice 0)
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -f "$policy/scaling_governor" ] || continue
        echo "[BoardPerformancePolicy] result=PASS mode=$mode processNice=$process_nice device=$(basename "$policy") cpus=$(tr ' ' ',' < "$policy/affected_cpus") governor=$(read_value "$policy/scaling_governor") minKHz=$(read_value "$policy/scaling_min_freq") maxKHz=$(read_value "$policy/scaling_max_freq") currentKHz=$(read_value "$policy/scaling_cur_freq") policySource=$POLICY_FILE"
    done
    if [ -f "$GPU_DEVFREQ/governor" ]; then
        echo "[BoardPerformancePolicy] result=PASS mode=$mode processNice=$process_nice device=gpu governor=$(read_value "$GPU_DEVFREQ/governor") minHz=$(read_value "$GPU_DEVFREQ/min_freq") maxHz=$(read_value "$GPU_DEVFREQ/max_freq") currentHz=$(read_value "$GPU_DEVFREQ/cur_freq") policySource=$POLICY_FILE"
    fi
    if [ -f "$DMC_DEVFREQ/governor" ]; then
        echo "[BoardPerformancePolicy] result=PASS mode=$mode processNice=$process_nice device=dmc governor=$(read_value "$DMC_DEVFREQ/governor") minHz=$(read_value "$DMC_DEVFREQ/min_freq") maxHz=$(read_value "$DMC_DEVFREQ/max_freq") currentHz=$(read_value "$DMC_DEVFREQ/cur_freq") policySource=$POLICY_FILE"
    fi
}

apply_auto_min()
{
    cpu_governor=$(read_setting CpuGovernor ondemand)
    gpu_governor=$(read_setting GpuGovernor simple_ondemand)
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -f "$policy/scaling_governor" ] || continue
        name=$(basename "$policy")
        policy_index=${name#policy}
        min_key="CpuPolicy${policy_index}MinKHz"
        max_key="CpuPolicy${policy_index}MaxKHz"
        min_khz=$(read_setting "$min_key" 0)
        max_khz=$(read_setting "$max_key" 0)
        if [ "$min_khz" -gt 0 ]; then
            if ! available_contains "$policy/scaling_available_frequencies" "$min_khz"; then
                echo "[BoardPerformancePolicy][ERROR] reason=frequency_not_available field=$min_key requestedKHz=$min_khz available=$(tr ' ' ',' < "$policy/scaling_available_frequencies") policySource=$POLICY_FILE" >&2
                return 1
            fi
            write_verified "$policy/scaling_min_freq" "$min_khz" "$min_key"
        fi
        if [ "$max_khz" -gt 0 ]; then
            if ! available_contains "$policy/scaling_available_frequencies" "$max_khz"; then
                echo "[BoardPerformancePolicy][ERROR] reason=frequency_not_available field=$max_key requestedKHz=$max_khz available=$(tr ' ' ',' < "$policy/scaling_available_frequencies") policySource=$POLICY_FILE" >&2
                return 1
            fi
            write_verified "$policy/scaling_max_freq" "$max_khz" "$max_key"
        fi
        write_verified "$policy/scaling_governor" "$cpu_governor" CpuGovernor
    done
    if [ -f "$GPU_DEVFREQ/governor" ]; then
        gpu_min_hz=$(read_setting GpuMinHz 0)
        gpu_max_hz=$(read_setting GpuMaxHz 0)
        if [ "$gpu_min_hz" -gt 0 ]; then
            if ! available_contains "$GPU_DEVFREQ/available_frequencies" "$gpu_min_hz"; then
                echo "[BoardPerformancePolicy][ERROR] reason=frequency_not_available field=GpuMinHz requestedHz=$gpu_min_hz available=$(tr ' ' ',' < "$GPU_DEVFREQ/available_frequencies") policySource=$POLICY_FILE" >&2
                return 1
            fi
            write_verified "$GPU_DEVFREQ/min_freq" "$gpu_min_hz" GpuMinHz
        fi
        if [ "$gpu_max_hz" -gt 0 ]; then
            if ! available_contains "$GPU_DEVFREQ/available_frequencies" "$gpu_max_hz"; then
                echo "[BoardPerformancePolicy][ERROR] reason=frequency_not_available field=GpuMaxHz requestedHz=$gpu_max_hz available=$(tr ' ' ',' < "$GPU_DEVFREQ/available_frequencies") policySource=$POLICY_FILE" >&2
                return 1
            fi
            write_verified "$GPU_DEVFREQ/max_freq" "$gpu_max_hz" GpuMaxHz
        fi
        write_verified "$GPU_DEVFREQ/governor" "$gpu_governor" GpuGovernor
    fi
    if [ -f "$DMC_DEVFREQ/governor" ]; then
        dmc_governor=$(read_setting DmcGovernor dmc_ondemand)
        dmc_min_hz=$(read_setting DmcMinHz 0)
        dmc_max_hz=$(read_setting DmcMaxHz 0)
        if [ "$dmc_min_hz" -gt 0 ]; then
            if ! available_contains "$DMC_DEVFREQ/available_frequencies" "$dmc_min_hz"; then
                echo "[BoardPerformancePolicy][ERROR] reason=frequency_not_available field=DmcMinHz requestedHz=$dmc_min_hz available=$(tr ' ' ',' < "$DMC_DEVFREQ/available_frequencies") policySource=$POLICY_FILE" >&2
                return 1
            fi
            write_verified "$DMC_DEVFREQ/min_freq" "$dmc_min_hz" DmcMinHz
        fi
        if [ "$dmc_max_hz" -gt 0 ]; then
            if ! available_contains "$DMC_DEVFREQ/available_frequencies" "$dmc_max_hz"; then
                echo "[BoardPerformancePolicy][ERROR] reason=frequency_not_available field=DmcMaxHz requestedHz=$dmc_max_hz available=$(tr ' ' ',' < "$DMC_DEVFREQ/available_frequencies") policySource=$POLICY_FILE" >&2
                return 1
            fi
            write_verified "$DMC_DEVFREQ/max_freq" "$dmc_max_hz" DmcMaxHz
        fi
        write_verified "$DMC_DEVFREQ/governor" "$dmc_governor" DmcGovernor
    fi
}

apply_performance()
{
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -f "$policy/scaling_governor" ] || continue
        write_verified "$policy/scaling_governor" performance CpuGovernor
    done
    if [ -f "$GPU_DEVFREQ/governor" ]; then
        write_verified "$GPU_DEVFREQ/governor" performance GpuGovernor
    fi
    if [ -f "$DMC_DEVFREQ/governor" ]; then
        write_verified "$DMC_DEVFREQ/governor" performance DmcGovernor
    fi
}

apply_mode()
{
    require_root
    if [ ! -f "$POLICY_FILE" ]; then
        echo "[BoardPerformancePolicy][ERROR] reason=policy_file_missing policySource=$POLICY_FILE" >&2
        exit 3
    fi
    save_original_state
    mode=$(read_setting PolicyMode auto_min)
    case "$mode" in
        auto_min) apply_auto_min ;;
        performance) apply_performance ;;
        *)
            echo "[BoardPerformancePolicy][ERROR] reason=unsupported_mode mode=$mode policySource=$POLICY_FILE" >&2
            exit 4
            ;;
    esac
    show_status
}

restore_mode()
{
    require_root
    if [ ! -d "$STATE_DIR" ]; then
        echo "[BoardPerformancePolicy][WARN] reason=no_saved_state action=restore_skipped stateDir=$STATE_DIR"
        show_status
        return
    fi
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -f "$policy/scaling_governor" ] || continue
        name=$(basename "$policy")
        [ -f "$STATE_DIR/$name.min" ] && cat "$STATE_DIR/$name.min" > "$policy/scaling_min_freq"
        [ -f "$STATE_DIR/$name.max" ] && cat "$STATE_DIR/$name.max" > "$policy/scaling_max_freq"
        [ -f "$STATE_DIR/$name.governor" ] && cat "$STATE_DIR/$name.governor" > "$policy/scaling_governor"
    done
    if [ -f "$GPU_DEVFREQ/governor" ]; then
        [ -f "$STATE_DIR/gpu.min" ] && cat "$STATE_DIR/gpu.min" > "$GPU_DEVFREQ/min_freq"
        [ -f "$STATE_DIR/gpu.max" ] && cat "$STATE_DIR/gpu.max" > "$GPU_DEVFREQ/max_freq"
        [ -f "$STATE_DIR/gpu.governor" ] && cat "$STATE_DIR/gpu.governor" > "$GPU_DEVFREQ/governor"
    fi
    if [ -f "$DMC_DEVFREQ/governor" ]; then
        [ -f "$STATE_DIR/dmc.min" ] && cat "$STATE_DIR/dmc.min" > "$DMC_DEVFREQ/min_freq"
        [ -f "$STATE_DIR/dmc.max" ] && cat "$STATE_DIR/dmc.max" > "$DMC_DEVFREQ/max_freq"
        [ -f "$STATE_DIR/dmc.governor" ] && cat "$STATE_DIR/dmc.governor" > "$DMC_DEVFREQ/governor"
    fi
    rm -f "$STATE_DIR"/*.governor "$STATE_DIR"/*.min "$STATE_DIR"/*.max "$STATE_DIR/policy_source"
    rmdir "$STATE_DIR" 2>/dev/null || true
    echo "[BoardPerformancePolicy] result=PASS action=restore policySource=$POLICY_FILE"
    show_status
}

case "${1:-status}" in
    apply|enable)
        apply_mode
        ;;
    restore)
        restore_mode
        ;;
    status)
        show_status
        ;;
    *)
        echo "用法：$0 apply|status|restore" >&2
        exit 2
        ;;
esac
