"""Serial full-file verification of named, completed P7 cases."""
import argparse, json
from pathlib import Path
from p7_validate_recording import validate
from p7_case_metrics import report
from p7_validate_conservation import audit

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('cases',nargs='+',type=Path)
    parser.add_argument('--ffmpeg',type=Path,required=True);args=parser.parse_args()
    results=[]
    for case in args.cases:
        directories=sorted((case/'recording').glob('*/output.mp4'))
        if not directories:raise RuntimeError('Missing actual recording '+str(case))
        for movie in directories:
            try:
                validate(movie.parent,args.ffmpeg)
                value=json.loads((movie.parent/'validation.json').read_text())
                results.append(dict(case=case.name,recording=movie.parent.name,**value))
            except Exception as error:
                results.append(dict(case=case.name,recording=movie.parent.name,result='FAIL',error=repr(error)))
        report(case)
        audit(case) # Separate artifact; file PASS never implies upstream PASS.
    output=args.cases[0].parent/'file_validation_last_batch.json'
    output.write_text(json.dumps(results,indent=2),encoding='utf-8')
    if any(row['result']!='PASS' for row in results):raise SystemExit(1)
