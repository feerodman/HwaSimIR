"""Independent pinhole/mesh-vertex reference for the explicit P8 synthetic scene.

Actual MP4 identity/body/PTS validation remains a separate required P7 check.
No assertion here calibrates equipment geometry or semantic keypoints.
"""
import argparse,csv,json,math
from pathlib import Path

def load(path):
    def invalid(value):raise ValueError('Nonfinite JSON number: '+value)
    with path.open(encoding='utf-8-sig') as f:return [json.loads(line,parse_constant=invalid) for line in f if line.strip()]
def project(p):
    x,y,z=p;f=400/math.tan(math.radians(17.5))
    return math.floor(400+f*x/y+.5),math.floor(400-f*z/y+.5)
def position(i,count,time_ms):
    sec=(time_ms%60000)/1000;phase=int(sec/2)%4;x=(i-(count-1)/2)*2.3;z=-.8 if i%2==0 else 1;y=20
    if phase==1:x+=(sec-2*math.floor(sec/2))*6
    if phase==2 and i<2:x,z,y=0,0,23 if i else 19
    return x,y,z,phase
def vertices(i):
    if i!=1:return [(x,0,z) for x in [-1,1] for z in [-1,1]]
    return [(math.sin(math.pi*r/16)*math.cos(2*math.pi*s/32),math.sin(math.pi*r/16)*math.sin(2*math.pi*s/32),math.cos(math.pi*r/16)) for r in range(17) for s in range(33)]
def validate(case,fixture):
    inputs=json.loads(fixture.read_text(encoding='utf-8'))['SyntheticTelemetryTargets'];count=len(inputs);checks=[];errors=[]
    for folder in sorted((case/'recording').iterdir()):
        if not folder.is_dir():continue
        annotations=load(folder/'producer_annotations.jsonl');telemetry=load(folder/'annotations.txt')
        assert len(annotations)==len(telemetry)
        for row,raw in zip(annotations,telemetry):
            seq=int(row['sourceSeq']);time=float(raw['timeMs']);actual={(o['targetType'],o['targetPlatID'],o['targetID']):o for o in row['targets']}
            assert row['width']==row['height']==800
            assert type(row['width']) is int and type(row['height']) is int
            assert row['version']==1 and math.isfinite(float(row['simTimeMs']))
            for obj in row['targets']:
                assert all(type(obj[key]) is int for key in ['targetType','targetPlatID','targetID'])
                assert len(obj['bboxCorners'])==4
                for point in obj['bboxCorners']+obj['keyPoints']:
                    assert type(point['x']) is int and type(point['y']) is int
                    assert 0<=point['x']<800 and 0<=point['y']<800
                corners=obj['bboxCorners']
                assert corners[0]['y']==corners[1]['y'] and corners[1]['x']==corners[2]['x'] and corners[2]['y']==corners[3]['y'] and corners[3]['x']==corners[0]['x']
                assert all(p['visible'] is True for p in obj['keyPoints'])
            # Float timestamp precision is checked against the independent DDS realtime copy.
            if abs(float(row['simTimeMs'])-time)>1.e-5:errors.append((seq,'simTimePrecision'))
            for i,input_row in enumerate(inputs):
                identity=tuple(input_row[:3]);x,y,z,phase=position(i,count,time)
                raw_identity=(raw['targets'][i]['targetType'],raw['targets'][i]['targetPlatID'],raw['targets'][i]['targetID'])
                if raw_identity!=identity:errors.append((seq,'fieldPropagation',i))
                uv=[project((x+a,y+b,z+c)) for a,b,c in vertices(i)]
                left=min(p[0] for p in uv);right=max(p[0] for p in uv);top=min(p[1] for p in uv);bottom=max(p[1] for p in uv)
                visible=right>=0 and bottom>=0 and left<800 and top<800 and bool(input_row[-1]) and identity[2]>=0 and not (phase==3 and i==count-1)
                got=actual.get(identity)
                if bool(got)!=visible:errors.append((seq,'presence',i,visible,bool(got)));continue
                if not visible:continue
                bounds=[max(0,left),max(0,top),min(799,right),min(799,bottom)]
                observed=[got['bboxCorners'][0]['x'],got['bboxCorners'][0]['y'],got['bboxCorners'][2]['x'],got['bboxCorners'][2]['y']]
                error=max(abs(a-b) for a,b in zip(bounds,observed))
                if error>1:errors.append((seq,'bboxProjection',i,bounds,observed))
                points={p['name']:p for p in got['keyPoints']}
                for name,offset in [('head',(0,-1.01,0)),('middle',(.5,-1.01,.5))]:
                    px,py=project(tuple(a+b for a,b in zip((x,y,z),offset)))
                    expected=0<=px<800 and 0<=py<800 and not (phase==2 and i==1)
                    # Other separated generated bodies can also occlude near a viewport boundary;
                    # inspect those cases rather than treating absent V1 points as zero coordinates.
                    if phase in [0,2,3] and (name in points)!=expected:errors.append((seq,'markerVisibility',i,name,expected))
                    if name in points and max(abs(points[name]['x']-px),abs(points[name]['y']-py))>1:errors.append((seq,'markerProjection',i,name))
                checks.append(dict(recording=folder.name,sourceSeq=seq,timeMs=time,objectIndex=i,phase=phase,targetType=identity[0],targetPlatID=identity[1],targetID=identity[2],bboxErrorPx=error,left=bounds[0],top=bounds[1],right=bounds[2],bottom=bounds[3],visibleMarkers=len(points)))
    if checks:
        with (case/'ordinary_coordinate_checks.csv').open('w',newline='',encoding='utf-8-sig') as f:w=csv.DictWriter(f,fieldnames=checks[0]);w.writeheader();w.writerows(checks)
    report=dict(result='PASS' if checks and not errors else 'FAIL',checkedObjectFrames=len(checks),errorCount=len(errors),firstErrors=errors[:25],
        scope=['DDS integer type/IDs','source 800x800','top-left pinhole projection','clipped mesh bbox','synthetic marker projection','ordinary foreground occlusion','invalid state omission'],
        limitations=['V1 omits invisible objects/points, without per-reason invalid status','geometric bbox is not a visible-pixel segmentation','equipment model semantics remain uncalibrated','MP4 byte pairing and decoded-pixel inspection require separate evidence'])
    (case/'ordinary_annotation_validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report));return report
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('case',type=Path);p.add_argument('fixture',type=Path);a=p.parse_args();validate(a.case,a.fixture)
