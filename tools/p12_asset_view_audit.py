"""Audit the P12 controlled asset-view DDS captures and build an offline gallery.

The images are decoded receiver outputs.  MaterialView=2 is an explicit synthetic
normal diagnostic for geometry/pose/display regression, not physical radiance.
"""
from __future__ import annotations

import argparse
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


def resolve_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else ROOT / path


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def last_metrics(text: str) -> dict:
    matches = re.findall(r"\[RuntimeMetricsV2\]\s+(\{[^\r\n]+\})", text)
    if not matches:
        raise RuntimeError("receiver log has no RuntimeMetricsV2")
    return json.loads(matches[-1])


parser = argparse.ArgumentParser()
parser.add_argument("--logs-root", default="logs/p5")
parser.add_argument("--output", default="logs/p12/p12b/asset_views")
args = parser.parse_args()
logs_root = resolve_path(args.logs_root)
output = resolve_path(args.output)
images_out = output / "images"
logs_out = output / "logs"
images_out.mkdir(parents=True, exist_ok=True)
logs_out.mkdir(parents=True, exist_ok=True)

assets = ("f35", "f22", "aim120", "aim9x")
bands = ("swir", "mwir")
views = ("near", "side")
rows = []

for asset in assets:
    for band in bands:
        for view in views:
            case_name = f"p12_{asset}_{band}_{view}_normals"
            if (asset, band, view) == ("f35", "swir", "near"):
                case_name = "p12_f35_swir_near_normals_scaled"
            case_dir = logs_root / case_name
            image_path = case_dir / "received.png"
            board_path = case_dir / "board.log"
            receiver_path = case_dir / "video.err.log"
            request_path = case_dir / "request.json"
            for required in (image_path, board_path, receiver_path, request_path):
                if not required.is_file():
                    raise FileNotFoundError(required)

            board = board_path.read_text(encoding="utf-8", errors="replace")
            receiver = receiver_path.read_text(encoding="utf-8", errors="replace")
            metrics = last_metrics(receiver)
            array = np.asarray(Image.open(image_path).convert("L"), dtype=np.uint8)
            stats = {
                "min": int(array.min()),
                "max": int(array.max()),
                "mean": float(array.mean()),
                "stddev": float(array.std()),
                "p01": float(np.percentile(array, 1)),
                "p50": float(np.percentile(array, 50)),
                "p99": float(np.percentile(array, 99)),
                "nonzeroPixels": int(np.count_nonzero(array)),
                "pixelCount": int(array.size),
            }
            diagnostic_ok = (
                f"scene={asset}" in board
                and f"view={view}" in board
                and "materialView=2" in board
                and "diagnosticEncoding=scene_wide_window_radiance" in board
                and "values=artificial_game_only" in board
            )
            transport_ok = (
                "running=1 codec=h264" in board
                and int(metrics.get("decodedFrames", 0)) >= 60
                and int(metrics.get("ddsErrors", -1)) == 0
                and int(metrics.get("decodeErrors", -1)) == 0
                and int(metrics.get("statusIdentityRejected", -1)) == 0
            )
            graphics_ok = not re.search(r":display[^\r\n]*\(error\)|GL error 0x", board)
            pixels_ok = stats["max"] >= 96 and stats["stddev"] >= 3.0
            passed = diagnostic_ok and transport_ok and graphics_ok and pixels_ok

            stem = f"{asset}_{band}_{view}"
            copied_image = images_out / f"{stem}_received.png"
            copied_board = logs_out / f"{stem}_board.log"
            copied_receiver = logs_out / f"{stem}_receiver.log"
            copied_request = logs_out / f"{stem}_request.json"
            shutil.copy2(image_path, copied_image)
            shutil.copy2(board_path, copied_board)
            shutil.copy2(receiver_path, copied_receiver)
            shutil.copy2(request_path, copied_request)
            rows.append({
                "asset": asset,
                "band": band.upper(),
                "view": view,
                "sourceCase": case_name,
                "receivedImage": copied_image.relative_to(output).as_posix(),
                "receivedSha256": sha256(copied_image),
                "boardLogSha256": sha256(copied_board),
                "receiverLogSha256": sha256(copied_receiver),
                "decodedFrames": int(metrics.get("decodedFrames", 0)),
                "ddsErrors": int(metrics.get("ddsErrors", -1)),
                "decodeErrors": int(metrics.get("decodeErrors", -1)),
                "statusIdentityRejected": int(metrics.get("statusIdentityRejected", -1)),
                **stats,
                "diagnosticOk": diagnostic_ok,
                "transportOk": transport_ok,
                "graphicsOk": graphics_ok,
                "pixelsOk": pixels_ok,
                "pass": passed,
            })

