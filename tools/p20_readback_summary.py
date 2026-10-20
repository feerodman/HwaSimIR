"""只统计正式首条激励提交之后的同回合同步读回探针，不把INIT/空闲混入。"""
from pathlib import Path
import argparse,csv,json,re,statistics
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('case');a=ap.parse_args();folder=root/'logs/p20/runs'/a.case
result=dict(case=a.case,diagnostic=True,productionPerformance=False,scope='同步glFinish探针改变调度；首条FormalReferenceCommit后的N次读回与N个stage_render记录计数对齐；不包括INIT/空闲。',streams={})
for role in ['coarse','precise']:
 board=folder/role/'board';lines=(board/'board.log').read_text(encoding='utf-8',errors='replace').splitlines()
 first=next(i for i,l in enumerate(lines) if '[FormalReferenceCommit]' in l and 'sourceSeq=1 ' in l)
 count=sum(1 for p in board.glob('stage_render*.csv') for _ in csv.DictReader(p.open()))
 # std::cout可能在字段边界被其它线程插入换行；只接受完整字段，允许空白跨行。
 body='\n'.join(lines[first:])
 pattern=r'\[P20DirectRgbTiming\]\s+w=\s*(\d+)\s+h=(\d+)\s+gpuWaitMs=([\d.]+)\s+readMs=([\d.]+)\s+convertAndRestoreMs=([\d.]+)\s+diagnostic=1'
 rows=[[float(x) for x in m] for m in re.findall(pattern,body)][:count]
 assert count>0 and len(rows)==count,(role,count,len(rows))
 s=dict(samples=count,width=int(rows[0][0]),height=int(rows[0][1]))
 for index,key in [(2,'gpuWaitMs'),(3,'rgbReadMs'),(4,'convertAndRestoreMs')]:
  vals=sorted(r[index] for r in rows);s[key]=dict(mean=statistics.mean(vals),p50=statistics.median(vals),p95=vals[int(.95*(len(vals)-1))],maximum=vals[-1])
 result['streams'][role]=s
(folder/'readback_summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
