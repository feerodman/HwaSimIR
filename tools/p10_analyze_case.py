"""Reuse independent production-identity MP4 checks and local clock ledgers."""
import argparse, json
from pathlib import Path
from p7_validate_recording import validate
from p8_validate_saved_cases import input_products
from p8_case_metrics import report

ROOT=Path(__file__).resolve().parents[1]
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('case',type=Path);p.add_argument('--seconds',type=float,required=True);a=p.parse_args()
    for movie in sorted((a.case/'recording').glob('*/output.mp4')):validate(movie.parent,FF,None)
    input_products(a.case)
    value=report(a.case,warmup=0,seconds=a.seconds)
    products=json.loads((a.case/'input_product_validation.json').read_text())
    assert value['inputAudit']['result']=='PASS' and products['result']=='PASS'
