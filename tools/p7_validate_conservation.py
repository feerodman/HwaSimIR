"""Audit writer -> board acceptance separately from per-frame file correctness.

The board sourceSeq is an accepted-input ordinal, not a sender wire sequence.
This audit detects count loss before acceptance; it cannot identify or replay
individual upstream losses, and never pairs annotation messages by arrival.
"""
import argparse, json, re
from pathlib import Path


def read(path):
    data = path.read_bytes() if path.exists() else b''
    return data.decode('utf-16' if data.startswith((b'\xff\xfe', b'\xfe\xff')) else 'utf-8-sig', errors='replace')


def audit(case):
    senders = sorted(case.glob('stim*.err.log'))
    sent = []
    errors = []
    for path in senders:
        values = re.findall(r'\[StimFinal\].*?successfulRealtimeWrites=(\d+)', read(path))
        sent.extend(int(v) for v in values)
    for path in sorted(case.glob('stim*.log')):
        for line in read(path).splitlines():
            if re.search(r'Send message failed|reconnect|\[StimDrain\]\[ERROR\]', line, re.I):
                errors.append(dict(file=path.name, message=line))
    log = read(case/'board.log') if (case/'board.log').exists() else read(case/'hwa.out.log') + read(case/'hwa.err.log')
    rounds = [dict(re.findall(r'(\w+)=([^ ]+)', line)) for line in re.findall(r'\[SyncRoundConservation\] ([^\r\n]+)', log)]
    accepted = [int(r['acceptedRealtime']) for r in rounds]
    measured = bool(sent) and len(sent) == len(accepted)
    equal = measured and sent == accepted
    captured = all(r['mode'] != 'sync' or int(r['inputMinusCaptured']) == 0 for r in rounds)
    verdict = 'NOT_MEASURED' if not measured else ('PASS' if equal and captured and not errors else 'FAIL')
    files = []
    for movie in sorted((case/'recording').glob('*/output.mp4')):
        index = movie.parent/'frame_index.jsonl'
        validation = movie.parent/'validation.json'
        rows = [json.loads(line) for line in read(index).splitlines()] if index.exists() else []
        files.append(dict(recording=movie.parent.name, indexedFrames=len(rows),
            fullFileValidation=json.loads(read(validation)).get('result') if validation.exists() else 'NOT_VERIFIED'))
    value = dict(case=case.name, result=verdict, successfulWriterCounts=sent, boardAcceptedCounts=accepted,
        acceptedMinusWriter=[a-s for a,s in zip(accepted,sent)], rounds=rounds, transportEvents=errors,
        recordings=files, scope='Upstream aggregate conservation, independent of embedded identity / body / MP4 validation',
        limitation='Counts cannot locate pre-acceptance losses. Async outputs may repeat accepted states. Late-join/reconnect saved subsets require their explicit gap evidence.')
    (case/'conservation.json').write_text(json.dumps(value,indent=2),encoding='utf-8')
    return value


if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('cases',nargs='+',type=Path);parser.add_argument('--strict',action='store_true');args=parser.parse_args()
    values=[audit(case) for case in args.cases]
    for value in values: print(json.dumps(value))
    if args.strict and any(value['result']!='PASS' for value in values):raise SystemExit(1)
