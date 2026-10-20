"""Actual board + production Qt + receiver run, with isolated output and explicit UTC."""
import argparse, csv, datetime, hashlib, json, os, subprocess, sys
from pathlib import Path
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--case',choices=['native','csv1','csv2','csv3','all-csv','smoke'],required=True)
ap.add_argument('--name',default='p20m3_'+datetime.datetime.now().strftime('%Y%m%d_%H%M%S'))
ap.add_argument('--receipt',default='logs/p20m3/20261021_engineering/deployment_v6/receipt.json')
ap.add_argument('--utc-date',required=True);ap.add_argument('--utc-hour',type=float,required=True)
ap.add_argument('--physical-evidence',action='store_true');a=ap.parse_args()
if 'HWASIMIR_SSH_PASSWORD' not in os.environ:raise SystemExit('Set HWASIMIR_SSH_PASSWORD for the authorized board account')
cases=['csv1','csv2','csv3'] if a.case=='all-csv' else [a.case]
for case in cases:
    name=a.name+'_'+case
    if case.startswith('csv'):
        filename={'csv1':'circleDataAll.csv','csv2':'circleDataAll2.csv','csv3':'circleDataAll3.csv'}[case]
        source=R/'1020测试内容'/filename
    else:
        out=R/'logs/p20m3'/name;out.mkdir(parents=True,exist_ok=False)
        original=R/'1020测试内容/circleDataAll3.csv'
        rows=list(csv.reader(original.open(encoding='utf-8-sig')));row=rows[1][:]
        # Explicit controlled fixture, not substituted for any customer trajectory.
        for i,v in {2:40,3:100,4:10000,8:0,10:40.00899321605919,11:100,12:10000,13:90,14:0,15:0,16:0,28:0,29:0,53:1}.items():row[i]=str(v)
        source=out/'native60.csv'
        with source.open('w',newline='',encoding='utf-8') as f:
            w=csv.writer(f);w.writerow(rows[0])
            for i in range(4261 if case=='native' else 180):row[0]=format(i*1000/60,'.12f');w.writerow(row)
    rows=list(csv.reader(source.open(encoding='utf-8-sig')))[1:]
    cmd=[sys.executable,str(R/'tools/p20_board_run.py'),'--name',name,'--frames',str(len(rows)),'--source',str(source),'--receipt',a.receipt,
        '--tmp-board-logs','--control-domain','150','--video-domain','1','--customer-domain','1','--use-deployed-domains','--use-deployed-qos',
        '--band','2','--precise-band','0','--fps','60','--input-hz','60','--utc-date',a.utc_date,'--utc-hour',str(a.utc_hour),
        '--stim-directory','build-p20m3-stim/release','--sender-config','build-p20m3-stim/release/NetworkConfig.ini',
        '--receiver-directory','build-p20m3-customer-receiver/release','--scheduling','performance']
    if a.physical_evidence:cmd+=['--physical-evidence']
    subprocess.run(cmd,cwd=R,check=True)
    d=R/'logs/p20/runs'/name
    (d/'source_identity.json').write_text(json.dumps(dict(source=str(source),sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        rows=len(rows),sourceSimulationSpanMs=float(rows[-1][0])-float(rows[0][0]),wallDeadlineSpanMs=(len(rows)-1)*1000/60,
        sourceTimestampsRewritten=False if case.startswith('csv') else 'native_60Hz_fixture',utcDate=a.utc_date,utcHour=a.utc_hour),indent=2),encoding='utf-8')
    subprocess.run([sys.executable,str(R/'tools/p20m3_performance.py'),name],cwd=R,check=True)
    if a.physical_evidence:subprocess.run([sys.executable,str(R/'tools/p20m3_evidence.py'),str(d)],cwd=R,check=True)
