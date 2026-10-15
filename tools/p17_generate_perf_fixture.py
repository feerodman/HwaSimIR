"""Independent 120 s weather performance input; never modifies 1.txt.

Same supported 1 m observer/target cell and 500 m LOS as the P14 ground fixture.
The original AIM120 remains the selected model. This is a diagnostic trajectory.
"""
import csv,hashlib,json,math,pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    source=ROOT/'DataDrivenTestQT/p14_ground_truck_weather_30s.txt'
    rows=list(csv.reader(source.read_text().splitlines()));header=rows[0];base=rows[1]
    payload=[','.join(header)]
    for i in range(7200):
        r=base.copy();r[0]=f'{20+i*1000/60:.9f}'
        shift=math.degrees((i/60*2)/(6371000*math.cos(math.radians(40))))
        for col in (3,11,60):r[col]=f'{float(base[col])+shift:.12f}'
        payload.append(','.join(r))
    data=('\n'.join(payload)+'\n').encode()
    name='p17_weather_performance_120s.txt'
    for folder in ('DataDrivenTestQT','build-DataDrivenTestQT-codex-mingw73_64-Release/release'):
        p=ROOT/folder/name
        if p.exists() and p.read_bytes()!=data:raise RuntimeError('Refusing to overwrite different fixture: '+str(p))
        p.write_bytes(data)
    manifest=dict(purpose='independent_weather_performance_not_original',sourceTemplate=str(source.relative_to(ROOT)),sourceTemplateSha256=hashlib.sha256(source.read_bytes()).hexdigest(),rows=7200,requestedFps=60,durationSeconds=120,observerAltitudeM=1,targetAltitudeM=1,rangeM=499.999797778,sharedEastVelocityMps=2,targetType='0x22_AIM120',calibration='NOT_VERIFIED_CALIBRATION',sha256=hashlib.sha256(data).hexdigest())
    (ROOT/'DataDrivenTestQT'/(name+'.json')).write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))
if __name__=='__main__':main()
