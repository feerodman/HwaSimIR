#!/bin/sh

# P11 remote inventory probe.  This script is deliberately read-only: it is
# streamed to `sh -s` over SSH and does not create or modify remote files.
set -u

role=${1:-unknown}
app_root=${2:-/userdata/HwaSimIR}
source_root=${3:-/home/linaro/userdata/HwaSimIR}
build_root=${4:-/home/linaro/userdata/HwaSimIR/cmake-build-codex-rk3588}

section()
{
    printf '\n===== %s =====\n' "$1"
}

run_optional()
{
    label=$1
    shift
    printf -- '--- %s ---\n' "$label"
    "$@" 2>&1 || printf '[InventoryWarning] label=%s exit=%s\n' "$label" "$?"
}

hash_file()
{
    path=$1
    if [ -f "$path" ]; then
        sha256sum "$path" 2>&1
    else
        printf '[Missing] %s\n' "$path"
    fi
}

elf_identity()
{
    path=$1
    if [ -f "$path" ]; then
        printf '[Elf] path=%s\n' "$path"
        sha256sum "$path" 2>&1
        stat -c 'mode=%a uid=%u gid=%g size=%s mtime=%y inode=%i links=%h' "$path" 2>&1
        file "$path" 2>&1 || true
        readelf -n "$path" 2>/dev/null | awk '/Build ID:/ {print "BuildId=" $3; found=1; exit} END {if(!found) print "BuildId=unavailable"}'
    fi
}

section identity
printf 'InventoryRole=%s\n' "$role"
printf 'TimestampUtc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ 2>/dev/null || date)"
run_optional hostname hostname
run_optional uname uname -a
run_optional os_release cat /etc/os-release
run_optional uptime uptime
run_optional architecture uname -m

section storage
run_optional filesystem df -hT
run_optional inode_usage df -hi
run_optional mounts findmnt -rn -o TARGET,SOURCE,FSTYPE,OPTIONS

section network
run_optional addresses ip -brief address
run_optional routes ip route
run_optional listeners ss -H -lntup

if [ "$role" = vm ]; then
    section vm_toolchain
    run_optional cmake cmake --version
    run_optional cxx c++ --version
    run_optional aarch64_cxx aarch64-linux-gnu-g++ --version
    run_optional ninja ninja --version
    run_optional make make --version
    run_optional python python3 --version
    run_optional qemu_aarch64 qemu-aarch64 --version

    section vm_dependencies
    for path in /opt/panda3d-aarch64 /usr/ZRDDS/ZRDDS-2.4.5 /usr/aarch64-linux-gnu /opt/arm64-sysroot; do
        if [ -e "$path" ]; then
            ls -ld "$path"
        else
            printf '[Missing] %s\n' "$path"
        fi
    done
    run_optional panda_libraries sh -c "find /opt/panda3d-aarch64/lib -maxdepth 1 -type f -o -type l 2>/dev/null | sort | head -80"
    run_optional zrdds_libraries sh -c "find /usr/ZRDDS/ZRDDS-2.4.5/lib -maxdepth 1 -type f -o -type l 2>/dev/null | sort | head -80"

    section vm_source
    printf 'SourceRoot=%s\nBuildRoot=%s\n' "$source_root" "$build_root"
    run_optional source_root_stat stat -c 'mode=%a uid=%u gid=%g mtime=%y inode=%i' "$source_root"
    if [ -d "$source_root/.git" ]; then
        run_optional git_head git -C "$source_root" rev-parse HEAD
        run_optional git_status git -C "$source_root" status --short
        run_optional git_branch git -C "$source_root" branch --show-current
    else
        printf '[Git] sourceIsRepository=0\n'
    fi
    run_optional source_top_level sh -c "find '$source_root' -maxdepth 2 -mindepth 1 -printf '%TY-%Tm-%TdT%TH:%TM:%TS %y %s %p\\n' 2>/dev/null | sort | tail -120"
    for rel in \
        HwaSimIR.cpp HwaSimIR.h CMakeLists.txt \
        IR/IRRadianceModelV2.cpp IR/IRRadianceModelV2.h \
        IR/IRModtranRadianceLut.cpp IR/IRModtranRadianceLut.h \
        IR/P6GraphicsTest.inl; do
        hash_file "$source_root/$rel"
    done

    section vm_build
    run_optional build_root_stat stat -c 'mode=%a uid=%u gid=%g mtime=%y inode=%i' "$build_root"
    if [ -f "$build_root/CMakeCache.txt" ]; then
        grep -E '^(CMAKE_(BUILD_TYPE|C_COMPILER|CXX_COMPILER|TOOLCHAIN_FILE)|PANDA3D_ROOT|ZRDDS_HOME|CMAKE_SYSROOT):' "$build_root/CMakeCache.txt" 2>&1 || true
    fi
    run_optional cmake_generator sh -c "grep -E '^CMAKE_GENERATOR(:|_INSTANCE:|_PLATFORM:)' '$build_root/CMakeCache.txt' 2>/dev/null || true"
    run_optional build_tree_candidates sh -c "find '$build_root' -maxdepth 3 -type f \\( -name HwaSim_IR -o -name '*.aarch64' \\) -printf '%TY-%Tm-%TdT%TH:%TM:%TS %s %p\\n' 2>/dev/null | sort"
    for candidate in "$build_root/HwaSim_IR" "$build_root/HwaSim_IR/HwaSim_IR"; do
        elf_identity "$candidate"
    done
    if [ -f "$build_root/build.ninja" ]; then
        printf 'BuildCommand=cmake --build %s --config Release -j4\n' "$build_root"
    elif [ -f "$build_root/Makefile" ]; then
        printf 'BuildCommand=cmake --build %s --config Release -j4\n' "$build_root"
    fi
    exit 0
