#!/bin/sh
set -eu

mode=${1:?mode required}
stage=${2:?stage required}
root=${3:-/userdata/HwaSimIR}
p11_suffix=.before_p11-20260918-001831
restore_dir="$root/rollback-p12-exercise-$stage"
saved_config="$root/Config.p12-exercise-$stage"
tested_p11_dir="$root/rollback-p11-tested-$stage"
marker="$root/.p12_rollback_exercise"

sha_file() {
    sha256sum "$1" | awk '{print $1}'
}

print_facts() {
    cd "$root"
    test -x HwaSim_IR
    test -x run_precise.sh
    test -f Config/deployment_manifest.sha256
    (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null)
    echo "ElfSha256=$(sha_file HwaSim_IR)"
    echo "BuildId=$(readelf -n HwaSim_IR | awk '/Build ID:/ {print $3; exit}')"
    echo "LauncherSha256=$(sha_file run_precise.sh)"
    echo "PerformanceToolSha256=$(sha_file rk3588_hwasimir_performance_mode.sh)"
    echo "ConfigManifestSha256=$(sha_file Config/deployment_manifest.sha256)"
    echo "RuntimeConfigSha256=$(sha_file Config/HwaSimIRRuntime.ini)"
    echo "NetworkConfigSha256=$(sha_file Config/NetworkConfig.ini)"
    echo "PreciseNetworkConfigSha256=$(sha_file Config/NetworkConfig_precise.ini)"
    echo "LutSha256=$(sha_file Config/Atmosphere/MODTRAN/processed/band_lut_si.csv)"
    echo "TargetsSha256=$(sha_file Config/TargetLib/Targets.json)"
    echo "TargetLibManifestSubsetSha256=$(grep ' TargetLib/' Config/deployment_manifest.sha256 | sha256sum | awk '{print $1}')"
    echo "FullConfigCheck=PASS"
}

test_no_owner() {
    test ! -e "$marker"
    ! pgrep -x HwaSim_IR >/dev/null
}

switch_p11() {
    cd "$root"
    test_no_owner
    test -x "HwaSim_IR$p11_suffix"
    test -x "run_precise.sh$p11_suffix"
    test -x "rk3588_hwasimir_performance_mode.sh$p11_suffix"
    test -d "Config$p11_suffix"
    test ! -e "$restore_dir"
    test ! -e "$saved_config"
    test ! -e "$tested_p11_dir"
    mkdir "$marker"
    mkdir "$restore_dir"
    cp -p HwaSim_IR run_precise.sh rk3588_hwasimir_performance_mode.sh "$restore_dir/"
    (sha256sum HwaSim_IR run_precise.sh rk3588_hwasimir_performance_mode.sh > "$restore_dir/p12_binary.sha256")
    (cd Config && sha256sum deployment_manifest.sha256 HwaSimIRRuntime.ini NetworkConfig.ini NetworkConfig_precise.ini Atmosphere/MODTRAN/processed/band_lut_si.csv TargetLib/Targets.json > "$restore_dir/p12_config_files.sha256")
    print_facts > "$restore_dir/p12_active_facts.env"

    staged_config="$root/Config.p11-exercise-new-$stage"
    test ! -e "$staged_config"
    cp -al "Config$p11_suffix" "$staged_config"
    (cd "$staged_config" && sha256sum -c deployment_manifest.sha256 >/dev/null)

    mv Config "$saved_config"
    mv "$staged_config" Config
    cp -p "HwaSim_IR$p11_suffix" HwaSim_IR.new
    cp -p "run_precise.sh$p11_suffix" run_precise.sh.new
    cp -p "rk3588_hwasimir_performance_mode.sh$p11_suffix" rk3588_hwasimir_performance_mode.sh.new
    chmod 755 HwaSim_IR.new run_precise.sh.new rk3588_hwasimir_performance_mode.sh.new
    mv HwaSim_IR.new HwaSim_IR
    mv run_precise.sh.new run_precise.sh
    mv rk3588_hwasimir_performance_mode.sh.new rk3588_hwasimir_performance_mode.sh
    test "$(sha_file HwaSim_IR)" = "$(sha_file "HwaSim_IR$p11_suffix")"
    test "$(sha_file run_precise.sh)" = "$(sha_file "run_precise.sh$p11_suffix")"
    print_facts > "$restore_dir/p11_active_facts.env"
    rmdir "$marker"
    echo "[P12RollbackSwitch] result=PASS release=P11 stage=$stage restore=$restore_dir savedConfig=$saved_config"
}

restore_p12() {
    cd "$root"
    test_no_owner
    test -d "$restore_dir"
    test -d "$saved_config"
    test ! -e "$tested_p11_dir"
    mkdir "$marker"
    mkdir "$tested_p11_dir"
    print_facts > "$restore_dir/p11_postrun_facts.env"
    mv Config "$tested_p11_dir/Config"
    mv "$saved_config" Config
    cp -p "$restore_dir/HwaSim_IR" HwaSim_IR.new
    cp -p "$restore_dir/run_precise.sh" run_precise.sh.new
    cp -p "$restore_dir/rk3588_hwasimir_performance_mode.sh" rk3588_hwasimir_performance_mode.sh.new
    chmod 755 HwaSim_IR.new run_precise.sh.new rk3588_hwasimir_performance_mode.sh.new
    mv HwaSim_IR.new HwaSim_IR
    mv run_precise.sh.new run_precise.sh
    mv rk3588_hwasimir_performance_mode.sh.new rk3588_hwasimir_performance_mode.sh
    sha256sum -c "$restore_dir/p12_binary.sha256" >/dev/null
    (cd Config && sha256sum -c "$restore_dir/p12_config_files.sha256" >/dev/null)
    print_facts > "$restore_dir/p12_restored_facts.env"
    rmdir "$marker"
    echo "[P12RollbackRestore] result=PASS release=P12 stage=$stage p11Tested=$tested_p11_dir"
}

recover_p12() {
    cd "$root"
    ! pgrep -x HwaSim_IR >/dev/null || return 1
    if test -d "$saved_config"; then
        failed="$root/Config.failed-p11-exercise-$stage"
        test ! -e "$failed"
        if test -d Config; then mv Config "$failed"; fi
        mv "$saved_config" Config
    fi
    if test -d "$restore_dir"; then
        cp -p "$restore_dir/HwaSim_IR" HwaSim_IR.new
        cp -p "$restore_dir/run_precise.sh" run_precise.sh.new
        cp -p "$restore_dir/rk3588_hwasimir_performance_mode.sh" rk3588_hwasimir_performance_mode.sh.new
        chmod 755 HwaSim_IR.new run_precise.sh.new rk3588_hwasimir_performance_mode.sh.new
        mv HwaSim_IR.new HwaSim_IR
        mv run_precise.sh.new run_precise.sh
        mv rk3588_hwasimir_performance_mode.sh.new rk3588_hwasimir_performance_mode.sh
    fi
    if test -d "$marker"; then rmdir "$marker"; fi
    print_facts
    echo "[P12RollbackRecover] result=PASS stage=$stage"
}

case "$mode" in
    facts) print_facts ;;
    switch-p11) switch_p11 ;;
    restore-p12) restore_p12 ;;
    recover-p12) recover_p12 ;;
    *) echo "unsupported mode: $mode" >&2; exit 64 ;;
esac
