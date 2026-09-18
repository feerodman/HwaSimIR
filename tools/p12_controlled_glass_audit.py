#!/usr/bin/env python3
"""Audit real RK3588 RGBA16F -> H.264 DDS controlled-glass cases."""
from __future__ import annotations

import csv
import hashlib
import html
import json
import re
import shutil
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "logs" / "p7"
OUTPUT = ROOT / "logs" / "p12" / "p12c" / "controlled_glass" / "evidence"
ROI = (280, 80, 520, 260)  # x0,y0,x1,y1, safely inside the front glass rectangle
CASES = [(band, background, transmission) for band in ("swir", "mwir")
         for background in ("bright", "dark") for transmission in ("on", "blocked")]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""): digest.update(block)
    return digest.hexdigest()


def read_pfm(path: Path) -> np.ndarray:
    with path.open("rb") as stream:
        magic = stream.readline().strip(); width, height = map(int, stream.readline().split()); scale = float(stream.readline())
        channels = 3 if magic == b"PF" else 1
        values = np.fromfile(stream, dtype="<f4" if scale < 0 else ">f4")
    if values.size != width * height * channels: raise RuntimeError(f"PFM size mismatch: {path}")
    return values.reshape(height, width, channels)


def last_metrics(text: str) -> dict:
    rows = re.findall(r"^\[RuntimeMetricsV2\]\s+(\{.*\})$", text, re.MULTILINE)
    return json.loads(rows[-1]) if rows else {}


def compare(left: dict, right: dict, label: str, band: str) -> dict:
    x0, y0, x1, y1 = ROI
    png_delta = np.abs(left["png"].astype(np.int16) - right["png"].astype(np.int16))
    raw_delta = np.abs(left["raw"] - right["raw"])
    roi_png, roi_raw = png_delta[y0:y1, x0:x1], raw_delta[y0:y1, x0:x1]
    heat = np.clip(png_delta.max(axis=2) * 4, 0, 255).astype(np.uint8)
    heat_path = OUTPUT / f"diff_{band}_{label}_dds_x4.png"; Image.fromarray(heat, "L").save(heat_path)
    raw_heat = raw_delta.max(axis=2)
    raw_scale = float(np.percentile(raw_heat, 99.9))
    raw_view = np.zeros(raw_heat.shape, dtype=np.uint8) if raw_scale <= 0 else np.clip(raw_heat / raw_scale * 255, 0, 255).astype(np.uint8)
    raw_heat_path = OUTPUT / f"diff_{band}_{label}_raw_normalized.png"; Image.fromarray(raw_view, "L").save(raw_heat_path)
    return {
        "label": label, "band": band, "left": left["label"], "right": right["label"],
        "roi": {"x0": x0, "y0": y0, "x1": x1, "y1": y1, "pixels": (x1-x0)*(y1-y0)},
        "dds": {"meanAbsChannel": float(roi_png.mean()), "maxAbsChannel": int(roi_png.max()),
                "changedPixelsGt1": int(np.any(roi_png > 1, axis=2).sum()), "diffImage": heat_path.name},
        "raw": {"meanAbsRadiance": float(roi_raw.mean()), "maxAbsRadiance": float(roi_raw.max()),
                "changedPixelsGt1e7": int(np.any(roi_raw > 1e-7, axis=2).sum()),
                "unit": "W/(m^2 sr um)", "diffImage": raw_heat_path.name},
    }