fi

if [ "$role" != board ]; then
    printf '[InventoryError] unsupportedRole=%s\n' "$role" >&2
    exit 2
fi

section board_hardware
run_optional cpuinfo sh -c "grep -E '^(processor|model name|Hardware|Revision|Serial)' /proc/cpuinfo | head -80"
run_optional memory free -h
run_optional devices sh -c "ls -l /dev/mali0 /dev/dri/* 2>&1"
run_optional thermal sh -c "for z in /sys/class/thermal/thermal_zone*; do [ -r \"\$z/type\" ] || continue; printf '%s=' \"\$(cat \"\$z/type\")\"; cat \"\$z/temp\"; done"
run_optional cpu_governors sh -c "for p in /sys/devices/system/cpu/cpufreq/policy*; do [ -r \"\$p/scaling_governor\" ] || continue; printf '%s governor=%s cur=%s min=%s max=%s\\n' \"\$(basename \"\$p\")\" \"\$(cat \"\$p/scaling_governor\")\" \"\$(cat \"\$p/scaling_cur_freq\")\" \"\$(cat \"\$p/scaling_min_freq\")\" \"\$(cat \"\$p/scaling_max_freq\")\"; done"
run_optional gpu_devfreq sh -c "for p in /sys/class/devfreq/*gpu* /sys/class/devfreq/fb000000.gpu; do [ -d \"\$p\" ] || continue; printf '%s governor=%s cur=%s min=%s max=%s available=%s\\n' \"\$p\" \"\$(cat \"\$p/governor\" 2>/dev/null)\" \"\$(cat \"\$p/cur_freq\" 2>/dev/null)\" \"\$(cat \"\$p/min_freq\" 2>/dev/null)\" \"\$(cat \"\$p/max_freq\" 2>/dev/null)\" \"\$(cat \"\$p/available_frequencies\" 2>/dev/null)\"; done"
run_optional dmc_devfreq sh -c "for p in /sys/class/devfreq/dmc; do [ -d \"\$p\" ] || continue; printf '%s governor=%s cur=%s min=%s max=%s available=%s\\n' \"\$p\" \"\$(cat \"\$p/governor\" 2>/dev/null)\" \"\$(cat \"\$p/cur_freq\" 2>/dev/null)\" \"\$(cat \"\$p/min_freq\" 2>/dev/null)\" \"\$(cat \"\$p/max_freq\" 2>/dev/null)\" \"\$(cat \"\$p/available_frequencies\" 2>/dev/null)\"; done"

section board_display_gpu
run_optional xorg_process sh -c "ps -eo pid,lstart,args | grep '[X]org'"
run_optional hwasimir_process sh -c "ps -eo pid,lstart,etimes,stat,ni,psr,rss,args | grep '[H]waSim_IR'"
run_optional xorg_environment sh -c "pid=\$(pgrep -o -f '[X]org[[:space:]]+:0([[:space:]]|$)' 2>/dev/null || true); [ -n \"\$pid\" ] || exit 1; tr '\\000' '\\n' < /proc/\$pid/environ | grep -E '^(DISPLAY|XAUTHORITY|LD_LIBRARY_PATH)=' || true; tr '\\000' ' ' < /proc/\$pid/cmdline; echo"
run_optional mali_modules sh -c "lsmod | grep -Ei 'mali|panthor|panfrost|rockchip' || true"
run_optional mali_libraries sh -c "ldconfig -p 2>/dev/null | grep -Ei 'lib(EGL|GLES|mali)' | head -80"

