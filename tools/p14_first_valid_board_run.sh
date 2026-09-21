#!/bin/sh
set -eu

root=${1:-/userdata/HwaSimIR}
fixture=${2:-$root/logs/HwaSimIRP14FirstValidFixture.final4}
band=${3:-2}
tag=${4:-p14_first_valid_fixture_final4_mwir}
scenario=${5:-all}
tail_frames=${6:-0}

case "$band" in
    0|2) ;;
    *) echo "[P14FixtureBoard][FATAL] band must be 0 or 2" >&2; exit 2 ;;
esac

if pgrep -x HwaSim_IR >/dev/null 2>&1; then
    echo "[P14FixtureBoard][FATAL] an HwaSim_IR process is already running" >&2
    exit 3
fi

test -x "$fixture"
test -f "$root/run_precise.sh"
mkdir -p "$root/logs"

board_log="$root/logs/${tag}_board.log"
sender_log="$root/logs/${tag}_sender.log"
: >"$board_log"
: >"$sender_log"

renderer_pid=
cleanup()
{
    if test -n "$renderer_pid" && kill -0 "$renderer_pid" 2>/dev/null; then
        kill -TERM "$renderer_pid" 2>/dev/null || true
        wait "$renderer_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

cd "$root"
if test "$scenario" = boundary_demo; then
    env \
        RenderPresentationMode=HeadlessOffscreen \
        P5MaterialView=0 \
        LinearDiagnosticPath="$root/logs/${tag}_linear" \
        LinearDiagnosticSeqs=1 \
        EnableAGC=true \
        Stage6DiagnosticsEnable=true \
        AGCDebugLog=true \
        HwaSimIRExitOnStop=false \
        HwaSimIRLocalRecordingEnable=false \
        ./run_precise.sh >"$board_log" 2>&1 &
else
    env \
        RenderPresentationMode=HeadlessOffscreen \
        P5MaterialView=0 \
        HwaSimIRExitOnStop=false \
        HwaSimIRLocalRecordingEnable=false \
        ./run_precise.sh >"$board_log" 2>&1 &
fi
renderer_pid=$!
echo "[P14FixtureBoard] rendererPid=$renderer_pid band=$band boardLog=$board_log"
sleep 6

if ! kill -0 "$renderer_pid" 2>/dev/null; then
    echo "[P14FixtureBoard][FATAL] renderer exited before fixture" >&2
    tail -80 "$board_log" >&2 || true
    exit 4
fi

export ZRDDS_HOME=${ZRDDS_HOME:-/usr/ZRDDS/ZRDDS-2.4.5}
export LD_LIBRARY_PATH="$ZRDDS_HOME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$fixture" \
    --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml \
    --band "$band" \
    --scenario "$scenario" \
    --tail-frames "$tail_frames" >"$sender_log" 2>&1
fixture_rc=$?

echo "[P14FixtureBoard] fixtureRc=$fixture_rc senderLog=$sender_log"
tail -12 "$sender_log" || true
exit "$fixture_rc"
