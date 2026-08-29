#!/bin/bash
set -u

if [ "$#" -lt 7 ]; then
  echo "usage: $0 MODE PAYLOAD_BYTES FRAMES FPS DOMAIN TOPIC QOS [IFACE]" >&2
  exit 2
fi

mode=$1
payload_bytes=$2
frames=$3
fps=$4
domain=$5
topic=$6
qos=$7
iface=${8:-eth0}
bench=./HwaSimIRVideoTransportBenchmark
case_log="/tmp/hwasimir_f1_bench_${mode}_$$.log"

cleanup() {
  rm -f -- "$case_log"
}
trap cleanup EXIT

export ZRDDS_HOME=/usr/ZRDDS/ZRDDS-2.4.5
export LD_LIBRARY_PATH="$ZRDDS_HOME/lib:${LD_LIBRARY_PATH:-}"

tx0=$(cat "/sys/class/net/$iface/statistics/tx_bytes")
up0=$(awk '{print $1}' /proc/uptime)
"$bench" --role pub --mode "$mode" --payload-bytes "$payload_bytes" \
  --frames "$frames" --fps "$fps" --domain "$domain" --timeout-sec 30 \
  --shutdown-drain-ms 2000 --qos "$qos" --topic "$topic" >"$case_log" 2>&1 &
pid=$!

sleep 0.1
clk=$(getconf CLK_TCK)
proc0=0
proc_last=0
up_last=$up0
rss_peak=0
threads_peak=0
fd_peak=0
if [ -r "/proc/$pid/stat" ]; then
  proc0=$(awk '{print $14+$15}' "/proc/$pid/stat")
  proc_last=$proc0
fi

while kill -0 "$pid" 2>/dev/null; do
  if [ -r "/proc/$pid/stat" ]; then
    proc_last=$(awk '{print $14+$15}' "/proc/$pid/stat")
    up_last=$(awk '{print $1}' /proc/uptime)
    rss=$(awk '/^VmRSS:/ {print $2}' "/proc/$pid/status")
    threads=$(awk '/^Threads:/ {print $2}' "/proc/$pid/status")
    fd_count=$(find "/proc/$pid/fd" -mindepth 1 -maxdepth 1 2>/dev/null | wc -l)
    [ "${rss:-0}" -gt "$rss_peak" ] && rss_peak=${rss:-0}
    [ "${threads:-0}" -gt "$threads_peak" ] && threads_peak=${threads:-0}
    [ "${fd_count:-0}" -gt "$fd_peak" ] && fd_peak=${fd_count:-0}
  fi
  sleep 0.5
done

wait "$pid"
rc=$?
up1=$(awk '{print $1}' /proc/uptime)
tx1=$(cat "/sys/class/net/$iface/statistics/tx_bytes")
elapsed=$(awk -v a="$up0" -v b="$up1" 'BEGIN {printf "%.3f", b-a}')
cpu_pct=$(awk -v p0="$proc0" -v p1="$proc_last" -v hz="$clk" -v e="$elapsed" \
  'BEGIN {if (e>0) printf "%.3f", ((p1-p0)/hz)/e*100; else print "0.000"}')
tx_mib_s=$(awk -v a="$tx0" -v b="$tx1" -v e="$elapsed" \
  'BEGIN {if (e>0) printf "%.3f", (b-a)/1048576/e; else print "0.000"}')

cat "$case_log"
echo "[BenchmarkResource] role=pub mode=$mode cpuAvgPct=$cpu_pct rssPeakKiB=$rss_peak threadsPeak=$threads_peak fdPeak=$fd_peak netTxMiBPerSec=$tx_mib_s elapsedSec=$elapsed exitCode=$rc"
exit "$rc"