section board_deployment
printf 'AppRoot=%s\n' "$app_root"
run_optional app_root_stat stat -c 'mode=%a uid=%u gid=%g mtime=%y inode=%i' "$app_root"
run_optional app_root_listing sh -c "find '$app_root' -maxdepth 1 -mindepth 1 -printf '%TY-%Tm-%TdT%TH:%TM:%TS %y %s %f\\n' 2>/dev/null | sort"
run_optional transaction_markers sh -c "find '$app_root' -maxdepth 1 \\( -name '.deployment_in_progress' -o -name '.retention_in_progress' -o -name 'Config.new_*' -o -name 'HwaSim_IR.new_*' \\) -printf '%y %p\\n' 2>/dev/null | sort"
elf_identity "$app_root/HwaSim_IR"
hash_file "$app_root/run_precise.sh"
hash_file "$app_root/rk3588_hwasimir_performance_mode.sh"
run_optional binary_dependencies ldd "$app_root/HwaSim_IR"

section board_config_identity
hash_file "$app_root/Config/deployment_manifest.sha256"
hash_file "$app_root/Config/deployment_version.env"
if [ -f "$app_root/Config/deployment_version.env" ]; then
    cat "$app_root/Config/deployment_version.env"
fi
if [ -f "$app_root/Config/deployment_manifest.sha256" ]; then
    printf -- '--- manifest_check ---\n'
    (cd "$app_root/Config" && sha256sum -c deployment_manifest.sha256 >/dev/null 2>&1)
    manifest_status=$?
    manifest_entries=$(wc -l < "$app_root/Config/deployment_manifest.sha256")
    printf 'ManifestCheckExit=%s ManifestEntries=%s\n' "$manifest_status" "$manifest_entries"
fi
for rel in \
    HwaSimIRRuntime.ini NetworkConfig_precise.ini NetworkConfig_search.ini \
    RK3588PerformancePolicy.conf SensorWave/default_SWIR.json \
    SensorWave/default_MWIR.json SensorWave/default_NVG.json \
    Materials/MaterialDatabase.csv Materials/MaterialBandOptics.csv \
    Atmosphere/MODTRAN/processed/band_lut_si.csv \
    Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv \
    TargetLib/Targets.json IRPlume/engine_plume_profiles.json; do
    hash_file "$app_root/Config/$rel"
done

section board_runtime_config
if [ -f "$app_root/Config/HwaSimIRRuntime.ini" ]; then
    grep -Ev '^[[:space:]]*(#|;|$)' "$app_root/Config/HwaSimIRRuntime.ini" 2>&1 || true
fi
if [ -f "$app_root/Config/RK3588PerformancePolicy.conf" ]; then
    cat "$app_root/Config/RK3588PerformancePolicy.conf"
fi
for cfg in "$app_root/Config/NetworkConfig_precise.ini" "$app_root/Config/NetworkConfig_search.ini"; do
    [ -f "$cfg" ] || continue
    printf -- '--- %s ---\n' "$cfg"
    grep -E '^[[:space:]]*(localIp|localPort|remoteIp|remotePort|Input|Output|Enabled|DomainId|Participant)=' "$cfg" 2>&1 || true
done

section board_rollback_inventory
run_optional backup_listing sh -c "find '$app_root' -maxdepth 1 -mindepth 1 \\( -name 'HwaSim_IR.before_*' -o -name 'run_precise.sh.before_*' -o -name 'rk3588_hwasimir_performance_mode.sh.before_*' -o -name 'Config.before_*' \\) -printf '%TY-%Tm-%TdT%TH:%TM:%TS %y %s %f\\n' 2>/dev/null | sort"
for candidate in "$app_root"/HwaSim_IR.before_*; do
    [ -f "$candidate" ] || continue
    elf_identity "$candidate"
done
for candidate in "$app_root"/Config.before_*; do
    [ -d "$candidate" ] || continue
    printf '[ConfigBackup] path=%s\n' "$candidate"
    stat -c 'mode=%a uid=%u gid=%g mtime=%y inode=%i' "$candidate" 2>&1
    hash_file "$candidate/deployment_manifest.sha256"
    hash_file "$candidate/deployment_version.env"
done

section board_recent_logs
run_optional log_inventory sh -c "find '$app_root/logs' -maxdepth 2 -type f -printf '%TY-%Tm-%TdT%TH:%TM:%TS %s %p\\n' 2>/dev/null | sort | tail -120"

# Stop before any transport-specific trailing record separator that a legacy
# Windows PowerShell native pipeline may append after the streamed script.
exit 0
