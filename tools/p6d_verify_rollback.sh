#!/bin/sh
# Explicit full hashes after tests. Never called by the production launcher.
set -eu
root=/userdata/HwaSimIR
for suffix in '' "$@"; do
    config=$root/Config$suffix
    version=$config/deployment_version.env
    test -f "$version"
    (cd "$config" && sha256sum -c deployment_manifest.sha256 >/dev/null)
    for key_file in 'ElfSha256:HwaSim_IR' 'LauncherSha256:run_precise.sh' 'PerformanceToolSha256:rk3588_hwasimir_performance_mode.sh'; do
        key=${key_file%%:*};file=${key_file#*:}
        expected=$(awk -F= -v key="$key" '$1==key {print $2;exit}' "$version")
        actual=$(sha256sum "$root/$file$suffix" | awk '{print $1}')
        test -n "$expected" && test "$expected" = "$actual"
        echo "[RollbackComponent] suffix=$suffix file=$file sha256=$actual result=PASS"
    done
    echo "[RollbackSnapshot] config=$config full_hash_check=PASS mode=read_only_no_switch"
done
live=$(stat -c %i "$root/Config/Weather/world_cloud_game.json")
old=$(stat -c %i "$root/Config.before_20260913-150853/Weather/world_cloud_game.json")
test "$live" != "$old"
echo "[RollbackInode] file=Weather/world_cloud_game.json live=$live P6C=$old independent=PASS"
echo '[RollbackVerification] result=PASS snapshots_not_modified=1 old_release_runtime_not_exercised=1'
