#!/bin/sh
# Explicit maintenance only; never called by normal startup.
set -eu
cd /userdata/HwaSimIR
! pgrep -x HwaSim_IR >/dev/null
for suffix in '' '.before_20260914-091924' '.before_20260914-111641'; do
    config="Config$suffix"
    version="$config/deployment_version.env"
    test -f "$version"
    (cd "$config" && sha256sum -c deployment_manifest.sha256 >"/tmp/p8_integrity${suffix:-_active}.log")
    expected=$(awk -F= '$1=="ElfSha256" {print $2}' "$version")
    actual=$(sha256sum "HwaSim_IR$suffix" | awk '{print $1}')
    test "$expected" = "$actual"
    expected_manifest=$(awk -F= '$1=="ConfigManifestSha256" {print $2}' "$version")
    actual_manifest=$(sha256sum "$config/deployment_manifest.sha256" | awk '{print $1}')
    test "$expected_manifest" = "$actual_manifest"
    launcher=$(awk -F= '$1=="LauncherSha256" {print $2}' "$version")
    test "$launcher" = "$(sha256sum "run_precise.sh$suffix" | awk '{print $1}')"
    performance=$(awk -F= '$1=="PerformanceToolSha256" {print $2}' "$version")
    test "$performance" = "$(sha256sum "rk3588_hwasimir_performance_mode.sh$suffix" | awk '{print $1}')"
    printf '[P8Integrity] result=PASS snapshot=%s elf=%s manifest=%s files=%s launcher=%s performanceTool=%s\n' "${suffix:-active}" "$actual" "$actual_manifest" "$(wc -l < "$config/deployment_manifest.sha256")" "$launcher" "$performance"
done
test ! -d /run/hwasimir-performance-policy
echo '[P8Exit] rendererStopped=1 savedPerformanceStateConsumed=1'
./rk3588_hwasimir_performance_mode.sh status
stat -c '[P8SnapshotInode] %n inode=%i hardlinks=%h bytes=%s' Config/HwaSimIRRuntime.ini Config.before_20260914-111641/HwaSimIRRuntime.ini Config/Weather/precipitation.vert Config.before_20260914-111641/Weather/precipitation.vert
df -h /userdata/HwaSimIR
