"""Compare actual renderer activation descriptors; paths may be Windows or board logs."""
import argparse
import json
from pathlib import Path
import re


def read(path):
    rows={};activations=0;max_return_delta=0
    for line in Path(path).read_text(encoding='utf-8-sig',errors='replace').splitlines():
        if '[CloudWorldDescriptor] ' not in line:continue
        d=json.loads(line.split('[CloudWorldDescriptor] ',1)[1]);key=d['cloudId'];activations+=1
        if key in rows:
            # Revisit is exact within a single executable/configuration.
            if rows[key]!=d:raise AssertionError('cloud descriptor changed on revisit: '+key)
        rows[key]=d
    if not rows:raise AssertionError('no normal cloud descriptors in '+str(path))
    return rows,activations


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('first');p.add_argument('second');p.add_argument('--output',required=True);a=p.parse_args()
    left,lc=read(a.first);right,rc=read(a.second);common=sorted(left.keys()&right.keys());delta=0
    if len(common)<2:raise AssertionError('need at least two shared actual clouds')
    for key in common:
        x,y=left[key],right[key]
        for field in ('cloudId','cell','template','sha256','animation'):
            if x[field]!=y[field]:raise AssertionError((key,field))
        for field in ('position','radius','density','rotation'):
            u,v=x[field],y[field]
            if not isinstance(u,list):u,v=[u],[v]
            error=max(abs(m-n) for m,n in zip(u,v));delta=max(delta,error)
            if error>1.e-6:raise AssertionError((key,field,error))
    report=dict(passed=True,first=a.first,second=a.second,first_unique=len(left),second_unique=len(right),
                first_activations=lc,second_activations=rc,shared_count=len(common),shared_cloud_ids=common,
                maximum_numeric_delta=delta,position_tolerance_m=1e-6,descriptors=[left[k] for k in common])
    Path(a.output).write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print(json.dumps({k:v for k,v in report.items() if k!='descriptors'},indent=2))


if __name__=='__main__':main()
