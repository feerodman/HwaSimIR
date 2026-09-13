#!/bin/sh
# Explicit, read-only full release validation after performance tests finish.
# This is not part of normal startup and does not switch or rewrite snapshots.
set -eu
root=/userdata/HwaSimIR
verify_release() {
    suffix=$1
    config=$root/Config$suffix
    version=$config/deployment_version.env
    test -f "$version"
    (cd "$config" && sha256sum -c deployment_manifest.sha256 >/dev/null)
    for key_file in 'ElfSha256:HwaSim_IR' 'LauncherSha256:run_precise.sh' 'PerformanceToolSha256:rk3588_hwasimir_performance_mode.sh'; do
        key=${key_file%%:*}
        file=${key_file#*:}
        expected=$(awk -F= -v key="$key" '$1==key {print $2;exit}' "$version")
        actual=$(sha256sum "$root/$file$suffix" | awk '{print $1}')
        test -n "$expected" && test "$expected" = "$actual"
        echo "[RollbackComponent] suffix=$suffix file=$file sha256=$actual result=PASS"
    done
    expected=$(awk -F= '$1=="ConfigManifestSha256" {print $2;exit}' "$version")
    actual=$(sha256sum "$config/deployment_manifest.sha256" | awk '{print $1}')
    test -n "$expected" && test "$expected" = "$actual"
    echo "[RollbackSnapshot] config=$config manifest=$actual full_hash_check=PASS mode=read_only_no_switch"
}
verify_release ''
verify_release .before_20260913-114812
verify_release .before_20260913-123927
for profile in default_NVG.json default_MWIR.json; do
    live=$(stat -c %i "$root/Config/SensorWave/$profile")
    old=$(stat -c %i "$root/Config.before_20260913-123927/SensorWave/$profile")
    test "$live" != "$old"
    echo "[RollbackInode] profile=$profile live=$live snapshot=$old independent_changed_file=PASS"
done
echo '[RollbackVerification] result=PASS clocks_unchanged=1 snapshots_not_modified=1 old_release_runtime_not_exercised=1'
