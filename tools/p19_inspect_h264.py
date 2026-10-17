"""Independent FFmpeg/SPS/PPS evidence. Raw-stream rate metadata is not network FPS."""
import argparse, collections, hashlib, json, re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('input');ap.add_argument('output');a=ap.parse_args()
source=Path(a.input).resolve();out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
bin=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'
cmd=[str(bin/'ffprobe.exe'),'-v','error','-select_streams','v:0','-show_streams','-show_frames','-show_entries','frame=pict_type,key_frame,pkt_size','-of','json',str(source)]
r=subprocess.run(cmd,capture_output=True,check=True);probe=json.loads(r.stdout);(out/'ffprobe.json').write_bytes(r.stdout)
trace=subprocess.run([str(bin/'ffmpeg.exe'),'-v','verbose','-i',str(source),'-map','0:v:0','-c:v','copy','-bsf:v','trace_headers','-frames:v','5','-f','null','-'],capture_output=True,check=True)
(out/'trace_headers.txt').write_bytes(trace.stderr)
data=source.read_bytes();starts=list(re.finditer(b'\x00\x00(?:\x00)?\x01',data));nals=[]
for i,m in enumerate(starts):
    end=starts[i+1].start() if i+1<len(starts) else len(data)
    if m.end()<end:nals.append(dict(type=data[m.end()]&31,bytes=end-m.end(),offset=m.start()))
types=collections.Counter(n['type'] for n in nals)
frames=collections.Counter(f['pict_type'] for f in probe['frames'])
report=dict(input=str(source),sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),decodedFrames=sum(frames.values()),frameTypes=frames,nalTypes=types,
            maxNalBytes=max(n['bytes'] for n in nals),nalCount=len(nals),stream=probe['streams'][0],rateBoundary='VUI/demux metadata only; no original transport timestamps',
            privateHwaFrameV2Present=bytes.fromhex('48776153696d49529a724f10816d0201') in data,privateIdentityCheck='actual HwaFrameV2 UUID; structured parser audit also retained',customerHardware='NOT_VERIFIED_CUSTOMER_HARDWARE')
(out/'report.json').write_text(json.dumps(report,indent=2));(out/'nals.json').write_text(json.dumps(nals))
print(json.dumps({k:v for k,v in report.items() if k!='stream'},indent=2))
