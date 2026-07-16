#!/usr/bin/env bash
set -euo pipefail

ROOT="${HWA_ROOT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
BIN_DIR="${HWA_BIN_DIR:-$ROOT/HwaSim_IR/Bin}"
APP="${HWA_APP:-$BIN_DIR/HwaSim_IR}"
RUNTIME_INI="${HWA_RUNTIME_INI:-$BIN_DIR/Config/HwaSimIRRuntime.ini}"
SECONDS_PER_CASE="${H5_SECONDS:-30}"
LOG_ROOT="${H5_LOG_ROOT:-$ROOT/logs/rk3588_h5_render_bottleneck_ab-$(date +%Y%m%d-%H%M%S)}"
STIM_CMD="${H5_STIM_CMD:-}"

if [[ ! -f "$RUNTIME_INI" ]]; then
  echo "[H5][ERROR] runtime ini not found: $RUNTIME_INI" >&2
  exit 1
fi
if [[ ! -x "$APP" ]]; then
  echo "[H5][ERROR] HwaSimIR executable not found or not executable: $APP" >&2
  exit 1
fi

mkdir -p "$LOG_ROOT"
BACKUP_INI="$LOG_ROOT/HwaSimIRRuntime.ini.backup"
cp "$RUNTIME_INI" "$BACKUP_INI"

restore_ini() {
  cp "$BACKUP_INI" "$RUNTIME_INI"
}
trap restore_ini EXIT

set_ini() {
  local section="$1"
  local key="$2"
  local value="$3"
  python3 - "$RUNTIME_INI" "$section" "$key" "$value" <<'PY'
import sys
from pathlib import Path

path = Path(sys.argv[1])
section = sys.argv[2]
key = sys.argv[3]
value = sys.argv[4]
lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
out = []
current = None
seen_section = False
written = False
for line in lines:
    stripped = line.strip()
    if stripped.startswith("[") and stripped.endswith("]"):
        if seen_section and not written:
            out.append(f"{key}={value}")
            written = True
        current = stripped[1:-1].strip()
        if current.lower() == section.lower():
            seen_section = True
    if current and current.lower() == section.lower() and stripped.lower().startswith(key.lower() + "="):
        out.append(f"{key}={value}")
        written = True
    else:
        out.append(line)
if not seen_section:
    out.append("")
    out.append(f"[{section}]")
    out.append(f"{key}={value}")
elif not written:
    out.append(f"{key}={value}")
path.write_text("\n".join(out) + "\n", encoding="utf-8")
PY
}

apply_common_headless_profile() {
  set_ini RenderBackend PresentationMode HeadlessOffscreen
  set_ini RenderBackend WindowPreview false
  set_ini RenderBackend HeadlessFastDirectFinal true
  set_ini RenderBackend HeadlessForceSyncVideoFalse true
  set_ini RenderBackend RenderPerfProbe true
  set_ini RenderBackend HeadlessImageProbe false
  set_ini TcpOutput JpegEncodeMode gray
  set_ini TcpOutput JpegQuality 95
  set_ini Annotation OverlayInSensorImage false
  set_ini Annotation JsonPerFrame true
  set_ini Annotation FastJsonMode true
  set_ini Annotation BBoxUpdateHz 10
  set_ini Annotation OcclusionUpdateHz 5
  set_ini Annotation ReuseLastWhenSkipped true
  set_ini Performance TargetUpdateCullInvisible true
}

run_case() {
  local name="$1"
  local quiet="$2"
  local readback="$3"
  local every_n="$4"
  local width="$5"
  local height="$6"
  local bbox_mode="$7"

  restore_ini
  apply_common_headless_profile
  set_ini Performance QuietPerfMode "$quiet"
  set_ini RenderBackend HeadlessReadbackMode "$readback"
  set_ini RenderBackend HeadlessReadbackEveryN "$every_n"
  set_ini RenderBackend HeadlessWidth "$width"
  set_ini RenderBackend HeadlessHeight "$height"
  set_ini Annotation BBoxFastMode "$bbox_mode"

  local case_dir="$LOG_ROOT/$name"
  mkdir -p "$case_dir"
  echo "[H5] case=$name quiet=$quiet readback=$readback everyN=$every_n size=${width}x${height} bbox=$bbox_mode"

  local app_status=0
  local app_pid=""
  local stim_pid=""

  (
    cd "$BIN_DIR"
    timeout "$SECONDS_PER_CASE" "$APP"
  ) >"$case_dir/hwa.out.log" 2>"$case_dir/hwa.err.log" &
  app_pid=$!
  sleep 2

  if [[ -n "$STIM_CMD" ]]; then
    timeout "$SECONDS_PER_CASE" bash -lc "$STIM_CMD" >"$case_dir/stim.out.log" 2>"$case_dir/stim.err.log" &
    stim_pid=$!
  fi

  wait "$app_pid" || app_status=$?
  if [[ -n "${stim_pid}" ]]; then
    kill "$stim_pid" >/dev/null 2>&1 || true
    wait "$stim_pid" >/dev/null 2>&1 || true
  fi
  if [[ "$app_status" != "0" && "$app_status" != "124" ]]; then
    echo "[H5][WARN] case=$name app_exit=$app_status" | tee -a "$case_dir/h5.status.log"
  fi
}

run_case A_current_h4_800_readback_every_frame_quiet_off false EveryFrame 1 800 800 mesh_body
run_case B_quiet_on true EveryFrame 1 800 800 mesh_body
run_case C_quiet_on_readback_disabled_for_probe true DisabledForPerfProbe 1 800 800 mesh_body
run_case D_quiet_on_640_readback_every_frame true EveryFrame 1 640 640 mesh_body
run_case E_quiet_on_400_readback_every_frame true EveryFrame 1 400 400 mesh_body
run_case F_quiet_on_bbox_cached_aabb_8corners true EveryFrame 1 800 800 cached_aabb_8corners

restore_ini
python3 "$ROOT/tools/rk3588_parse_perf_summary.py" "$LOG_ROOT" -o "$LOG_ROOT/summary.csv" || true
echo "[H5] LOG_ROOT=$LOG_ROOT"
echo "[H5] SUMMARY=$LOG_ROOT/summary.csv"
