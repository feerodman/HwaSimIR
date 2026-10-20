#!/usr/bin/env python3
"""RK3588 上成对启动隔离 P20 候选，并在两路结束后恢复共享系统策略。

用法：python3 p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_候选名
按 Ctrl+C 停止本脚本启动的两个实例；不停止其它进程，不切换 current。
"""
import argparse,datetime,fcntl,glob,json,os,pathlib,signal,subprocess,time
ap=argparse.ArgumentParser();ap.add_argument('candidate');a=ap.parse_args()
root=pathlib.Path(a.candidate).resolve()
if root.parent!=pathlib.Path('/userdata/HwaSimIR/candidates') or not root.name.startswith('P20_'):
    raise SystemExit('需要隔离 P20 candidates 目录')
lock=open('/tmp/hwasimir-p20-pair.lock','w');fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
if subprocess.run(['pgrep','-x','HwaSim_IR'],stdout=subprocess.DEVNULL).returncode==0:
    raise SystemExit('已有 renderer，请先由其所有者停止，避免同 Topic 多生产者')
out=pathlib.Path('/tmp')/('hwasimir-p20-pair-'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
out.mkdir();saved={}
for pattern in ['/sys/devices/system/cpu/cpufreq/policy*/scaling_governor','/sys/devices/system/cpu/cpufreq/policy*/scaling_min_freq','/sys/devices/system/cpu/cpufreq/policy*/scaling_max_freq','/sys/class/devfreq/*/governor','/sys/class/devfreq/*/min_freq','/sys/class/devfreq/*/max_freq']:
    for name in glob.glob(pattern):saved[name]=pathlib.Path(name).read_text().strip()
(out/'policy_before.json').write_text(json.dumps(saved,indent=2))
policy=(root/'precise/Config/RK3588PerformancePolicy.conf').read_text().replace('RestoreOnExit=true','RestoreOnExit=false')
(out/'policy.conf').write_text(policy)
processes=[];files=[];result={'candidate':str(root),'currentReleaseChanged':False,'processes':[]}
def interrupted(signum,frame):raise KeyboardInterrupt()
signal.signal(signal.SIGTERM,interrupted)
try:
    for role in ['coarse','precise']:
        base=root/role;assert (base/'HwaSim_IR').is_file()
        env=os.environ.copy();env.update(HWASIMIR_ROOT=str(base),HWASIMIR_CHANNEL=role,
            HWASIMIR_RUNTIME_CONFIG=str(base/'Config/HwaSimIRRuntime.ini'),HWASIMIR_NETWORK_CONFIG=str(base/'network.ini'),
            ZRDDS_HOME=str(base/'zrdds'),HwaSimIRCloudAlgorithm='PuffHybrid',
            HWASIMIR_PERF_POLICY_FILE=str(out/'policy.conf'),HWASIMIR_PERF_STATE_DIR=str(out/'state'))
        log=(out/(role+'.log')).open('wb');files.append(log)
        p=subprocess.Popen(['bash','./run_precise.sh','--channel',role,'--network-config',str(base/'network.ini')],cwd=base,env=env,stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
        processes.append(p);result['processes'].append(dict(role=role,pid=p.pid,root=str(base)))
    (out/'processes.json').write_text(json.dumps(result,indent=2))
    print('两路已启动；等待各自 INIT/ACK。日志：',out,flush=True)
    while all(p.poll() is None for p in processes):time.sleep(.5)
except KeyboardInterrupt:print('停止本脚本拥有的两路并恢复策略',flush=True)
finally:
    for p in processes:
        if p.poll() is None:os.killpg(p.pid,signal.SIGTERM)
    for p in processes:
        try:p.wait(timeout=8)
        except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);p.wait()
    for f in files:f.close()
    errors=[]
    for name,value in saved.items():
        try:pathlib.Path(name).write_text(value+'\n')
        except OSError as e:errors.append(str(e))
    actual={name:pathlib.Path(name).read_text().strip() for name in saved}
    result.update(systemPolicyRestored=not errors and actual==saved,restoreErrors=errors,actualPolicy=actual,ownedProcessExitCodes=[p.returncode for p in processes])
    (out/'result.json').write_text(json.dumps(result,indent=2))
    print('两路已退出；系统策略恢复：',result['systemPolicyRestored'],'；回执：',out/'result.json',flush=True)
