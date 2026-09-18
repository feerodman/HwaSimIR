#!/usr/bin/env python3
"""Build the auditable P12 runtime/evidence delivery without credentials or licenses."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import os
import re
import shutil
import sys
import time
import zipfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Callable, Iterable


ROOT = Path(__file__).resolve().parents[1]
DELIVERY_DIR = ROOT / "deliverables" / "HwaSimIR_P12"
ARCHIVE = DELIVERY_DIR / "HwaSimIR_P12_Delivery.zip"
ARCHIVE_ROOT = PurePosixPath("HwaSimIR_P12_Delivery")


@dataclass(frozen=True)
class Item:
    source: Path
    archive_path: PurePosixPath
    category: str


ITEMS: dict[str, Item] = {}


FORBIDDEN_SUFFIXES = {
    ".lic",
    ".pdb",
    ".iobj",
    ".ipdb",
    ".o",
    ".obj",
    ".ddslog",
}
FORBIDDEN_NAMES = {
    "license.txt",
    "zrddslicence.lic",
    "zrddslicense.lic",
    "networkconfig - 副本.ini",
}
TEXT_SUFFIXES = {
    ".txt",
    ".md",
    ".csv",
    ".json",
    ".jsonl",
    ".ini",
    ".xml",
    ".log",
    ".env",
    ".ps1",
    ".sh",
    ".py",
    ".cpp",
    ".h",
    ".pro",
}
STORED_SUFFIXES = {".png", ".jpg", ".jpeg", ".gif", ".mp4", ".h264", ".zip", ".gz", ".7z"}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def resolved_inside_workspace(path: Path) -> Path:
    resolved = path.resolve()
    try:
        resolved.relative_to(ROOT.resolve())
    except ValueError as exc:
        raise RuntimeError(f"Refusing path outside workspace: {resolved}") from exc
    return resolved


def globally_excluded(path: Path) -> bool:
    lower_name = path.name.lower()
    if lower_name in FORBIDDEN_NAMES or path.suffix.lower() in FORBIDDEN_SUFFIXES:
        return True
    if lower_name.endswith(".before") or ".before_" in lower_name:
        return True
    return False


def add_file(source: Path | str, archive_path: PurePosixPath | str, category: str) -> None:
    source_path = resolved_inside_workspace(ROOT / source if not isinstance(source, Path) else source)
    if not source_path.is_file():
        raise FileNotFoundError(source_path)
    if globally_excluded(source_path):
        return
    rel = PurePosixPath(str(archive_path).replace("\\", "/"))
    key = rel.as_posix()
    item = Item(source_path, rel, category)
    prior = ITEMS.get(key)
    if prior and prior.source != source_path:
        raise RuntimeError(f"Duplicate archive member {key}: {prior.source} vs {source_path}")
    ITEMS[key] = item


def add_tree(
    source_root: Path | str,
    archive_root: PurePosixPath | str,
    category: str,
    include: Callable[[Path, Path], bool] | None = None,
) -> None:
    root = resolved_inside_workspace(ROOT / source_root if not isinstance(source_root, Path) else source_root)
    if not root.is_dir():
        raise FileNotFoundError(root)
    arc_root = PurePosixPath(str(archive_root).replace("\\", "/"))
    for path in sorted(root.rglob("*"), key=lambda p: p.as_posix().lower()):
        if not path.is_file() or globally_excluded(path):
            continue
        rel = path.relative_to(root)
        if include is not None and not include(path, rel):
            continue
        add_file(path, arc_root / PurePosixPath(rel.as_posix()), category)


def runtime_config_filter(path: Path, rel: Path) -> bool:
    parts = [part.lower() for part in rel.parts]
    if "modtran" not in parts:
        return True
    index = parts.index("modtran")
    tail = parts[index + 1 :]
    if not tail:
        return False
    if tail[0] != "processed":
        return False
    return path.name.lower() in {
        "band_lut_si.csv",
        "band_lut.csv",
        "solar_heating_lut_si.csv",
    }


def ordinary_evidence_filter(path: Path, rel: Path) -> bool:
    lower_parts = [part.lower() for part in rel.parts]
    if "recording" in lower_parts:
        return False
    if path.suffix.lower() in {".mp4", ".h264"}:
        return False
    return True


def modtran_evidence_filter(path: Path, rel: Path) -> bool:
    # Keep the execution/license *evidence* JSON, but never redistribute an engine or license file.
    if path.suffix.lower() in {".exe", ".dll", ".lic"}:
        return False
    return True


def add_windows_runtime() -> None:
    data_root = ROOT / "build-DataDrivenTestQT-codex-mingw73_64-Release" / "release"
    for directory in ("bearer", "Config", "iconengines", "imageformats", "platforms", "styles", "translations"):
        add_tree(data_root / directory, PurePosixPath("runtime/DataDrivenTestQT") / directory, "runtime")
    for name in (
        "DataDrivenTestQT.exe",
        "D3Dcompiler_47.dll",
        "libEGL.dll",
        "libgcc_s_seh-1.dll",
        "libGLESV2.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll",
        "NetworkConfig.ini",
        "opengl32sw.dll",
        "ordinary_demo_1km.txt",
        "ordinary_demo_1km.txt.json",
        "Qt5Core.dll",
        "Qt5Gui.dll",
        "Qt5Network.dll",
        "Qt5Svg.dll",
        "Qt5Widgets.dll",
        "ZRDDSCpp.dll",
    ):
        add_file(data_root / name, PurePosixPath("runtime/DataDrivenTestQT") / name, "runtime")

    hwa_root = ROOT / "HwaSim_IR" / "Bin"
    for path in sorted(hwa_root.iterdir(), key=lambda p: p.name.lower()):
        if path.is_file() and not globally_excluded(path):
            if path.suffix.lower() in {".exe", ".dll"}:
                add_file(path, PurePosixPath("runtime/HwaSim_IR_Windows") / path.name, "runtime")
    add_tree(hwa_root / "Panda3D-1.10", "runtime/HwaSim_IR_Windows/Panda3D-1.10", "runtime")
    add_tree(hwa_root / "Config", "runtime/HwaSim_IR_Windows/Config", "runtime", runtime_config_filter)

    video_root = ROOT / "HwaSim_IR_VideoDisplay" / "x64" / "Release"
    for path in sorted(video_root.iterdir(), key=lambda p: p.name.lower()):
        if path.is_file() and not globally_excluded(path):
            if path.name == "NetworkConfig.ini" or path.suffix.lower() in {".exe", ".dll"}:
                add_file(path, PurePosixPath("runtime/HwaSim_IR_VideoDisplay") / path.name, "runtime")
    for directory in ("bearer", "Config", "iconengines", "imageformats", "platforms", "qss", "styles", "translations"):
        add_tree(video_root / directory, PurePosixPath("runtime/HwaSim_IR_VideoDisplay") / directory, "runtime")


def add_rk_runtime() -> None:
    add_file(
        "logs/p12/p12d/build/scenegraph-prewarm2-20260918-042654/HwaSim_IR.rk3588",
        "runtime/RK3588/HwaSim_IR",
        "runtime",
    )
    add_file("tools/rk3588_run_hwasimir_precise.sh", "runtime/RK3588/run_precise.sh", "runtime")
    add_file("tools/rk3588_hwasimir_performance_mode.sh", "runtime/RK3588/rk3588_hwasimir_performance_mode.sh", "runtime")


def add_sources_and_tools() -> None:
    source_files = [
        "DataDrivenTestQT/DataDrivenTestQT.pro",
        "DataDrivenTestQT/NetworkConfig.ini",
        "DataDrivenTestQT/main.cpp",
        "DataDrivenTestQT/mainwindow.cpp",
        "DataDrivenTestQT/mainwindow.h",
        "DataDrivenTestQT/ordinary_demo_1km.txt",
        "DataDrivenTestQT/ordinary_demo_1km.txt.json",
        "HwaSim_IR/HwaSim_IR/Annotation/AnnotationConfig.cpp",
        "HwaSim_IR/HwaSim_IR/Annotation/AnnotationManager.cpp",
        "HwaSim_IR/HwaSim_IR/Annotation/AnnotationManager.h",
        "HwaSim_IR/HwaSim_IR/Annotation/AnnotationProjector.cpp",
        "HwaSim_IR/HwaSim_IR/Annotation/AnnotationProjector.h",
        "HwaSim_IR/HwaSim_IR/HwaSimIR.cpp",
        "HwaSim_IR/HwaSim_IR/HwaSim_IR.vcxproj",
        "HwaSim_IR/HwaSim_IR/IR/IRLinearReadback.h",
        "HwaSim_IR/HwaSim_IR/IR/IRSceneMaterialMapper.cpp",
        "HwaSim_IR/HwaSim_IR/IR/P12DiagnosticWriter.h",
        "HwaSim_IR/HwaSim_IR/IR/P5GraphicsTest.inl",
        "HwaSim_IR/HwaSim_IR/IR/P6GraphicsTest.inl",
        "HwaSim_IR/HwaSim_IR/IR/P6GraphicsTest.inl",
        "HwaSim_IR/HwaSim_IR/IR/P7GraphicsTest.inl",
        "HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/DdsVideoReceiverWorker.cpp",
        "HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/DdsVideoReceiverWorker.h",
        "HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.cpp",
        "HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.h",
    ]
    # P7 is named separately in this checkout; ignore only this optional typo-safe candidate.
    for source in source_files:
        path = ROOT / source
        if path.is_file():
            add_file(path, PurePosixPath("source") / PurePosixPath(source), "source")

    for source in (
        "HwaSim_IR/Bin/Config/Annotation/annotation_profiles.json",
        "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv",
        "HwaSim_IR/Bin/Config/TargetLib/Targets.json",
    ):
        add_file(source, PurePosixPath("source") / PurePosixPath(source), "source")
    add_tree("HwaSim_IR/Bin/Config/TargetLib/p12", "source/HwaSim_IR/Bin/Config/TargetLib/p12", "source")

    tool_names = [
        "p11_civil_target_integration_check.ps1",
        "p11_controlled_samples_integration_check.ps1",
        "p5_asset_audit.py",
        "p5_run_case.ps1",
        "p6d_run_case.ps1",
        "p7_run_case.ps1",
        "rk3588_run_hwasimir_precise.sh",
        "stage0_build.ps1",
    ]
    tools_root = ROOT / "tools"
    for path in sorted(tools_root.glob("p12*"), key=lambda p: p.name.lower()):
        if path.is_file():
            add_file(path, PurePosixPath("tools") / path.name, "tool")
        elif path.is_dir():
            add_tree(path, PurePosixPath("tools") / path.name, "tool")
    for name in tool_names:
        add_file(tools_root / name, PurePosixPath("tools") / name, "tool")


def add_docs() -> None:
    docs = [
        "HwaSimIR_P12_P11_Audit.md",
        "HwaSimIR_P12_Execution_State.md",
        "HwaSimIR_P12_Issue_Ledger.csv",
        "HwaSimIR_P12_ManualStartup_And_NoImage_Fix.md",
        "HwaSimIR_P12_Imaging_Regression_And_Remaining.md",
        "HwaSimIR_P12_Closeout.md",
        "HwaSimIR_P12_Delivery_Readme.md",
    ]
    for name in docs:
        add_file(ROOT / "docs" / name, PurePosixPath("docs") / name, "document")
    add_file(ROOT / "docs" / "HwaSimIR_P12_Delivery_Readme.md", "README.md", "document")


def add_evidence() -> None:
    selected = [
        ("logs/p12/p12a/ordinary-mixed-syncgate-20260917-204034", "evidence/p12a/ordinary-mixed-syncgate-20260917-204034"),
        ("logs/p12/p12a/ordinary-mixed-ui-20260917-204319", "evidence/p12a/ordinary-mixed-ui-20260917-204319"),
        ("logs/p12/p12a/mixed-late-restart-20260917-204516", "evidence/p12a/mixed-late-restart-20260917-204516"),
        ("logs/p12/p12a/mixed-input-pause-20260917-204738", "evidence/p12a/mixed-input-pause-20260917-204738"),
        ("logs/p12/p12a/ordinary-mwir-final-20260917-210454", "evidence/p12a/ordinary-mwir-final-20260917-210454"),
        ("logs/p12/p12a/nir-compat-rk-20260917-210308", "evidence/p12a/nir-compat-rk-20260917-210308"),
        ("logs/p12/p12b/asset_views", "evidence/p12b/asset_views"),
        ("logs/p12/p12b/assets", "evidence/p12b/assets"),
        ("logs/p12/p12b/active_illumination_unit", "evidence/p12b/active_illumination_unit"),
        ("logs/p12/p12b/aero_locality_unit", "evidence/p12b/aero_locality_unit"),
        ("logs/p12/p12b/thermal_inertia_unit", "evidence/p12b/thermal_inertia_unit"),
        ("logs/p12/p12b/civil_integration", "evidence/p12b/civil_integration"),
        ("logs/p12/p12b/controlled_samples_integration", "evidence/p12b/controlled_samples_integration"),
        ("logs/p12/p12b/ordinary_assets/f22_mwir_annotation_final", "evidence/p12b/f22_mwir_annotation_final"),
        ("logs/p12/p12c/weather_cloud", "evidence/p12c/weather_cloud"),
        ("logs/p12/p12c/controlled_glass/evidence", "evidence/p12c/controlled_glass"),
        ("logs/p12/p12c/manual_init_coverage", "evidence/p12c/manual_init_coverage"),
        ("logs/p12/p12d/deployment/p12-elf-20260918-042702", "evidence/p12d/deployment/p12-elf-20260918-042702"),
        ("logs/p12/p12d/deployment/p12-20260918-042901", "evidence/p12d/deployment/p12-20260918-042901"),
        ("logs/p12/p12d/performance/ordinary-performance-20260918-043110", "evidence/p12d/performance/ordinary-performance-20260918-043110"),
        ("logs/p12/p12d/performance/ordinary-performance-20260918-020639/MWIR_300s_Snow", "evidence/p12d/performance/MWIR_300s_Snow"),
        ("logs/p12/p12d/performance/p12d-perf-ab-20260918-012607", "evidence/p12d/performance/pfm_sync"),
        ("logs/p12/p12d/performance/p12d-perf-ab-20260918-014219", "evidence/p12d/performance/pfm_async"),
        ("logs/p12/p12d/lifecycle/ordinary-lifecycle-20260918-043854", "evidence/p12d/lifecycle"),
        ("logs/p12/p12d/rollback/p12rb-20260918-045030", "evidence/p12d/rollback/failed_attempt_retained"),
        ("logs/p12/p12d/rollback/p12rb-20260918-045424", "evidence/p12d/rollback/pass"),
    ]
    for source, archive in selected:
        add_tree(source, archive, "evidence", ordinary_evidence_filter)
    add_tree("logs/p12/p12c/modtran_2km", "evidence/p12c/modtran_2km", "evidence", modtran_evidence_filter)


def final_status_payload() -> dict:
    return {
        "schema": "hwasimir.p12.final-status.v1",
        "generatedUtc": utc_now(),
        "workspace": str(ROOT),
        "acceptanceMode": "DDS_ONLY",
        "overallStatus": "PARTIAL",
        "milestones": [
            {"id": "P12A", "status": "COMPLETE_WITH_LITERAL_MANUAL_CLICK_NOT_RUN"},
            {"id": "P12B", "status": "COMPLETE_ENGINEERING_REGRESSION"},
            {"id": "P12C", "status": "COMPLETE_ENGINEERING_DATA_CALIBRATION_NOT_VERIFIED"},
            {"id": "P12D", "status": "COMPLETE_WITH_SWIR_COLD_PERFORMANCE_FAIL"},
        ],
        "functionalStatus": "PASS",
        "performanceStatus": "FAIL",
        "performanceReason": "SWIR 60 s cold accepted-to-writer has 3 frames over 80 ms; max 84.159 ms",
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
        "manualClickStatus": "NOT_RUN_LITERAL_BUTTON_SEQUENCE",
        "identities": {
            "DataDrivenTestQT.exe": "1b8cdf2d88cf6bfa4d9de4cb0d8249ec3dca67eaba78db66ed47bd0c03d6ad16",
            "HwaSim_IR.Windows.exe": "53d213a65c572287d23fce2cf120661653c7903e48b332b6bf7678dd609355ea",
            "HwaSim_IR.RK3588.elf": "2d5791360307af746e103d0fdb394a003ef72f70c02f25edcfd3cec3ead2d583",
            "HwaSim_IR.RK3588.buildId": "25372bcde3528cace876c4d10c55b9bf91046ed7",
            "VideoDisplay.exe": "a832e28feee08259c829f14b455f484489d19eb210d8b7dbb660c4ba4b40898e",
            "band_lut_si.csv": "6f22d25d32bc00e753b454560af630d09c2e1a130d53835fa505e27d97f5fd99",
            "Targets.json": "16425775fac239f07d2dcf7186db5fccb684b43040fe03da626f6ffff393ad02",
            "boardConfigManifest": "ca19fc482985ff6b0ea75eb2614f4892fae2d5d14687e64edacb033539e977e7",
        },
        "evidence": {
            "ordinaryStartup": "evidence/p12a/ordinary-mixed-ui-20260917-204319",
            "assets": "evidence/p12b/asset_views/audit.json",
            "weatherCloud": "evidence/p12c/weather_cloud/audit.json",
            "glass": "evidence/p12c/controlled_glass/audit.json",
            "modtran2km": "evidence/p12c/modtran_2km/qc_results.json",
            "performance": "evidence/p12d/performance/ordinary-performance-20260918-043110/suite_summary.json",
            "lifecycle": "evidence/p12d/lifecycle/lifecycle_summary.json",
            "rollback": "evidence/p12d/rollback/pass/rollback_summary.json",
        },
        "prohibitions": [
            "No identity-filter removal or arbitrary-publisher subscription",
            "No Latest/input-drop/copy-old-frame/down-resolution performance workaround",
            "No historical failure relabeling",
            "No equipment-specific physical signature claim from generic assets",
            "No real calibration claim without traceable measurements",
        ],
        "archiveNote": "The non-recursive ZIP SHA-256 is stored in the sibling .sha256 sidecar outside the archive.",
    }


def offline_html() -> str:
    cards = [
        ("普通启动", "../evidence/p12a/ordinary-mixed-ui-20260917-204319/receiver_actual_widget.png", "严格 1001/2 + VideoStatus 自动发现，真实 Qt widget。"),
        ("四模型双波段", "../evidence/p12b/asset_views/contact_sheet.png", "16 个 RK H.264 DDS received 图，通用几何/材质/位姿回归。"),
        ("云雨雪", "../evidence/p12c/weather_cloud/contact_sheet_received_dds.png", "真实 LOS 云前后、单 cloudId 与 rain/snow 像素贡献。"),
        ("受控玻璃", "../evidence/p12c/controlled_glass/contact_sheet_received_dds.png", "SWIR/MWIR、明暗背景、透过 on/blocked，8/8 PASS。"),
        ("生命周期晚启动", "../evidence/p12d/lifecycle/receiver_late.widget.png", "晚启动、重启、SWIR→MWIR→SWIR、STOP→START。"),
        ("P12 回滚恢复", "../evidence/p12d/rollback/pass/p12_restored/ordinary_ui/receiver_actual_widget.png", "P11 收到新图后恢复 P12，再次收到新图。"),
    ]
    rendered = []
    for title, image_path, description in cards:
        rendered.append(
            f'<article><h2>{html.escape(title)}</h2><a href="{html.escape(image_path)}">'
            f'<img loading="lazy" src="{html.escape(image_path)}" alt="{html.escape(title)}"></a>'
            f'<p>{html.escape(description)}</p></article>'
        )
    return """<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HwaSimIR P12 离线交付索引</title><style>
