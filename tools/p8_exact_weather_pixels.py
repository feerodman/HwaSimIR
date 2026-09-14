"""Compare 0/128/192 using identical full-input checksums AND source ordinal."""
import csv,json
from pathlib import Path
import numpy as np
from p8_case_metrics import ordered_ledger
from p8_image_evidence import records,extract

BASE=Path(__file__).resolve().parents[1]/'logs/p8'
def main():
    rows=[]
    for weather in ['rain','snow']:
        maps={}
        for n in [0,128,192]:
            case=BASE/f'P8_visual_{weather}_{n}'
            digests={int(r['sourceSeq']):r['digestFNV1a64'] for r in ordered_ledger(case,'accepted')}
            maps[n]={(digests[int(item[2]['sourceSeq'])],int(item[2]['sourceSeq'])):item for item in records(case)}
        zero_case=BASE/f'P8_visual_{weather}_0'
        alternate=BASE/f'P8_visual_{weather}_0_phasealigned'
        if (alternate/'board.log').exists():
            digests={int(r['sourceSeq']):r['digestFNV1a64'] for r in ordered_ledger(alternate,'accepted')}
            alternative={(digests[int(item[2]['sourceSeq'])],int(item[2]['sourceSeq'])):item for item in records(alternate)}
            if any(k[1]>120 for k in set(alternative)&set(maps[128])&set(maps[192])):
                maps[0]=alternative;zero_case=alternate
        common=set.intersection(*(set(m) for m in maps.values()))
        assert common,weather+': no matching full inputs'
        warmed={key for key in common if key[1]>120}
        key=min(warmed or common,key=lambda key:abs(float(maps[128][key][2]['simTimeMs'])%60000-4000))
        images={n:extract(maps[n][key],BASE/f'delivery/{weather}_exact_{n}.png') for n in maps}
        assert len({maps[n][key][2]['simTimeMs'] for n in maps})==1
        for n in [192,128]:
            delta=np.max(abs(images[n].astype(np.int16)-images[0].astype(np.int16)),axis=2)
            rows.append(dict(weather=weather,particles=n,sourceSeq=key[1],initializationFrame=key[1]==1,canonicalFullInputDigest=key[0],simTimeMs=maps[n][key][2]['simTimeMs'],commonFullInputOrdinals=len(common),pixelsOver2=int((delta>2).sum()),pixelsOver4=int((delta>4).sum()),pixelsOver8=int((delta>8).sum()),maximumDifference=int(delta.max()),scope='Actual DDS decoded RGB8; same full input checksum, same sourceSeq, frozen mapping, same normal cloud world; codec effects included; an initialization reference is explicitly marked and does not validate steady performance'))
        old,new=rows[-2:]
        for row in (old,new):row['zeroReferenceCase']=zero_case.name
        new['result']='PASS' if 0<new['pixelsOver4']<old['pixelsOver4'] else 'FAIL';new['ratioOver4To192']=new['pixelsOver4']/old['pixelsOver4'] if old['pixelsOver4'] else None
    with (BASE/'delivery/exact_weather_pixel_coverage.csv').open('w',newline='',encoding='utf-8-sig') as f:
        keys=list(dict.fromkeys(k for r in rows for k in r));w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)
    (BASE/'exact_weather_pixel_coverage.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print(json.dumps(rows))
if __name__=='__main__':main()
