#!/usr/bin/env python3
"""Build P14 evidence contact sheets from final-version captured pixels."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "logs" / "p14" / "images_final"
OUT.mkdir(parents=True, exist_ok=True)
FONT = ImageFont.load_default()


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def find_one(pattern: str) -> Path:
    matches = list(ROOT.glob(pattern))
    if len(matches) != 1:
        raise RuntimeError(f"expected one match for {pattern!r}, got {len(matches)}")
    return matches[0]


def sheet(name: str, title: str, cells: list[tuple[str, Path]], columns: int,
          cell_size: tuple[int, int] = (420, 420), footer: str = "") -> dict:
    for _, path in cells:
        if not path.is_file():
            raise FileNotFoundError(path)
    label_h = 32
    title_h = 44
    footer_h = 36 if footer else 0
    rows = (len(cells) + columns - 1) // columns
    width = columns * cell_size[0]
    height = title_h + rows * (cell_size[1] + label_h) + footer_h
    canvas = Image.new("RGB", (width, height), (17, 22, 28))
    draw = ImageDraw.Draw(canvas)
    draw.text((12, 14), title, fill=(235, 240, 245), font=FONT)
    for index, (label, path) in enumerate(cells):
        row, column = divmod(index, columns)
        x = column * cell_size[0]
        y = title_h + row * (cell_size[1] + label_h)
        with Image.open(path) as image:
            frame = image.convert("RGB")
            frame.thumbnail(cell_size, Image.Resampling.LANCZOS)
            px = x + (cell_size[0] - frame.width) // 2
            py = y + (cell_size[1] - frame.height) // 2
            canvas.paste(frame, (px, py))
        draw.rectangle((x, y + cell_size[1], x + cell_size[0], y + cell_size[1] + label_h),
                       fill=(28, 36, 45))
        draw.text((x + 10, y + cell_size[1] + 10), label, fill=(210, 225, 235), font=FONT)
    if footer:
        draw.text((12, height - footer_h + 12), footer, fill=(245, 205, 105), font=FONT)
    output = OUT / name
    canvas.save(output, optimize=True)
    return {
        "path": str(output.relative_to(ROOT)).replace("\\", "/"),
        "sha256": sha256(output),
        "sources": [
            {"label": label, "path": str(path.relative_to(ROOT)).replace("\\", "/"), "sha256": sha256(path)}
            for label, path in cells
        ],
    }


def main() -> int:
    weather_cases = {
        ("SWIR", "Clear"): "p14_final4_ground_swir_clear",
        ("SWIR", "Cloud / visibility 12 km"): "p14_final4c_ground_swir_cloud_visibility",
        ("SWIR", "Rain"): "p14_final4_ground_swir_rain",
        ("SWIR", "Snow"): "p14_final4_ground_swir_snow",
        ("MWIR", "Clear"): "p14_final4_ground_mwir_clear",
        ("MWIR", "Cloud / visibility 12 km"): "p14_final4_ground_mwir_cloud_visibility",
        ("MWIR", "Rain"): "p14_final4_ground_mwir_rain",
        ("MWIR", "Snow"): "p14_final4_ground_mwir_snow",
    }
    weather_received: list[tuple[str, Path]] = []
    weather_fixed: list[tuple[str, Path]] = []
    for (band, weather), case in weather_cases.items():
        case_root = ROOT / "logs" / "p14" / "runs" / case
        weather_received.append((f"{band} | {weather} | DDS decoded", case_root / "keyframes" / "middle.png"))
        weather_fixed.append((f"{band} | {weather} | fixed raw mapping",
                              find_one(f"logs/p14/runs/{case}/diagnostic_linear/*_rgb8.png")))

    products: dict[str, dict] = {}
    products["weather_received"] = sheet(
        "weather_received_2band.png",
        "P14 final ELF - civil ground target - received DDS H.264 frames",
        weather_received, 4,
        footer="Captured from final same-version Clear / Cloud+12km / Rain / Snow runs; NOT calibration.",
    )
    products["weather_fixed"] = sheet(
        "weather_fixed_mapping_2band.png",
        "P14 final ELF - raw SI keyframes under one fixed display mapping",
        weather_fixed, 4,
        footer="Fixed mapping is diagnostic evidence; ordinary videos use the normal display chain.",
    )

    original_cells: list[tuple[str, Path]] = []
    for band, case in (
        ("SWIR", "p14_final4_original_1_swir_clear"),
        ("MWIR", "p14_final4_original_1_mwir_clear"),
    ):
        case_root = ROOT / "logs" / "p14" / "runs" / case / "keyframes"
        original_cells.extend([
            (f"{band} | early FollowInput | ViewValid=0", case_root / "start.png"),
            (f"{band} | later FollowInput | ViewValid=1", case_root / "middle.png"),
        ])
    products["original_visibility"] = sheet(
        "original_1_visibility_transition.png",
        "Immutable DataDrivenTestQT/1.txt - FollowInput presentation",
        original_cells, 2,
        footer="Rows 1-659 remain accepted geometry/background products with the target hidden by the source flag.",
    )

    boundary_cells: list[tuple[str, Path]] = []
    for band, case in (
        ("SWIR", "p14_final5_first_valid_swir_r2"),
        ("MWIR", "p14_final5_first_valid_mwir"),
    ):
        case_root = ROOT / "logs" / "p14" / "runs" / case
        boundary_cells.extend([
            (f"{band} | first valid | fixed raw mapping", find_one(f"logs/p14/runs/{case}/diagnostic_linear/*_rgb8.png")),
            (f"{band} | first valid | received DDS decode", case_root / "received_keyframe.png"),
        ])
    products["first_valid"] = sheet(
        "first_valid_boundary_2band.png",
        "Independent external DDS fixture - no packet -> zero placeholder -> first valid",
        boundary_cells, 2,
        footer="Explicit test-side ForceVisibleForDemo; production filtering unchanged; 181/181 products per band.",
    )

    plume_cells = [
        ("P13 historical display diagnostic (not signed-radiance proof)",
         ROOT / "logs" / "p13" / "delivery_final" / "images" / "plume_on.png"),
        ("P14 heat source OFF | final fixed raw mapping",
         find_one("logs/p14/runs/p14_final4_plume_truck_mwir_off_raw/diagnostic_linear/*_rgb8.png")),
        ("P14 heat source ON | final fixed raw mapping",
         find_one("logs/p14/runs/p14_final4_plume_truck_mwir_on_raw/diagnostic_linear/*_rgb8.png")),
        ("P14 signed raw ON-OFF | red is positive",
         ROOT / "logs" / "p14" / "plume_signed_raw_qc_final4" / "on_minus_off_signed_raw.png"),
    ]
    products["plume"] = sheet(
        "plume_root_cause_and_signed_raw.png",
        "Actual priority=100 sprite path - historical symptom and final controlled generic heat source",
        plume_cells, 2,
        footer="Final ROI ON-OFF mean +0.067446793 W/(m2 sr um); positive pixels 728184; negative pixels 0.",
    )

    ui_cells = [
        ("Actual Qt click: Civil truck 0x55 + demo visibility selected",
         ROOT / "logs" / "p14" / "ui" / "target_type_click_selected.png"),
        ("INIT click: target and visibility controls frozen",
         ROOT / "logs" / "p14" / "ui" / "target_type_click_init_frozen.png"),
    ]
    products["target_ui"] = sheet(
        "target_type_ui_click.png",
        "DataDrivenTestQT production widget exercised through Qt mouse/key events",
        ui_cells, 2, cell_size=(640, 400),
        footer="Combo userData carries the protocol code; RESET re-enables editing.",
    )

    manifest = {
        "schema": "HwaSimIR.P14.ImageManifest.1",
        "result": "PASS",
        "generatedFromCapturedEvidence": True,
        "syntheticImageGeneration": False,
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
        "products": products,
    }
    manifest_path = OUT / "image_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"result": "PASS", "manifest": str(manifest_path), "products": len(products)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
