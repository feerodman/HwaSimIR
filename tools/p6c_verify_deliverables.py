"""Verify written artifacts and the final ordinary deployment acceptance evidence."""
from pathlib import Path
import hashlib,json,subprocess,zipfile,argparse
import numpy as np
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6c'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
    return h.hexdigest()

def main():
    args=argparse.ArgumentParser();args.add_argument('--final',action='store_true');options=args.parse_args()
    result={'synthetic':{},'ordinary_release':{}}
    for band in ('nir','mwir'):
        folder=OUT/f'synthetic_{band}_800_final';m=json.loads((folder/'manifest.json').read_text())
        for a in m['artifacts']:
            p=folder/a['path'];assert p.stat().st_size==a['bytes'] and sha(p)==a['sha256'],p
        for file,digest in m['implementation'].items():assert sha(ROOT/'experiments/ordinary_sensor_lab'/file)==digest,file
        for row in m['sequence']:
            assert row['fixed_prnu_sha256']==m['identity']['fixed_prnu_sha256']
            assert row['fixed_dark_rate_sha256']==m['identity']['fixed_dark_rate_sha256']
        for i in range(40):
            frame=folder/f'frame_{i:04d}'
            raw=np.load(frame/'raw_dn.npy',allow_pickle=False)
            assert raw.dtype==np.uint16 and raw.shape==(800,800) and raw.max()<16384
            assert np.array_equal(raw,np.asarray(Image.open(frame/'raw_dn_uint16.png')))
            assert np.array_equal(np.floor(raw.astype(float)/16383*255+.5).astype(np.uint8),np.asarray(Image.open(frame/'preview_u8.png')))
        result['synthetic'][band]={'pass':True,'artifacts':len(m['artifacts']),'frames':40,'implementation_hashes_match':True,'raw_dn_and_previews_verified':True}
    e=json.loads((OUT/'evidence.json').read_text())
    for band in (1,2):
        names=[f'release_b{band}_{rate}_pause_init' for rate in (20,30,60)]+[f'release_b{band}_60hz_{sec}s' for sec in (60,600)]
        for name in names:
            a=e['cases'].get(name)
            if not options.final and (not a or not a['conservation_pass']):continue
            assert a and a['conservation_pass'],name
            assert a['request']['productionDefaults'] and a['request']['normal'],name
            assert a['runtime']['envOverrideCount']=='0',name
            assert a['visible_max']==2,name
            assert a['receiver_decode_errors']==0,name
            if 'pause' in name:
                assert len(a['counts']['rounds'])==2,name
                log=(OUT/name/'stim.err.log').read_text(encoding='utf8')
                assert '[StimPause] state=paused' in log and 'catchUpBurst=0' in log,name
            else:
                assert len(a['counts']['source'])==1,name
                assert int(a['counts']['source'][0]['elapsedMs'])>=1000*(a['request']['seconds']-1),name
                assert a['timing']['receiver_gui_fps']['zero_windows']==0,name
            fields=a['effective_display']
            assert fields and all(r.get('source')=='band_profile' and r.get('legacyConflict')=='0' for r in fields),name
            assert all(r['preset']==('Auto' if band==1 else 'Game') for r in fields),name
            assert Image.open(OUT/name/'received.png').size==(800,800),name
            result['ordinary_release'][name]={'pass':True,'source_total':sum(int(r['acceptedRealtime']) for r in a['counts']['rounds']),'preset':'Auto' if band==1 else 'Game'}
    baseline=json.loads((OUT/'baseline/source.json').read_text())
    base={r['path']:r['sha256'] for r in baseline['files']}
    preserve=['docs/HwaSimIR_P6C_Display_Closeout_SensorImageLab_Codex.md','experiments/ordinary_sensor_lab/test_reference.py']
    for file in preserve:assert sha(ROOT/file)==base[file],file
    result['preserved']={p:sha(ROOT/p) for p in preserve}
    # New and modified source files only, including user attachment unchanged.
    changed=subprocess.check_output(['git','diff','--name-only','-z'],cwd=ROOT).decode().split('\0')
    added=subprocess.check_output(['git','ls-files','--others','--exclude-standard','-z'],cwd=ROOT).decode().split('\0')
    paths=sorted(set(p for p in changed+added if p))
    result['head']=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    result['files']=[{'path':p,'sha256':sha(ROOT/p)} for p in paths if (ROOT/p).is_file()]
    if options.final:
        # Native delivery images must remain byte-identical to actual receiver dumps.
        delivery=json.loads((OUT/'delivery/manifest.json').read_text())
        for row in delivery:
            p=ROOT/row['path'];assert sha(p)==row['sha256'],p
            if row['meaning'].startswith('unannotated actual'):
                assert sha(ROOT/row['sources'][0])==row['sha256'],p
        result['display_delivery_files_verified']=len(delivery)
        subprocess.run(['git','diff','--check'],cwd=ROOT,check=True)
        (OUT/'final_tracked.patch').write_bytes(subprocess.check_output(['git','diff','--binary'],cwd=ROOT))
        with zipfile.ZipFile(OUT/'p6c_changed_source.zip','w',zipfile.ZIP_DEFLATED) as z:
            for p in paths:
                if (ROOT/p).is_file():z.write(ROOT/p,p)
        result['source_zip_sha256']=sha(OUT/'p6c_changed_source.zip')
    result['complete_release_verified']=options.final
    (OUT/('final_integrity.json' if options.final else 'artifact_integrity_partial.json')).write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print('Synthetic manifests and raw codes verified;',len(result['ordinary_release']),'ordinary releases verified; final=',options.final)
if __name__=='__main__':main()
