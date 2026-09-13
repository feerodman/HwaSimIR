"""Read-only RK3588 thermal samples; never changes a clock or governor."""
import argparse,subprocess,time,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--seconds',type=float,required=True);a=p.parse_args()
start=time.monotonic()
with Path(a.output).open('w',encoding='utf8') as out:
    while time.monotonic()-start<a.seconds:
        r=subprocess.run(['ssh','-o','ConnectTimeout=5','-o','StrictHostKeyChecking=yes','root@192.168.1.116',
            'cat /sys/class/thermal/thermal_zone*/type /sys/class/thermal/thermal_zone*/temp /proc/uptime'],capture_output=True,text=True,timeout=10)
        row=dict(elapsedSec=time.monotonic()-start,exitCode=r.returncode)
        if r.returncode==0:
            lines=r.stdout.splitlines();n=(len(lines)-1)//2
            row.update(temperaturesC={lines[i]:float(lines[n+i])/1000 for i in range(n)},boardUptimeSec=float(lines[-1].split()[0]))
        else:row['error']=r.stderr
        out.write(json.dumps(row)+'\n');out.flush();time.sleep(5)
