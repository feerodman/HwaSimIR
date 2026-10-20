"""双路共用一次系统策略快照；最后一个测试进程退出后核实恢复。"""
import base64, json, shlex

SNAPSHOT = r'''
import glob,json,pathlib
values={}
for pattern in ('/sys/devices/system/cpu/cpufreq/policy*/scaling_governor',
 '/sys/devices/system/cpu/cpufreq/policy*/scaling_min_freq',
 '/sys/devices/system/cpu/cpufreq/policy*/scaling_max_freq',
 '/sys/class/devfreq/*/governor','/sys/class/devfreq/*/min_freq','/sys/class/devfreq/*/max_freq'):
 for name in glob.glob(pattern):
  values[name]=pathlib.Path(name).read_text().strip()
print(json.dumps(values))
'''

def remote_python(ssh, source):
    encoded = base64.b64encode(source.encode('utf-8')).decode('ascii')
    return ssh('python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+encoded+"'))"))

class RuntimePolicy:
    def __init__(self, ssh, output, root, mode):
        self.ssh, self.output = ssh, output
        self.before=json.loads(remote_python(ssh,SNAPSHOT))
        (output/'policy_before.json').write_text(json.dumps(self.before,indent=2),encoding='utf-8')
        original=ssh('cat '+shlex.quote(root+'/Config/RK3588PerformancePolicy.conf'))
        lines=original.replace('\r','').splitlines()
        policy='\n'.join('RestoreOnExit=false' if x.startswith('RestoreOnExit=') else
                         ('PolicyMode=performance' if mode=='performance' else x) if x.startswith('PolicyMode=') else x
                         for x in lines)+'\n'
        self.remote='/tmp/hwasimir-p20-'+output.name
        source="from pathlib import Path\np=Path("+repr(self.remote)+")\np.mkdir(exist_ok=False)\n(p/'policy.conf').write_text("+repr(policy)+")"
        remote_python(ssh,source)
        (output/'runtime_policy.conf').write_text(policy,encoding='utf-8')

    def environment(self):
        return ' HWASIMIR_PERF_POLICY_FILE='+self.remote+'/policy.conf HWASIMIR_PERF_STATE_DIR='+self.remote+'/state'

    def restore(self):
        # 只回写本次保存的明确 sysfs 属性；不删除任何部署目录。
        source="import pathlib,json\nvalues="+repr(self.before)+"\nerrors=[]\n"
        source+="for name,value in values.items():\n try:pathlib.Path(name).write_text(value+'\\n')\n except OSError as e:errors.append(str(e))\n"
        source+=SNAPSHOT.replace('print(json.dumps(values))','print(json.dumps(dict(actual=values,errors=errors)))')
        result=json.loads(remote_python(self.ssh,source))
        result['restored']=not result['errors'] and result['actual']==self.before
        (self.output/'policy_restore.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
        return result['restored']