body{font:15px/1.55 system-ui,"Microsoft YaHei",sans-serif;margin:0;background:#0d1117;color:#e6edf3}
header,main,footer{max-width:1180px;margin:auto;padding:24px}h1{margin:0 0 8px}a{color:#58a6ff}
.status{border-left:5px solid #d29922;background:#161b22;padding:12px 16px;margin:18px 0}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:18px}
article{background:#161b22;border:1px solid #30363d;border-radius:10px;padding:14px}article h2{font-size:18px;margin:0 0 10px}
img{display:block;width:100%;max-height:440px;object-fit:contain;background:#000;border-radius:6px}table{border-collapse:collapse;width:100%;margin:16px 0}td,th{border:1px solid #30363d;padding:8px;text-align:left}th{background:#21262d}
code{background:#21262d;padding:2px 5px;border-radius:4px}small{color:#8b949e}
</style></head><body><header><h1>HwaSimIR P12 离线交付索引</h1>
<div class="status"><strong>总体：PARTIAL。</strong> 普通 DDS 功能链、工程成像、2 km 数据、生命周期和回滚通过；SWIR 冷启动仍有 3 帧 &gt;80 ms，真实标定未验证。</div>
<p><a href="../docs/HwaSimIR_P12_Closeout.md">收口报告</a> · <a href="../docs/HwaSimIR_P12_ManualStartup_And_NoImage_Fix.md">普通启动</a> · <a href="../docs/HwaSimIR_P12_Imaging_Regression_And_Remaining.md">成像回归</a> · <a href="../final_status.json">结构化状态</a> · <a href="../delivery_file_manifest.json">成员清单</a></p></header>
<main><table><tr><th>里程碑</th><th>状态</th></tr>
<tr><td>P12A</td><td>普通启动真实有图；仅人手逐键未执行</td></tr><tr><td>P12B</td><td>16/16 资产工程回归</td></tr>
<tr><td>P12C</td><td>云/2 km/玻璃完成；标定未验证</td></tr><tr><td>P12D</td><td>生命周期/回滚完成；SWIR 冷启动性能 FAIL</td></tr></table>
<section class="grid">""" + "".join(rendered) + """</section></main>
<footer><small>所有 received 图均来自真实 DDS 解码；线性/固定映射/差分图按文件名分开，不冒充 received。ZIP SHA-256 见包外同名 .sha256。</small></footer></body></html>"""


def write_generated_files() -> tuple[Path, Path]:
    DELIVERY_DIR.mkdir(parents=True, exist_ok=True)
    final_status = DELIVERY_DIR / "final_status.json"
    report = DELIVERY_DIR / "index.html"
    final_status.write_text(json.dumps(final_status_payload(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    report.write_text(offline_html(), encoding="utf-8")
    return final_status, report


def secret_scan(items: Iterable[Item]) -> None:
    patterns = [
        re.compile(r"(?i)hwasimir_ssh_password\s*=\s*['\"]?(?:123|linaro)"),
        re.compile(r"(?i)password\s*[:=]\s*(?:123|linaro)\b"),
        re.compile(r"密码\s*(?:[:：=]|为)?\s*(?:123|linaro)\b"),
    ]
    findings: list[str] = []
    for item in items:
        path = item.source
        if path.suffix.lower() not in TEXT_SUFFIXES or path.stat().st_size > 16 * 1024 * 1024:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if any(pattern.search(text) for pattern in patterns):
            findings.append(f"{item.archive_path} <- {path}")
    if findings:
        raise RuntimeError("Credential-like literal found in delivery inputs:\n" + "\n".join(findings))


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            block = handle.read(4 * 1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def build_manifest(items: list[Item], manifest_path: Path) -> dict:
    entries = []
    total = 0
    checkpoint = 0
    for index, item in enumerate(items, 1):
        size = item.source.stat().st_size
        digest = sha256_file(item.source)
        total += size
        entries.append(
            {
                "path": item.archive_path.as_posix(),
                "bytes": size,
                "sha256": digest,
                "category": item.category,
            }
        )
        if total - checkpoint >= 256 * 1024 * 1024 or index == len(items):
            print(f"HASH {index}/{len(items)} files, {total / (1024**3):.3f} GiB", flush=True)
            checkpoint = total
    payload = {
        "schema": "hwasimir.p12.delivery-file-manifest.v1",
        "generatedUtc": utc_now(),
        "archiveRoot": ARCHIVE_ROOT.as_posix(),
        "entryCountExcludingManifest": len(entries),
        "totalBytesExcludingManifest": total,
        "manifestSelfListed": False,
        "entries": entries,
        "intentionalExclusions": [
            "DDS/MODTRAN license files and credentials",
            "debug symbols and compiler intermediates",
            "old MP4/H264 recordings and unrelated P11 UDP/TCP matrices",
            "multi-gigabyte non-runtime MODTRAN spectral audit tables; P12 2 km raw evidence is included",
        ],
    }
    manifest_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return payload


def zip_info_for(source: Path, archive_path: PurePosixPath) -> zipfile.ZipInfo:
    stat = source.stat()
    stamp = time.localtime(stat.st_mtime)[:6]
    # ZIP cannot represent dates before 1980.
    if stamp[0] < 1980:
        stamp = (1980, 1, 1, 0, 0, 0)
    info = zipfile.ZipInfo((ARCHIVE_ROOT / archive_path).as_posix(), stamp)
    info.external_attr = (stat.st_mode & 0xFFFF) << 16
    info.compress_type = zipfile.ZIP_STORED if source.suffix.lower() in STORED_SUFFIXES else zipfile.ZIP_DEFLATED
    return info


def add_to_zip(zf: zipfile.ZipFile, source: Path, archive_path: PurePosixPath, progress: dict[str, int]) -> None:
    info = zip_info_for(source, archive_path)
    with source.open("rb") as reader, zf.open(info, "w", force_zip64=True) as writer:
        while True:
            block = reader.read(4 * 1024 * 1024)
            if not block:
                break
            writer.write(block)
            progress["bytes"] += len(block)
            if progress["bytes"] - progress["checkpoint"] >= 256 * 1024 * 1024:
                print(f"ZIP {progress['bytes'] / (1024**3):.3f} GiB source streamed", flush=True)
                progress["checkpoint"] = progress["bytes"]


def verify_zip(archive: Path, manifest: dict) -> None:
    expected = {(ARCHIVE_ROOT / PurePosixPath(entry["path"])).as_posix() for entry in manifest["entries"]}
    expected.add((ARCHIVE_ROOT / PurePosixPath("delivery_file_manifest.json")).as_posix())
    with zipfile.ZipFile(archive, "r") as zf:
        bad = zf.testzip()
        if bad:
            raise RuntimeError(f"ZIP CRC verification failed at {bad}")
        actual = set(zf.namelist())
        if actual != expected:
            missing = sorted(expected - actual)
            extra = sorted(actual - expected)
            raise RuntimeError(f"ZIP member mismatch; missing={missing[:10]} extra={extra[:10]}")


def build_archive() -> dict:
    final_status, report = write_generated_files()
    add_windows_runtime()
    add_rk_runtime()
    add_sources_and_tools()
    add_docs()
    add_evidence()
    add_file(final_status, "final_status.json", "status")
    add_file(report, "report/index.html", "report")

    items = sorted(ITEMS.values(), key=lambda item: item.archive_path.as_posix().lower())
    secret_scan(items)
    manifest_path = DELIVERY_DIR / "delivery_file_manifest.json"
    manifest = build_manifest(items, manifest_path)

    temp_archive = DELIVERY_DIR / "HwaSimIR_P12_Delivery.zip.partial"
    for target in (temp_archive, ARCHIVE):
        resolved_inside_workspace(target)
        if target.exists():
            target.unlink()
    progress = {"bytes": 0, "checkpoint": 0}
    with zipfile.ZipFile(temp_archive, "w", allowZip64=True, compresslevel=6) as zf:
        for index, item in enumerate(items, 1):
            add_to_zip(zf, item.source, item.archive_path, progress)
            if index % 500 == 0:
                print(f"ZIP {index}/{len(items)} members", flush=True)
        add_to_zip(zf, manifest_path, PurePosixPath("delivery_file_manifest.json"), progress)
    temp_archive.replace(ARCHIVE)
    verify_zip(ARCHIVE, manifest)

    archive_sha = sha256_file(ARCHIVE)
    sidecar = DELIVERY_DIR / "HwaSimIR_P12_Delivery.zip.sha256"
    sidecar.write_text(f"{archive_sha}  {ARCHIVE.name}\n", encoding="ascii")
    receipt = {
        "schema": "hwasimir.p12.delivery-receipt.v1",
        "generatedUtc": utc_now(),
        "archive": str(ARCHIVE),
        "bytes": ARCHIVE.stat().st_size,
        "sha256": archive_sha,
        "members": len(manifest["entries"]) + 1,
        "payloadBytes": manifest["totalBytesExcludingManifest"],
        "zipCrcVerified": True,
        "credentialAndLicenseScan": "PASS",
        "manifest": str(manifest_path),
        "sha256Sidecar": str(sidecar),
    }
    receipt_path = DELIVERY_DIR / "delivery_receipt.json"
    receipt_path.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--list-only", action="store_true", help="collect inputs and print counts without hashing/zipping")
    args = parser.parse_args()
    if args.list_only:
        final_status, report = write_generated_files()
        add_windows_runtime()
        add_rk_runtime()
        add_sources_and_tools()
        add_docs()
        add_evidence()
        add_file(final_status, "final_status.json", "status")
        add_file(report, "report/index.html", "report")
        items = sorted(ITEMS.values(), key=lambda item: item.archive_path.as_posix().lower())
        secret_scan(items)
        total = sum(item.source.stat().st_size for item in items)
        by_category: dict[str, dict[str, int]] = {}
        for item in items:
            entry = by_category.setdefault(item.category, {"files": 0, "bytes": 0})
            entry["files"] += 1
            entry["bytes"] += item.source.stat().st_size
        print(json.dumps({"files": len(items), "bytes": total, "categories": by_category}, indent=2))
        return 0
    receipt = build_archive()
    print(json.dumps(receipt, ensure_ascii=False, indent=2), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
