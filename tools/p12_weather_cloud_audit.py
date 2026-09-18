#!/usr/bin/env python3
"""Build P12 cloud/precipitation evidence from real RK3588 DDS runs.

The source ``received.png`` files are emitted by the Windows DDS receiver.  The
script never substitutes the local/raw diagnostic image for a received image.
"""

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
OUTPUT = ROOT / "logs" / "p12" / "p12c" / "weather_cloud"

CASES = [
    ("cloud_mwir_sample1000", "p12_cloud_mwir_plate1000_final", "cloud", "MWIR", "cloud_behind_sample"),
    ("cloud_mwir_sample6500", "p12_cloud_mwir_plate6500_final", "cloud", "MWIR", "cloud_in_front_of_sample"),
    ("cloud_swir_sample1000", "p12_cloud_swir_plate1000_final", "cloud", "SWIR", "cloud_behind_sample"),
    ("cloud_swir_sample6500", "p12_cloud_swir_plate6500_final", "cloud", "SWIR", "cloud_in_front_of_sample"),
    ("cloud_mwir_hide_D73EEA5ECB3EA0B8", "p12_cloud_mwir_hide_D73EEA5ECB3EA0B8", "cloud_hide", "MWIR", "cloud_in_front_of_sample"),
    ("cloud_mwir_hide_198EE8338358C182", "p12_cloud_mwir_hide_198EE8338358C182", "cloud_hide", "MWIR", "cloud_in_front_of_sample"),
    ("rain_mwir_on", "p12_rain_mwir_batch_on", "rain_on", "MWIR", None),
    ("rain_mwir_off", "p12_rain_mwir_batch_off", "rain_off", "MWIR", None),
    ("snow_mwir_on", "p12_snow_mwir_batch_on", "snow_on", "MWIR", None),
    ("snow_mwir_off", "p12_snow_mwir_batch_off", "snow_off", "MWIR", None),
    ("rain_swir_on", "p12_rain_swir_batch_on", "rain_on", "SWIR", None),
    ("snow_swir_on", "p12_snow_swir_batch_on", "snow_on", "SWIR", None),
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_image(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.int16)


def image_stats(path: Path) -> dict:
    array = load_image(path)
    gray = np.rint(array.mean(axis=2)).astype(np.uint8)
    return {
        "width": int(array.shape[1]),
        "height": int(array.shape[0]),
        "minimum": int(gray.min()),
        "maximum": int(gray.max()),
        "mean": float(gray.mean()),
        "stddev": float(gray.std()),
        "dynamicRange": int(gray.max()) - int(gray.min()),
    }


def compare_images(left: Path, right: Path, output: Path) -> dict:
    a = load_image(left)
    b = load_image(right)
    if a.shape != b.shape:
        raise ValueError(f"image shape mismatch: {left} {a.shape} != {right} {b.shape}")
    delta = np.abs(a - b)
    gray = delta.max(axis=2)
    changed = gray > 1
    # A fixed 8x gain makes low-contrast cloud/precipitation changes inspectable
    # without changing the source evidence.
    heat = np.clip(gray * 8, 0, 255).astype(np.uint8)
    Image.fromarray(heat, mode="L").save(output)
    return {
        "changedPixelsGt1": int(changed.sum()),
        "changedFractionGt1": float(changed.mean()),
        "meanAbsChannel": float(delta.mean()),
        "maxAbsChannel": int(delta.max()),
        "diffImage": output.name,
    }


def last_runtime_metrics(text: str) -> dict:
    matches = re.findall(r"^\[RuntimeMetricsV2\]\s+(\{.*\})$", text, re.MULTILINE)
    return json.loads(matches[-1]) if matches else {}


def cloud_audits(text: str) -> list[dict]:
    found: dict[str, dict] = {}
    for raw in re.findall(r"^\[CloudLineOfSightAudit\]\s+(\{.*\})$", text, re.MULTILINE):
        item = json.loads(raw)
        found[item["cloudId"]] = item
    return [found[key] for key in sorted(found)]


def make_contact_sheet(rows: list[dict], output: Path) -> None:
    tile_w, tile_h, caption_h = 320, 320, 58
    columns = 4
    count_rows = (len(rows) + columns - 1) // columns
    canvas = Image.new("RGB", (columns * tile_w, count_rows * (tile_h + caption_h)), "#101318")
    draw = ImageDraw.Draw(canvas)
    font = ImageFont.load_default()
    for index, row in enumerate(rows):
        x = (index % columns) * tile_w
        y = (index // columns) * (tile_h + caption_h)
        image = Image.open(OUTPUT / row["receivedCopy"]).convert("RGB")
        image.thumbnail((tile_w, tile_h))
        canvas.paste(image, (x + (tile_w - image.width) // 2, y + (tile_h - image.height) // 2))
        caption = f"{row['label']}\nDDS decoded {row['metrics'].get('decodedFrames', '?')}; reject {row['metrics'].get('statusIdentityRejected', '?')}"
        draw.multiline_text((x + 6, y + tile_h + 5), caption, fill="white", font=font, spacing=3)
    canvas.save(output)


def main() -> int:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    rows: list[dict] = []
    failures: list[str] = []

    for label, directory_name, kind, band, expected_relation in CASES:
        directory = SOURCE / directory_name
        required = [directory / "received.png", directory / "linear_rgb8.png", directory / "linear.pfm", directory / "board.log", directory / "video.err.log"]
        missing = [str(path) for path in required if not path.is_file()]
        if missing:
            failures.append(f"{label}: missing {missing}")
            continue

        board = (directory / "board.log").read_text(encoding="utf-8", errors="replace")
        video = (directory / "video.err.log").read_text(encoding="utf-8", errors="replace")
        metrics = last_runtime_metrics(video)
        audits = cloud_audits(board)
        checks = {
            "ddsDecoded": int(metrics.get("decodedFrames", 0)) > 0,
            "ddsErrorsZero": int(metrics.get("ddsErrors", -1)) == 0 and int(metrics.get("decodeErrors", -1)) == 0,
            "identityRejectedZero": int(metrics.get("statusIdentityRejected", -1)) == 0,
            "releaseHashRecorded": (directory / "release.sha256").is_file(),
        }
        if kind.startswith("cloud"):
            checks["twoVisibleCloudAudits"] = len(audits) == 2
            checks["centralRayIntersects"] = len(audits) == 2 and all(item.get("centralRayIntersects") for item in audits)
            checks["relation"] = len(audits) == 2 and all(item.get("relation") == expected_relation for item in audits)
            if kind == "cloud_hide":
                checks["oneCloudHidden"] = sum(bool(item.get("hiddenByAudit")) for item in audits) == 1
        elif kind.endswith("_on"):
            weather = kind.split("_")[0]
            checks["batchAllocated"] = "[PrecipitationBatch] particles=128 draws=1" in board
            checks["batchFrameRendered"] = bool(re.search(rf"\[PrecipitationFrame\].*type={weather}.*batchCount=1", board))
            checks["overlayInactive"] = bool(re.search(rf"\[Stage7 PrecipitationOverlay\] mode=Batch active=0 type={weather}", board))
        elif kind.endswith("_off"):
            checks["precipitationDisabled"] = "[Stage7 Precipitation] enabled=0 type=none" in board
            checks["overlayInactive"] = "[Stage7 PrecipitationOverlay] mode=Batch active=0 type=none" in board

        received_copy = f"{label}_received_dds.png"
        linear_copy = f"{label}_linear_rgb8.png"
        shutil.copy2(directory / "received.png", OUTPUT / received_copy)
        shutil.copy2(directory / "linear_rgb8.png", OUTPUT / linear_copy)
        row = {
            "label": label,
            "sourceCase": directory_name,
            "kind": kind,
            "band": band,
            "result": "PASS" if all(checks.values()) else "FAIL",
            "checks": checks,
            "metrics": metrics,
            "cloudAudits": audits,
            "receivedCopy": received_copy,
            "linearCopy": linear_copy,
            "receivedStats": image_stats(directory / "received.png"),
            "linearStats": image_stats(directory / "linear_rgb8.png"),
            "sha256": {
                "received": sha256(directory / "received.png"),
                "linearRgb8": sha256(directory / "linear_rgb8.png"),
                "linearPfm": sha256(directory / "linear.pfm"),
                "boardLog": sha256(directory / "board.log"),
            },
            "provenance": {
                "received": str((directory / "received.png").relative_to(ROOT)).replace("\\", "/"),
                "linearRgb8": str((directory / "linear_rgb8.png").relative_to(ROOT)).replace("\\", "/"),
                "linearPfm": str((directory / "linear.pfm").relative_to(ROOT)).replace("\\", "/"),
                "meaning": "received is a Windows DDS decoder dump; linear artifacts are RK3588 diagnostics",
            },
        }
        if row["result"] != "PASS":
            failures.append(f"{label}: failed checks {[key for key, value in checks.items() if not value]}")
        rows.append(row)

    by_label = {row["label"]: row for row in rows}
    comparisons: list[dict] = []
    comparison_specs = [
        ("hide_D73EEA5ECB3EA0B8", "cloud_mwir_sample6500", "cloud_mwir_hide_D73EEA5ECB3EA0B8"),
        ("hide_198EE8338358C182", "cloud_mwir_sample6500", "cloud_mwir_hide_198EE8338358C182"),
        ("rain_mwir_on_minus_off", "rain_mwir_off", "rain_mwir_on"),
        ("snow_mwir_on_minus_off", "snow_mwir_off", "snow_mwir_on"),
        ("mwir_sample6500_minus_1000", "cloud_mwir_sample1000", "cloud_mwir_sample6500"),
        ("swir_sample6500_minus_1000", "cloud_swir_sample1000", "cloud_swir_sample6500"),
    ]
    for label, left_label, right_label in comparison_specs:
        if left_label not in by_label or right_label not in by_label:
            failures.append(f"comparison {label}: source case missing")
            continue
        entry = {"label": label, "left": left_label, "right": right_label}
        received_diff = OUTPUT / f"diff_{label}_received_x8.png"
        linear_diff = OUTPUT / f"diff_{label}_linear_x8.png"
        entry["received"] = compare_images(
            OUTPUT / by_label[left_label]["receivedCopy"], OUTPUT / by_label[right_label]["receivedCopy"], received_diff
        )
        entry["linear"] = compare_images(
            OUTPUT / by_label[left_label]["linearCopy"], OUTPUT / by_label[right_label]["linearCopy"], linear_diff
        )
        entry["result"] = "PASS" if entry["received"]["changedPixelsGt1"] > 100 and entry["linear"]["changedPixelsGt1"] > 100 else "FAIL"
        if entry["result"] != "PASS":
            failures.append(f"comparison {label}: insufficient actual pixel change")
        comparisons.append(entry)

    result = "PASS" if not failures and len(rows) == len(CASES) else "FAIL"
    report = {
        "schema": "HwaSimIR.P12.WeatherCloudAudit.1",
        "result": result,
        "caseCount": len(rows),
        "failureCount": len(failures),
        "failures": failures,
        "importantLimit": "Engineering rendering evidence only; NOT_VERIFIED_CALIBRATION.",
        "cases": rows,
        "comparisons": comparisons,
    }
    (OUTPUT / "audit.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

    with (OUTPUT / "audit.csv").open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.writer(stream)
        writer.writerow(["label", "kind", "band", "result", "decoded", "identityRejected", "receivedSha256", "linearPfmSha256"])
        for row in rows:
            writer.writerow([
                row["label"], row["kind"], row["band"], row["result"], row["metrics"].get("decodedFrames"),
                row["metrics"].get("statusIdentityRejected"), row["sha256"]["received"], row["sha256"]["linearPfm"],
            ])

    make_contact_sheet(rows, OUTPUT / "contact_sheet_received_dds.png")
    table_rows = []
    for row in rows:
        table_rows.append(
            "<tr>"
            f"<td>{html.escape(row['label'])}</td><td>{row['band']}</td><td>{row['result']}</td>"
            f"<td>{html.escape(str(row['metrics'].get('decodedFrames', '')))}</td>"
            f"<td><a href='{row['receivedCopy']}'><img src='{row['receivedCopy']}'></a></td>"
            f"<td><a href='{row['linearCopy']}'><img src='{row['linearCopy']}'></a></td>"
            "</tr>"
        )
    comparison_rows = []
    for item in comparisons:
        comparison_rows.append(
            "<tr>"
            f"<td>{html.escape(item['label'])}</td><td>{item['result']}</td>"
            f"<td>{item['received']['changedPixelsGt1']}</td><td>{item['linear']['changedPixelsGt1']}</td>"
            f"<td><img src='{item['received']['diffImage']}'></td><td><img src='{item['linear']['diffImage']}'></td>"
            "</tr>"
        )
    document = f"""<!doctype html><meta charset='utf-8'><title>P12 weather/cloud evidence</title>
<style>body{{font:14px system-ui;background:#111;color:#eee;margin:24px}}table{{border-collapse:collapse;width:100%}}th,td{{border:1px solid #555;padding:6px;vertical-align:top}}img{{width:240px;height:240px;object-fit:contain;background:#000}}code{{color:#9ef}}</style>
<h1>P12 weather / cloud evidence — {result}</h1>
<p><b>received</b> images below are actual Windows DDS decoder dumps. Linear images/PFM hashes are separate RK3588 diagnostics and are never substituted for received evidence.</p>
<p><code>NOT_VERIFIED_CALIBRATION</code>: these are controlled engineering effects, not real material or sensor calibration claims.</p>
<p><a href='audit.json'>JSON</a> · <a href='audit.csv'>CSV</a> · <a href='contact_sheet_received_dds.png'>DDS contact sheet</a></p>
<h2>Cases</h2><table><tr><th>case</th><th>band</th><th>result</th><th>DDS decoded</th><th>received DDS</th><th>linear diagnostic</th></tr>{''.join(table_rows)}</table>
<h2>Single-factor / per-cloud pixel differences</h2><table><tr><th>comparison</th><th>result</th><th>received changed pixels</th><th>linear changed pixels</th><th>received diff ×8</th><th>linear diff ×8</th></tr>{''.join(comparison_rows)}</table>
"""
    (OUTPUT / "index.html").write_text(document, encoding="utf-8")
    print(json.dumps({"result": result, "cases": len(rows), "comparisons": comparisons, "failures": failures}, indent=2))
    return 0 if result == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