if not all(row["pass"] for row in rows):
    failed = [f"{r['asset']}/{r['band']}/{r['view']}" for r in rows if not r["pass"]]
    raise RuntimeError("asset view audit failed: " + ", ".join(failed))

with (output / "summary.csv").open("w", newline="", encoding="utf-8-sig") as handle:
    writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
    writer.writeheader()
    writer.writerows(rows)

(output / "audit.json").write_text(
    json.dumps({
        "schema": 1,
        "result": "PASS",
        "caseCount": len(rows),
        "evidenceClass": "real RK3588 H264 DDS receiver decode",
        "interpretation": "synthetic normal diagnostic for geometry, pose and display only",
        "cases": rows,
    }, indent=2),
    encoding="utf-8",
)

font = ImageFont.load_default()
tile_w, tile_h, label_h = 320, 320, 34
sheet = Image.new("RGB", (tile_w * 4, (tile_h + label_h) * 4), "white")
draw = ImageDraw.Draw(sheet)
for index, row in enumerate(rows):
    source = Image.open(output / row["receivedImage"]).convert("RGB")
    source.thumbnail((tile_w, tile_h), Image.Resampling.LANCZOS)
    x = (index % 4) * tile_w
    y = (index // 4) * (tile_h + label_h)
    paste_x = x + (tile_w - source.width) // 2
    paste_y = y + label_h + (tile_h - source.height) // 2
    sheet.paste(source, (paste_x, paste_y))
    draw.text((x + 6, y + 8), f"{row['asset']} {row['band']} {row['view']}", fill="black", font=font)
sheet.save(output / "contact_sheet.png")

cards = []
for row in rows:
    label = f"{row['asset']} {row['band']} {row['view']}"
    cards.append(
        '<figure><img src="{}" alt="{}"><figcaption>{}<br>'
        'decoded={} max={} std={:.2f}<br><code>{}</code></figcaption></figure>'.format(
            html.escape(row["receivedImage"]), html.escape(label), html.escape(label),
            row["decodedFrames"], row["max"], row["stddev"], row["receivedSha256"]))
document = """<!doctype html><html lang="zh-CN"><head><meta charset="utf-8">
<title>HwaSimIR P12 asset view evidence</title><style>
body{font:14px system-ui;margin:24px;background:#181818;color:#eee}h1{font-size:22px}
.notice{max-width:1050px;padding:12px;background:#302a18;border:1px solid #806d2d}
.grid{display:grid;grid-template-columns:repeat(4,minmax(220px,1fr));gap:12px;margin-top:18px}
figure{margin:0;padding:10px;background:#252525;border:1px solid #444}img{width:100%;height:auto;background:#000}
figcaption{line-height:1.45;margin-top:7px}code{font-size:10px;word-break:break-all}
</style></head><body><h1>HwaSimIR P12 资产几何/位姿/显示回归</h1>
<p class="notice">PASS：16/16。图片来自 RK3588 H264 DDS，在 Windows 显示端真实解码后导出。
MaterialView=2 是合成法线诊断，只用于通用模型几何、位姿和显示链；不代表具体装备的真实材料、温度或辐射特征。</p>
<div class="grid">""" + "\n".join(cards) + """</div></body></html>"""
(output / "index.html").write_text(document, encoding="utf-8")
print(json.dumps({"result": "PASS", "cases": len(rows), "output": str(output)}, ensure_ascii=False))