def main() -> int:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    rows, data, failures = [], {}, []
    for band, background, transmission in CASES:
        label = f"{band}_{background}_{transmission}"
        source = SOURCE / f"p12_glass_{label}"
        required = [source/n for n in ("received.png", "linear_rgb8.png", "linear.pfm", "received.h264", "board.log", "video.err.log", "effective_environment.json", "request.json", "release.sha256")]
        missing = [str(p) for p in required if not p.is_file()]
        if missing: failures.append(f"{label}: missing={missing}"); continue
        board = (source/"board.log").read_text(encoding="utf-8", errors="replace")
        video = (source/"video.err.log").read_text(encoding="utf-8", errors="replace")
        metrics = last_metrics(video)
        expected_fixture = f"background={background.title()} transmission={'On' if transmission == 'on' else 'Blocked'}"
        checks = {
            "explicitFixture": "[P12 ControlledGlassFixture]" in board and expected_fixture in board,
            "explicitComposite": f"[P12 ControlledGlassComposite] transmission={'On' if transmission == 'on' else 'Blocked'}" in board,
            "rgba16fActual": "[Stage6 RawRadianceBuffer]" in board and "actualTextureComponentType=half_float" in board and "actualTextureComponents=4" in board and "actualAlphaBits=16" in board,
            "rawAttachmentVerified": "[Stage6 RawAttachment]" in board and "textureStorageVerified=1" in board,
            "ddsDecoded": int(metrics.get("decodedFrames", 0)) >= 470,
            "ddsErrorsZero": int(metrics.get("ddsErrors", -1)) == 0 and int(metrics.get("decodeErrors", -1)) == 0,
            "identityRejectedZero": int(metrics.get("statusIdentityRejected", -1)) == 0,
            "frameIdentityMatched": "mismatch=0" in video and "association=AU_SEI_V2" in video,
            "h264AnnexBPresent": (source/"received.h264").stat().st_size > 1000,
            "calibrationLimitLogged": "calibration=NOT_VERIFIED_CALIBRATION" in board,
        }
        case_out = OUTPUT / label; case_out.mkdir(exist_ok=True)
        for src_name, dst_name in (("received.png","received_dds.png"),("linear_rgb8.png","linear_diagnostic.png"),("linear.pfm","raw_radiance.pfm"),("video.err.log","dds_receiver.log"),("board.log","board.log"),("effective_environment.json","effective_environment.json"),("request.json","request.json"),("release.sha256","release.sha256")):
            shutil.copy2(source/src_name, case_out/dst_name)
        png = np.asarray(Image.open(source/"received.png").convert("RGB"), dtype=np.uint8)
        raw = read_pfm(source/"linear.pfm")
        checks["rawFinite800x800"] = raw.shape == (800,800,3) and bool(np.isfinite(raw).all())
        record = {
            "schema": "hwasimir.p12.controlled-glass-case.v1", "label": label,
            "band": band.upper(), "background": background.title(), "transmission": transmission.title(),
            "result": "PASS" if all(checks.values()) else "FAIL", "checks": checks, "metrics": metrics,
            "roi": {"x0": ROI[0], "y0": ROI[1], "x1": ROI[2], "y1": ROI[3], "meaning": "inside front glass, excluding edges and background border"},
            "artifacts": {"received": "received_dds.png", "raw": "raw_radiance.pfm", "linearPreview": "linear_diagnostic.png", "boardLog": "board.log", "receiverLog": "dds_receiver.log"},
            "sha256": {"received": sha256(source/"received.png"), "raw": sha256(source/"linear.pfm"), "h264": sha256(source/"received.h264"), "boardLog": sha256(source/"board.log")},
            "provenance": {"received": "actual Windows DDS H.264 decoder dump", "raw": "actual RK3588 RGBA16F render-target RGB readback serialized as PFM", "sameRun": True},
            "modelScope": "single straight-through composite only; no refraction, Fresnel or multiple reflection",
            "calibration": "NOT_VERIFIED_CALIBRATION",
        }
        (case_out/"case.json").write_text(json.dumps(record, indent=2), encoding="utf-8")
        if record["result"] != "PASS": failures.append(f"{label}: checks={[k for k,v in checks.items() if not v]}")
        record["receivedCopy"] = f"{label}/received_dds.png"; rows.append(record)
        data[label] = {"label": label, "png": png, "raw": raw}

    comparisons = []
    for band in ("swir", "mwir"):
        specs = [
            ("transmission_on_minus_blocked_bright", f"{band}_bright_on", f"{band}_bright_blocked", "positive"),
            ("background_bright_minus_dark_transmission_on", f"{band}_bright_on", f"{band}_dark_on", "positive"),
            ("background_bright_minus_dark_transmission_blocked", f"{band}_bright_blocked", f"{band}_dark_blocked", "zero"),
        ]
        for label, left, right, expectation in specs:
            item = compare(data[left], data[right], label, band)
            if expectation == "positive":
                passed = item["dds"]["changedPixelsGt1"] > 40000 and item["raw"]["changedPixelsGt1e7"] > 40000
            else:
                passed = item["dds"]["changedPixelsGt1"] == 0 and item["raw"]["maxAbsRadiance"] <= 1e-7
            item["expectation"] = expectation; item["result"] = "PASS" if passed else "FAIL"
            if not passed: failures.append(f"comparison {band}/{label} failed")
            comparisons.append(item)

    # Received DDS contact sheet; raw diagnostics are kept separately and never substituted.
    font = ImageFont.load_default(); tiles=[]
    for row in rows:
        im=Image.open(OUTPUT/row["receivedCopy"]).convert("RGB"); im.thumbnail((300,300)); tiles.append((row,im.copy()))
    sheet=Image.new("RGB",(1200,2*350),"#111318"); draw=ImageDraw.Draw(sheet)
    for i,(row,im) in enumerate(tiles):
        x=(i%4)*300;y=(i//4)*350;sheet.paste(im,(x,y));draw.text((x+5,y+305),f"{row['label']}\nDDS={row['metrics'].get('decodedFrames')} {row['result']}",fill="white",font=font)
    sheet.save(OUTPUT/"contact_sheet_received_dds.png")

    result = "PASS" if not failures and len(rows)==8 else "FAIL"
    report={"schema":"hwasimir.p12.controlled-glass-audit.v1","result":result,"caseCount":len(rows),"failures":failures,
            "rgba16f":"actual half-float 4-component target verified in every board log","transport":"H.264 Annex-B over DDS, decoded on Windows",
            "roi":{"x0":ROI[0],"y0":ROI[1],"x1":ROI[2],"y1":ROI[3]},"cases":rows,"comparisons":comparisons,
            "calibration":"NOT_VERIFIED_CALIBRATION"}
    (OUTPUT/"audit.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    with (OUTPUT/"audit.csv").open("w",newline="",encoding="utf-8-sig") as f:
        w=csv.writer(f);w.writerow(["case","band","background","transmission","result","ddsDecoded","receivedSha256","rawSha256"])
        for r in rows:w.writerow([r["label"],r["band"],r["background"],r["transmission"],r["result"],r["metrics"].get("decodedFrames"),r["sha256"]["received"],r["sha256"]["raw"]])
    case_html="".join(f"<tr><td>{html.escape(r['label'])}</td><td>{r['result']}</td><td>{r['metrics'].get('decodedFrames')}</td><td><a href='{r['receivedCopy']}'><img src='{r['receivedCopy']}'></a></td></tr>" for r in rows)
    comp_html="".join(f"<tr><td>{html.escape(c['band']+'/'+c['label'])}</td><td>{c['result']}</td><td>{c['dds']['changedPixelsGt1']}</td><td>{c['raw']['meanAbsRadiance']:.9g}</td><td><img src='{c['dds']['diffImage']}'></td><td><img src='{c['raw']['diffImage']}'></td></tr>" for c in comparisons)
    (OUTPUT/"index.html").write_text(f"""<!doctype html><meta charset='utf-8'><title>P12 controlled glass</title><style>body{{font:14px system-ui;background:#111;color:#eee;margin:24px}}table{{border-collapse:collapse}}td,th{{border:1px solid #555;padding:6px}}img{{width:260px;height:260px;object-fit:contain;background:#000}}code{{color:#9ef}}</style><h1>P12 controlled glass DDS evidence - {result}</h1><p>Front glass + known Bright/Dark plate; fixed camera/epoch/config. Actual RK3588 <code>RGBA16F</code> raw path and actual Windows DDS H.264 decode are distinct artifacts from the same run.</p><p><code>NOT_VERIFIED_CALIBRATION</code>. Engineering assumptions only; single straight-through composite, no refraction/Fresnel/multiple reflection.</p><p><a href='audit.json'>JSON</a> | <a href='audit.csv'>CSV</a> | <a href='contact_sheet_received_dds.png'>DDS contact sheet</a></p><h2>Cases</h2><table><tr><th>case</th><th>result</th><th>decoded</th><th>received DDS</th></tr>{case_html}</table><h2>ROI comparisons</h2><table><tr><th>comparison</th><th>result</th><th>DDS changed px</th><th>raw mean abs</th><th>DDS diff x4</th><th>raw normalized diff</th></tr>{comp_html}</table>""",encoding="utf-8")
    print(json.dumps({"result":result,"cases":len(rows),"comparisons":comparisons,"failures":failures},indent=2))
    return 0 if result=="PASS" else 1

if __name__ == "__main__": raise SystemExit(main())
