"""Low-frequency supported-clock/thermal observation; never changes OS time.

The only control is terminating this explicitly authorized renderer test when
the board's existing first thermal trip is reached. Protections stay enabled.
"""
import argparse,json,subprocess,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);p.add_argument('--seconds',type=float,required=True);a=p.parse_args()
command='''for z in /sys/class/thermal/thermal_zone*; do printf 'T %s ' "$(cat "$z/type")"; cat "$z/temp"; done
printf 'LIMIT '; cat /sys/class/thermal/thermal_zone0/trip_point_0_temp
for d in /sys/devices/system/cpu/cpufreq/policy*; do printf 'CPU %s ' "${d##*/}"; cat "$d/scaling_cur_freq"; done
for d in /sys/class/devfreq/fb000000.gpu /sys/class/devfreq/dmc; do printf 'DEV %s ' "${d##*/}"; cat "$d/cur_freq"; done
printf 'UP '; cat /proc/uptime
grep 'MemAvailable:' /proc/meminfo
pgrep -x HwaSim_IR | sed 's/^/PID /'
for p in $(pgrep -x HwaSim_IR); do for t in /proc/$p/task/*/stat; do printf 'THREAD '; cat "$t"; done; done
if command -v ss >/dev/null 2>&1; then ss -tin dst 192.168.1.188 2>/dev/null | sed 's/^/TCP /'; fi
'''
start=time.monotonic();failures=0
with a.output.open('w',encoding='utf-8') as out:
    while time.monotonic()-start<a.seconds:
        r=subprocess.run(['ssh','-T','-o','ConnectTimeout=5','-o','StrictHostKeyChecking=yes','root@192.168.1.116','sh -s'],input=command.encode('ascii'),capture_output=True,timeout=15)
        row=dict(elapsedSec=time.monotonic()-start,exitCode=r.returncode,temperaturesC={},cpuKHz={},devfreqHz={},threads=[],tcpDetails=[]);pids=[];limit=75
        failures=failures+1 if r.returncode else 0
        if r.returncode:row['error']=r.stderr.decode('utf-8',errors='replace')
        for line in r.stdout.decode('utf-8',errors='replace').splitlines():
            fields=line.split()
            if not fields:continue
            if fields[0]=='T':row['temperaturesC'][fields[1]]=int(fields[2])/1000
            elif fields[0]=='LIMIT':limit=int(fields[1])/1000;row['existingTripC']=limit
            elif fields[0]=='CPU':row['cpuKHz'][fields[1]]=int(fields[2])
            elif fields[0]=='DEV':row['devfreqHz'][fields[1]]=int(fields[2])
            elif fields[0]=='UP':row['boardUptimeSec']=float(fields[1])
            elif fields[0]=='MemAvailable:':row['memoryAvailableKiB']=int(fields[1])
            elif fields[0]=='PID':pids.append(int(fields[1]))
            elif fields[0]=='TCP':row['tcpDetails'].append(line[4:])
            elif fields[0]=='THREAD':
                stat=line[len('THREAD '):];end=stat.rindex(')');tail=stat[end+2:].split()
                row['threads'].append(dict(tid=int(stat.split(' ',1)[0]),name=stat[stat.index('(')+1:end],state=tail[0],userTicks=int(tail[11]),systemTicks=int(tail[12]),cpu=int(tail[36])))
        row['rendererPids']=pids
        hot=row['temperaturesC'] and max(row['temperaturesC'].values())>=limit
        if hot:
            row['result']='THERMAL_TRIP_TEST_TERMINATED'
            if pids:subprocess.run(['ssh','-o','StrictHostKeyChecking=yes','root@192.168.1.116','kill -TERM '+' '.join(map(str,pids))],check=True)
        if failures>=3:
            row['result']='MONITOR_UNAVAILABLE_TEST_TERMINATED'
            subprocess.run(['ssh','-o','StrictHostKeyChecking=yes','root@192.168.1.116','pkill -TERM -x HwaSim_IR'],check=False)
        out.write(json.dumps(row)+'\n');out.flush()
        if hot or failures>=3:break
        time.sleep(5)
