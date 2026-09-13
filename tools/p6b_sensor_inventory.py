"""Read-only metadata inventory of the two explicitly authorized installation roots.

Writes hashes/units/status only. Does not copy proprietary resource bytes, load
plugins, inspect license keys, or import commercial SDKs into the sensor lab.
"""
from pathlib import Path
import hashlib
import json
import re
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OND = Path('D:/Presagis/Suite22/Ondulus_IR_22_0')
VEGA = Path('D:/Presagis/Suite22/Vega_Prime_22_0')
items = []


def add(path, category, units, scope, experiment, note=''):
    path=Path(path)
    if not path.is_file():
        raise FileNotFoundError(path)
    data=path.read_bytes()
    item=dict(path=str(path.resolve()),sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),
              format=path.suffix.lower(),category=category,units=units,scope=scope,
              source='installed Ondulus IR 22.0' if path.is_relative_to(OND) else 'installed Vega Prime 22.0',
              readable=True,local_experiment=experiment,redistribution='not_established_do_not_bundle',
              rights_basis='installed proprietary material; no dataset redistribution grant verified',note=note)
    if path.suffix.lower()=='.png':
        with Image.open(path) as im:
            item.update(size=list(im.size),channels=im.mode)
    items.append(item)
    return item


for p in sorted((OND/'data/IIT').glob('QE_*.txt')):
    rows=[]
    for line in p.read_text(encoding='utf8',errors='replace').splitlines()[1:]:
        try:row=[float(x) for x in line.split()]
        except ValueError:continue
        if len(row)==2:rows.append(row)
    item=add(p,'QE','wavelength um; header says percent; numerical normalization unresolved',
             'image intensifier photocathode example, not ordinary CMOS calibration','blocked_unit_semantics',
             'Do not reinterpret 0..0.3 values as fractions without resolving file reader semantics. No conversion guessed.')
    item.update(sample_count=len(rows),wavelength_range_um=[rows[0][0],rows[-1][0]],value_range=[min(y for x,y in rows),max(y for x,y in rows)])
for p in sorted((OND/'data/MTF').glob('*.txt')):
    add(p,'MTF','cycles/mm; dimensionless normalized MTF','synthetic/example spatial response','readable_reference_only',
        'cycles/pixel = cycles/mm * pitch_mm. Requires correct pixel pitch; no measured-device provenance found.')
for name,category,units,scope in [
    ('Spectral_Response_subsystem.html','SRF','normalized relative energy response; wavelength um','relative response can encode wavelength-dependent QE; constant QE also used downstream'),
    ('Optics_Transmission_subsystem.html','optics','fraction 0..1','documented default constant 1, no measured lens/filter curve'),
    ('Integrated_Radiance_subsystem.html','optics_factor_accounting','spectral integration','optics transmission and spectral response contributions'),
    ('Sensor_Configuration_subsystem.html','sensor_configuration','API-dependent','parameter definitions, not device calibration'),
    ('Sensor_Noise_subsystem.html','noise','electrons; dark density A/m2; NEP pW/sqrt(Hz); NETD K','model description; no identified per-device read-noise measurement'),
    ('Detector_Signal_Transfer_subsystem.html','ADC','electrons; Counts/electron; thermal branch V/nW and V','photon and thermal branches are distinct'),
    ('Itensifier_Configuration_subsystem.html','intensifier','photometric/radiometric mixed model','intensifier gain/QE/saturation, not CMOS parameters')]:
    add(OND/'docs/help'/name,category,units,scope,'documentation_reference')
for name,category,units in [
    ('IRSensorConfig/SensorConfigurationSystem.h','exposure_pitch_QE_well_dark_ADC','IntegrationTime us; QE fraction; well electron; dark A/m2; dimensions typed'),
    ('IRSensorConfig/SpectralResponseSystem.h','SRF_API','relative spectral response'),
    ('IRSensorConfig/OpticsTransmissionSystem.h','optics_API','fraction 0..1'),
    ('IRSensor/SensorNoiseSystem.h','noise_API','model noise, not measured readout specification'),
    ('Core/Dimension.h','unit_types','typed m/mm/um conversion; serialized sensor field convention not verified')]:
    add(OND/'include/Ondulus'/name,category,units,'public installed SDK headers read as documentation','documentation_reference')
for name in ['mainwindow_configurationtab.cpp','mainwindow_documentation.cpp','mainwindow_sensortab.cpp']:
    add(OND/'samples/IRUI'/name,'UI_units','pitch UI um; integration time us; other fields as documented',
        'sample UI parameter bindings','sample_code_reference',
        'Header grants licensed end-user sample-source use; this does not establish a grant for neighboring data files. No source copied.')
for name in ['default_LLLTV.json','LLLTV_VIS-NIR.json','default_NVG.json','default_MWIR.json','default_LWIR.json']:
    p=OND/'data/configuration'/name
    add(p,'example_profile','field-specific; detector pitch JSON serialization unit not inferred from UI',
        'example configurations, mixed photon/thermal/intensifier branches; no identified ordinary detector datasheet',
        'reference_only_not_used_in_lab')
for name in ['3D_cloud_textures.html','3D_clouds.html']:
    add(VEGA/'docs/help'/name,'cloud_construction','visual shape/color/alpha; not measured density units',
        'gray texture + transparency for billboards/cloud puffs','visual_reference_only')
add(VEGA/'docs/help/images/download/attachments/50582129/3d_cloud_textures_01-03.png','cloud_reference',
    'visual reference only','local documentation thumbnail, not delivered as project output','visual_reference_only')
for name in ['vpEnvCloud3D.h','vpEnvCloud3DGenerator.h','vpEnvCloud3DPuff.h']:
    add(VEGA/'include/vegaprime'/name,'cloud_API','API geometry/texture definitions',
        'normal 3D graphical clouds, not sensor calibration','documentation_reference')

# Search scope is recorded without copying arbitrary files or license directories.
report=dict(schema='local_sensor_resources_readiness_1',roots=[str(OND),str(VEGA)],
            procedure='read-only selected docs/help, include, samples, data; filenames searched with rg; no binary reverse engineering',
            not_found=['ordinary camera identified manufacturer SRF/QE measurements','measured optical/filter transmission curve',
                       'ordinary detector per-device read noise, conversion gain and black-level calibration'],
            limitations=['No redistribution rights inferred from installation or a sample-source exception.',
                         'No commercial curve or proprietary image was loaded into the independent lab or deployment.',
                         'Vega documentation references original .inta cloud textures; no matching standalone files found under the supplied Vega root.'],
            resources=items)
dest=ROOT/'docs/HwaSimIR_Sensor_Resources_Readiness.json'
dest.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print(f'{len(items)} files hashed; metadata only: {dest}')
