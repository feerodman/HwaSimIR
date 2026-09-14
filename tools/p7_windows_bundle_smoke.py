"""Exercise packaged Qt programs with bounded, local-only startup/exit cases."""
import argparse,json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
env=os.environ.copy();env['QT_FORCE_STDERR_LOGGING']='1'
startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
parser=argparse.ArgumentParser();parser.add_argument('versions',nargs='*',default=['P7_HEAD_rollback','P7_C4']);args=parser.parse_args()
rows=[]
for version in args.versions:
    for app,exe in [('receiver','HwaSim_IR_VideoDisplay.exe'),('sender','DataDrivenTestQT.exe')]:
        folder=root/'releases/windows'/version/app
        args=['--receive-transport=tcp','--acceptance-exit-ms=2500'] if app=='receiver' else [
            '--control-transport=udp','--init-only','--network-config='+str(root/'logs/p7/windows_smoke_loopback.ini')]
        prefix=root/'logs/p7'/f'{version}_{app}_smoke_python'
        with prefix.with_suffix('.out.log').open('wb') as out,prefix.with_suffix('.err.log').open('wb') as err:
            run=subprocess.run([str(folder/exe),*args],cwd=folder,env=env,stdout=out,stderr=err,
                startupinfo=startup,timeout=20)
        log=prefix.with_suffix('.err.log').read_text(encoding='utf-8',errors='replace')
        assert run.returncode==0,(version,app,run.returncode)
        assert ('[AcceptanceExit]' if app=='receiver' else '[StimInit]') in log,(version,app,'missing actual entry evidence')
        rows.append(dict(version=version,application=app,exitCode=run.returncode,result='PASS',
            scope='receiver idle TCP; sender existing INIT-only to UDP loopback; no board control traffic'))
(root/'logs/p7/windows_bundle_smoke.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print(json.dumps(rows))
