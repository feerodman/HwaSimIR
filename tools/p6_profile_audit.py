"""Enumerate every archived/current profile scalar, distinguishing storage from use."""
from pathlib import Path
import json
root=Path(__file__).resolve().parents[1]
out=root/'logs/p6';out.mkdir(parents=True,exist_ok=True)
prefix='Systems.SensorConfigurationSystem.'
fields={
 'Width':('width','ProcessRealSimSceneInitData -> IRSensorModel','仅缺省','pixel'),
 'Height':('height','ProcessRealSimSceneInitData -> IRSensorModel','仅缺省','pixel'),
 'FOVH':('fovHDeg','缺省 pixelAngle 反推；仍经过接口几何校验/上限','仅缺省','degree'),
 'FOVV':('fovVDeg','LogActiveIRSensorProfile','仅日志','degree'),
 'SpectralResponseRangeLow':('spectralLowUm','固定波段一致性检查；Stage5 debug input.sensorLowUm','正常使用（验证/诊断）','um'),
 'SpectralResponseRangeHigh':('spectralHighUm','固定波段一致性检查；Stage5 debug input.sensorHighUm','正常使用（验证/诊断）','um'),
 'ADCBitNumber':('adcBits','LogActiveIRSensorProfile；不控制实际编码量化','仅日志','bit'),
 'NoiseEquivalentTemperatureDifference':('netdK','LogActiveIRSensorProfile；未接入 NETD 噪声算法','仅日志','legacy nominal K; uncalibrated'),
 'FocalLength':('focalLengthMm','LogActiveIRSensorProfile；不参与有效接口 FOV 计算','仅日志','mm'),
 'DetectorPitch':('detectorPitchMm','LogActiveIRSensorProfile；未用于电子通量','仅日志','mm'),
 'LensFnumber':('lensFNumber','LogActiveIRSensorProfile；未用于电子通量','仅日志','ratio'),
 'BlackHot':('blackHot','LogActiveIRSensorProfile；实际极性使用 HwaSimIR.Display','仅日志','bool')}
def leaves(v,path=''):
    if isinstance(v,dict):
        for k,x in v.items():yield from leaves(x,f'{path}.{k}' if path else k)
    elif isinstance(v,list):
        if not v:yield path,[]
        for i,x in enumerate(v):yield from leaves(x,f'{path}[{i}]')
    else:yield path,v
all_rows=[]
for f in ['default_NVG.json','default_MWIR.json']:
    base=root/'HwaSim_IR/Bin/Config/SensorWave'
    legacy=json.loads((base/'Archive/P5'/f).read_text(encoding='utf-8-sig'))
    active=json.loads((base/f).read_text(encoding='utf-8-sig'))
    active_paths=dict(leaves(active))
    rows=[]
    for path,value in leaves(legacy):
        key=path[len(prefix):] if path.startswith(prefix) else ''
        member,consumer,status,unit=fields.get(key,('—','无当前消费者','未实现/未解析','未确认'))
        if path=='Systems.DisplaySystem.DisplayBits':member,consumer,status,unit='displayBits','LogActiveIRSensorProfile；实际视频固定 8 bit','仅日志','bit'
        rows.append(dict(file=f,path=path,archivedValue=value,active=path in active_paths,parsed=member!='—',member=member,consumer=consumer,status=status,unit=unit,
            override='有效 DDS/显式兼容接口 > profile 缺省 > 代码' if status=='仅缺省' else '不启用归档遗留算法'))
    for path,value in leaves(active):
        if not path.startswith('HwaSimIR.'):continue
        key=path.split('.')[-1]
        members={'Gamma':'displayPresets[].gamma -> m_stage5SensorInputDisplayGamma','Gain':'displayPresets[].gain -> m_stage6DisplayConfig.displayGain','OffsetGray':'displayPresets[].offsetGray -> m_stage6DisplayConfig.displayOffset','WhiteHot':'displayPresets[].whiteHot -> m_stage6DisplayConfig.whiteHot','Mode':'displayPresets[].automatic -> m_stage6AgcEnabled',
          'DefaultPreset':'defaultDisplayPreset','SchemaVersion':'schemaVersion','Band':'band validation','Revision':'revision'}
        rows.append(dict(file=f,path=path,active=True,parsed=key in members,member=members.get(key,'—'),consumer='ApplyStage6DisplayConfig / final shader / pre-display AGC' if key in ['Gamma','Gain','OffsetGray','WhiteHot','Mode','DefaultPreset'] else '加载验证/来源日志',status='正常使用' if key in members else '追溯元数据',unit={'Gamma':'inverse exponent','Gain':'ratio','OffsetGray':'gray8','WhiteHot':'bool','Mode':'Fixed/Auto'}.get(key,'metadata'),override='env > 显式旧 INI > band profile > code; 每字段唯一来源'))
    all_rows+=rows
(out/'profile_fields.json').write_text(json.dumps(all_rows,ensure_ascii=False,indent=2),encoding='utf8')
lines=['# P6A 每波段字段消费清单','','表中旧系统字段以 Archive/P5 的精确原文件为盘点全集；active 标明精简配置中是否保留。JSON 加载不代表实现遗留算法。','','| 文件 / 完整 JSON 路径 | active / 解析 | 成员 | 消费者 | 状态 / 单位 |','|---|---|---|---|---|']
for r in all_rows:lines.append(f"| {r['file']} / `{r['path']}` | {int(r['active'])} / {int(r['parsed'])} | {r['member']} | {r['consumer']} | {r['status']} / {r['unit']} |")
(root/'docs/HwaSimIR_P6A_Profile_Field_Usage.md').write_text('\n'.join(lines)+'\n',encoding='utf8')
print(f'Enumerated {len(all_rows)} fields; JSON and Markdown written')
