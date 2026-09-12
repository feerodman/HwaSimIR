"""Archive the exact legacy profiles and create independently versioned band defaults."""
from pathlib import Path
import json,shutil
root=Path(__file__).resolve().parents[1]
folder=root/'HwaSim_IR/Bin/Config/SensorWave'
archive=folder/'Archive/P5';archive.mkdir(parents=True,exist_ok=True)
keys='Width Height FOVH FOVV FocalLength DetectorPitch LensFnumber ADCBitNumber NoiseEquivalentTemperatureDifference BlackHot SpectralResponseRangeLow SpectralResponseRangeHigh'.split()
for filename,band in [('default_NVG.json','NIR'),('default_MWIR.json','MWIR')]:
    old=archive/filename
    if not old.exists():shutil.copy2(folder/filename,old)
    d=json.loads(old.read_text(encoding='utf-8-sig'))
    legacy=dict(Gamma=1.0,Gain=1.0,OffsetGray=0.0,WhiteHot=True,Mode='Fixed')
    game=dict(legacy,Gamma=1.8)
    active={'SensorType':d['SensorType'],'Systems':{'SensorConfigurationSystem':{k:d['Systems']['SensorConfigurationSystem'][k] for k in keys},'DisplaySystem':{'DisplayBits':d['Systems']['DisplaySystem']['DisplayBits']}},
        'HwaSimIR':{'SchemaVersion':1,'Band':band,'Revision':'P6A-1','LegacyArchive':'Archive/P5/'+filename,'Display':{'DefaultPreset':'Legacy','Presets':{'Legacy':legacy,'Game':game,'Auto':dict(game,Mode='Auto'),'Black':dict(game,WhiteHot=False)}}}}
    (folder/filename).write_text(json.dumps(active,ensure_ascii=False,indent=2)+'\n',encoding='utf8')

ini=root/'HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini'
text=ini.read_text(encoding='utf-8-sig')
for key in ['WhiteHot=1','DisplayGain=1.0','DisplayOffset=0.0','SensorInputDisplayGamma=1.0','EnableAGC=false']:
    text=text.replace('\n'+key+'\n','\n; P6A profile default; uncomment for explicit legacy INI override: '+key+'\n')
ini.write_text(text,encoding='utf8')
print('Archived P5 profiles; NIR/MWIR Legacy display defaults remain equivalent.')
