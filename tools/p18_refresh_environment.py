import json,os,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];path=ROOT/'logs/p18/final_environment.json';report=json.loads(path.read_text())
env=os.environ.copy();env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p18')
def ssh(command):return subprocess.check_output(['ssh','-o','UpdateHostKeys=no','-o','ConnectTimeout=10','root@192.168.1.116',command],env=env,text=True,encoding='utf-8').strip()
report.setdefault('beforeAuditCleanupDisk',report['boardDisk']);report['boardDisk']=ssh('df -k /userdata')
report['boardRenderProcesses']=ssh('pgrep -a -x HwaSim_IR || true');report['boardGovernors']=ssh('cat /sys/devices/system/cpu/cpufreq/policy*/scaling_governor')
report['postCleanupWindowsUtc']=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())
report['noRemainingTestProcesses']=not report['boardRenderProcesses'] and not report['remainingLocalTestProcesses']
report['governorsRestoredToObservedBaseline']=all(s=='ondemand' for s in report['boardGovernors'].splitlines())
path.write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
