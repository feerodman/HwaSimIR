"""Metadata completeness check only: never loads calibration into an image model."""
import argparse
import json
from pathlib import Path
from curve_io import load_config

REQUIRED = ('manufacturer','model','serial_or_population','detector_type','temperature_K',
            'readout_mode','exposure_definition','pitch_um','spectral_response',
            'optical_transmission','dark_electrons_per_pixel_s','read_noise_e_rms',
            'full_well_e','conversion_DN_per_e','black_level_DN','adc_bits',
            'measurement_conditions','measurement_sources','usage_rights')
LEVELS=('synthetic_reference','documented_example','device_characterized','device_validated')


def assess(record):
    if record.get('declared_level') not in LEVELS:
        raise ValueError('unknown provenance level')
    missing=[k for k in REQUIRED if record.get(k) is None or record.get(k)=='' or record.get(k)==[]]
    # Completeness cannot certify truth, consistent conditions, or measured agreement.
    return dict(declared_level=record['declared_level'],missing=missing,
                metadata_complete=not missing,device_characterization_verified=False,
                independent_measurement_agreement_verified=False,production_import_supported=False,
                result='incomplete' if missing else 'requires manual source and common-condition verification')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('record',type=Path)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    config,sources=load_config(args.record);result=assess(config);result['sources']=sources
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print(result['result'],len(result['missing']),'missing fields; no calibration import')
