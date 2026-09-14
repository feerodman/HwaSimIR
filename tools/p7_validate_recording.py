"""Validate saved MP4 products independently of the production C++ serializer.

Read MP4 sample bytes by ffprobe position, parse AVCC/SEI directly, compare the
embedded production key and annotation bytes with every sidecar row, and decode
all video frames to RGB hashes. Counts alone never produce a passing result.
"""
import argparse
import csv
import hashlib
import json
import struct
import subprocess
from pathlib import Path

UUID = bytes.fromhex('48776153696d49529a724f10816d0201')


def product(nal):
    if not nal or nal[0] & 31 != 6:
        return []
    rbsp = bytearray()
    zeros = 0
    for value in nal[1:]:
        if zeros == 2 and value == 3:
            zeros = 0
            continue
        rbsp.append(value)
        zeros = zeros + 1 if value == 0 else 0
    pos, found = 0, []
    while pos < len(rbsp) and rbsp[pos] != 128:
        kind = length = 0
        while rbsp[pos] == 255:
            kind += 255
            pos += 1
        kind += rbsp[pos]
        pos += 1
        while rbsp[pos] == 255:
            length += 255
            pos += 1
        length += rbsp[pos]
        pos += 1
        payload = bytes(rbsp[pos:pos+length])
        pos += length
        if kind != 5 or payload[:16] != UUID:
            continue
        wire, cursor = payload[16:], 0

        def read(fmt):
            nonlocal cursor
            value = struct.unpack_from('<'+fmt, wire, cursor)[0]
            cursor += struct.calcsize('<'+fmt)
            return value

        def string():
            nonlocal cursor
            size = read('I')
            value = wire[cursor:cursor+size]
            assert len(value) == size
            cursor += size
            return value

        assert read('I') == 2
        p = {'session': string().decode(), 'channel': string().decode()}
        for key in ['generation','run','frameSeq','sourceSeq']:
            p[key] = read('Q')
        for key in ['ptsMs','acceptedSteadyNs','executeSteadyNs','captureSteadyNs',
                    'encodeSteadyNs','writerSubmitSteadyNs']:
            p[key] = read('q')
        for key in ['platID','sensorID','round']:
            p[key] = read('i')
        p['annotationEnabled'] = bool(read('B'))
        p['saveRequested'] = bool(read('B'))
        p['body'] = string()
        found.append(p)
    return found


def validate(directory, ffmpeg, expected=None):
    probe = ffmpeg.with_name('ffprobe.exe' if ffmpeg.suffix == '.exe' else 'ffprobe')
    movie = directory/'output.mp4'
    probe_data = json.loads(subprocess.check_output([str(probe),'-v','error','-select_streams','v:0',
        '-show_packets','-show_streams','-of','json',str(movie)]))
    packets = probe_data['packets']
    rows = [json.loads(line) for line in (directory/'frame_index.jsonl').read_text().splitlines()]
    body_file = (directory/'producer_annotations.jsonl').read_bytes()
    stream = probe_data['streams'][0]
    length_size = int(stream.get('nal_length_size',4))
    assert len(rows) == len(packets), (len(rows),len(packets))
    if expected is not None:
        assert len(rows) == expected, (len(rows), expected)
    hashes_path = directory/'decoded_rgb.framemd5'
    subprocess.run([str(ffmpeg),'-v','error','-y','-threads','1','-i',str(movie),'-map','0:v:0','-an',
        '-threads','1','-filter_threads','1',
        '-fps_mode','passthrough','-enc_time_base','1:1000000','-pix_fmt','rgb24','-f','framemd5',str(hashes_path)],check=True)
    decoded = [line for line in hashes_path.read_text().splitlines() if line and not line.startswith('#')]
    assert len(decoded) == len(rows)
    results, keys = [], set()
    with movie.open('rb') as video:
        for ordinal, (row, packet, rgb) in enumerate(zip(rows,packets,decoded),1):
            video.seek(int(packet['pos']))
            data = video.read(int(packet['size']))
            offset, products = 0, []
            while offset < len(data):
                count = int.from_bytes(data[offset:offset+length_size],'big')
                offset += length_size
                nal = data[offset:offset+count]
                assert len(nal) == count
                offset += count
                products.extend(product(nal))
            assert len(products) == 1, (ordinal,'SEI product count',len(products))
            p = products[0]
            for key, value in p.items():
                if key == 'body':
                    continue
                assert str(row[key]) == str(value), (ordinal,key,row[key],value)
            identity = tuple(str(p[k]) for k in ['session','platID','sensorID','channel','generation','run','round','frameSeq'])
            assert identity not in keys, (ordinal,'duplicate production identity')
            keys.add(identity)
            assert int(row['storageIndex']) == ordinal
            start, count = int(row['annotationBodyOffset']),int(row['annotationBodyBytes'])
            actual_body = body_file[start:start+count]
            assert actual_body == p['body'], (ordinal,'body differs from actual MP4 SEI')
            digest = hashlib.sha256(actual_body).hexdigest()
            assert digest == row['annotationBodySha256'] == row['annotationSha256']
            annotation = json.loads(actual_body)
            assert int(annotation['frameSeq']) == p['frameSeq']
            assert int(annotation['ptsMs']) == p['ptsMs']
            file_pts = round(float(packet['pts_time'])*1e6)
            assert abs(file_pts-int(row['mp4PtsUs'])) <= 1, (ordinal,'MP4 PTS mismatch')
            assert stream['width'] == row['width'] and stream['height'] == row['height']
            results.append(dict(storageIndex=ordinal,session=p['session'],generation=p['generation'],
                run=p['run'],round=p['round'],frameSeq=p['frameSeq'],sourceSeq=p['sourceSeq'],
                ptsMs=p['ptsMs'],mp4PtsUs=file_pts,bodySha256=digest,bodyMatchesMp4=True,
                rgbMd5=rgb.split(',')[-1].strip(),width=row['width'],height=row['height'],
                queueWaitMs=row['queueWaitMs'],latencyEstimated=row['outputLatencyEstimated'],
                outputLatencyMs=row.get('outputLatencyMs',''),uncertaintyMs=row.get('clockUncertaintyMs','')))
    with (directory/'all_frame_correspondence.csv').open('w',newline='',encoding='utf-8-sig') as out:
        writer=csv.DictWriter(out,fieldnames=results[0].keys());writer.writeheader();writer.writerows(results)
    report=dict(result='PASS',frames=len(rows),independentEmbeddedIdentity=True,
        annotationBodyMatchesEveryMp4Sample=True,decodedEveryFrame=True,
        movieSha256=hashlib.sha256(movie.read_bytes()).hexdigest(),
        scope='Actual saved MP4, embedded production identity, complete bodies, per-frame index and RGB decode')
    (directory/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('directory',type=Path)
    parser.add_argument('--ffmpeg',type=Path,required=True)
    parser.add_argument('--expected',type=int)
    args=parser.parse_args()
    validate(args.directory,args.ffmpeg,args.expected)
