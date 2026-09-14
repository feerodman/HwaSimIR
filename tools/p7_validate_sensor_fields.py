"""Compare explicit field inputs with real Qt controls, wire construction and DDS display."""
import argparse,csv,json,re
from pathlib import Path

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('case',type=Path);p.add_argument('expected',type=Path);a=p.parse_args()
    expected=json.loads(a.expected.read_text(encoding='utf-8-sig'))
    assert len(expected)==27
    actual={}
    for line in (a.case/'video.err.log').read_text(encoding='utf-8',errors='replace').splitlines():
        match=re.search(r'\[P7SensorReadback\] (\w+) ([\d.eE+-]+)',line)
        if match:actual[match[1]]=float(match[2])
    ui=json.loads((a.case/'sender_ui.png.json').read_text(encoding='utf-8-sig'))
    rows=[]
    for name,value in expected.items():
        assert name in actual and abs(actual[name]-value)<1e-9,(name,value,actual.get(name))
        control=ui['controls'][name]
        assert ui['fields'][name]==value and control['visible'] and control['enabled']
        rows.append(dict(field=name,control=control['class'],input=value,constructed=ui['fields'][name],ddsReceiver=actual[name],result='PASS'))
    with (a.case/'sensor_field_roundtrip.csv').open('w',encoding='utf-8-sig',newline='') as output:
        writer=csv.DictWriter(output,fieldnames=rows[0]);writer.writeheader();writer.writerows(rows)
    print('PASS: 27 explicit inputs -> controls -> construction -> actual DDS receiver display setter')
