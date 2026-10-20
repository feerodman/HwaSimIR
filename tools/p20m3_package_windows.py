import configparser,hashlib,json,os,shutil
from pathlib import Path
R=Path(__file__).resolve().parents[1]
out=R/'logs/p20m3/20261021_engineering'
dst=R/'build-p20m3-renderer/bin';src=R/'HwaSim_IR/Bin'
def ignore(path,names):
 p=Path(path)
 if p.name=='MODTRAN':return [n for n in names if n!='processed']
 if p.name=='processed' and p.parent.name=='MODTRAN':return [n for n in names if n not in ('band_lut_si.csv','solar_heating_lut_si.csv','p13_coverage_manifest.json','p14_coverage_manifest.json')]
 return []
for name in ('Config','Panda3D-1.10'):
 if not (dst/name).exists():shutil.copytree(src/name,dst/name,copy_function=os.link,ignore=ignore)
for p in src.iterdir():
 if p.suffix=='.dll' or p.name=='zrddslicence.lic':
  if not (dst/p.name).exists():os.link(p,dst/p.name)
parameter=R/'HwaSim_IR/Bin/Config/Atmosphere/Engineering/P20M3_GREY_GAS_1/parameters.txt';sha=hashlib.sha256(parameter.read_bytes()).hexdigest()
shutil.copy2(parameter,dst/'Config/Atmosphere/engineering_atmosphere.txt')
for name in ('provenance.json','quality.json'):shutil.copy2(parameter.parent/name,dst/'Config/Atmosphere'/('P20M3_'+name))
def config(path,changes):
 c=configparser.ConfigParser(interpolation=None,strict=False);c.optionxform=str;c.read(path,encoding='utf-8-sig')
 for section,values in changes.items():
  if not c.has_section(section):c.add_section(section)
  for k,v in values.items():c[section][k]=str(v)
  if section=='M1NirMwirPhysics':
   for obsolete in ('EnableSWIR','EnableMWIR'):c.remove_option(section,obsolete)
 tmp=path.with_suffix('.p20m3.tmp')
 with tmp.open('w',encoding='utf-8') as f:c.write(f,space_around_delimiters=False)
 os.replace(tmp,path)
physics=dict(AtmospherePolicy='EngineeringContinuous',EngineeringParameters=str(dst/'Config/Atmosphere/engineering_atmosphere.txt').replace('\\','/'),EngineeringParameterSha256=sha,CompareOnly='false',EnableRuntime='true',EnableSWIRRuntime='true',EnableMWIRRuntime='true',UtcTimeMode='UnixEpochMs')
config(dst/'Config/HwaSimIRRuntime.ini',{'M1NirMwirPhysics':physics,'Debug':{'ExitOnStop':'true'},'DdsProtocol':{'DomainId':151},'DdsVideo':{'DomainId':151},'CustomerDdsVideo':{'DomainId':151}})
stim=R/'build-p20m3-stim/release'
config(stim/'NetworkConfig.ini',{'M1NirMwirPhysics':physics,'Demo':{'UtcDate':'2026-10-21','UtcHour':'3.5'},'DdsProtocol':{'DomainId':151,'QosFile':str(dst/'Config/DDS/ZRDDS_PROTOCOL_QOS.xml').replace('\\','/')}})
(out/'windows_package.json').write_text(json.dumps(dict(renderer=str(dst/'HwaSim_IR.exe'),stim=str(stim/'DataDrivenTestQT.exe'),parameter_sha256=sha,quality='ENGINEERING_APPROX',renderer_sha256=hashlib.sha256((dst/'HwaSim_IR.exe').read_bytes()).hexdigest(),stim_sha256=hashlib.sha256((stim/'DataDrivenTestQT.exe').read_bytes()).hexdigest()),indent=2))
print(dst)
