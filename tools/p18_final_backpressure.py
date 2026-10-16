"""A DDS reader that keeps its history full, followed by final environment receipts."""
import json,os,psutil,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
deadline=time.monotonic()+1200
last=ROOT/'logs/p18/runs/dual_final_mwir_v4/result.json'
while not last.exists():
    if time.monotonic()>deadline:raise SystemExit('final dual matrix has not completed')
    time.sleep(2)
if json.loads(last.read_text()).get('functionalCountResult')!='PASS':raise SystemExit('inspect final dual count failure')
with (ROOT/'logs/p18/held_dds_history_v4.log').open('w',encoding='utf-8') as log:
    subprocess.run([sys.executable,str(ROOT/'tools/p18_single_run.py'),'--name','held_dds_history_v4','--deployment','candidate4','--receiver-build','build-p18-customer-receiver-v4',
        '--role','precise','--width','320','--codec','raw','--pixels','gray8','--init-h264','1','--old-display','--short-seconds','20','--absent','--hold-dds-samples-seconds','60'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
env=os.environ.copy();env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p18')
def ssh(command):return subprocess.check_output(['ssh','-o','UpdateHostKeys=no','-o','ConnectTimeout=10','root@192.168.1.116',command],env=env,text=True,encoding='utf-8').strip()
report={'capturedWindowsUtc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),'boardRenderProcesses':ssh('pgrep -a -x HwaSim_IR || true'),
    'boardGovernors':ssh('cat /sys/devices/system/cpu/cpufreq/policy*/scaling_governor'),'boardDisk':ssh('df -k /userdata'),
    'boardUtc':ssh('date -u'),'candidate4ElfSha256':ssh('sha256sum /userdata/HwaSimIR/p18/candidate4/HwaSim_IR')}
names={'HwaSimIRCustomerTxVideoReceiverDemo.exe','HwaSim_IR_VideoDisplay.exe','DataDrivenTestQT.exe','HwaSim_IR.exe'}
report['remainingLocalTestProcesses']=[p.info for p in psutil.process_iter(['pid','name','exe']) if p.info['name'] in names]
report['noRemainingTestProcesses']=not report['boardRenderProcesses'] and not report['remainingLocalTestProcesses']
report['governorsRestoredToObservedBaseline']=all(s=='ondemand' for s in report['boardGovernors'].splitlines())
(ROOT/'logs/p18/final_environment.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2),flush=True)
