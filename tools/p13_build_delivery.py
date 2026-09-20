#!/usr/bin/env python3
"""Build the immutable P13 offline evidence delivery and verify every MP4."""

from __future__ import annotations

import datetime as dt
import hashlib
import html
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "logs" / "p13" / "delivery_final"
ZIP_PATH = ROOT / "HwaSimIR_P13_Delivery.zip"
ZIP_SHA_PATH = ROOT / "HwaSimIR_P13_Delivery.zip.sha256"

FINAL_ELF_SHA = "779e7431228326789aa5daffea14ddcef7089bb23686882a362db764ee699765"
FINAL_BUILD_ID = "ce57610d48d9065e2e126b10d587e71e310b67c0"
FINAL_CONFIG_SHA = "6dc22e3f9e3e84e5a102efb46272028d29b194af98abdeeba3d0940c11d9d2ff"
RUNTIME_INI_SHA = "bbe2fad9fa9fffad9876bda8b1a106f986cad975f7812fbfdd254bdfedd05d67"
INPUT_SHA = "f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901"
LUT_SHA = "de72d333dfab19989ba56856075a7d95ea5d9305f84bc1ab2a3ef9e5a97464be"
COVERAGE_SHA = "be2cf0d2e7f89827b44ba0952c89dceb3c040b13c28881ab92ebb7fef4c752dc"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def require_file(relative: str) -> Path:
    path = ROOT / relative
    if not path.is_file():
        raise FileNotFoundError(f"required file missing: {relative}")
    return path


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


copied: list[dict[str, object]] = []


def copy_file(source_relative: str, dest_relative: str, role: str, provenance: str = "P13") -> Path:
    source = require_file(source_relative)
    destination = OUT / dest_relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    item = {
        "path": destination.relative_to(OUT).as_posix(),
        "bytes": destination.stat().st_size,
        "sha256": sha256(destination),
        "role": role,
        "provenance": provenance,
        "source": source.relative_to(ROOT).as_posix(),
    }
    copied.append(item)
    return destination


def copy_optional(source_relative: str, dest_relative: str, role: str, provenance: str = "P13") -> Path | None:
    if not (ROOT / source_relative).is_file():
        return None
    return copy_file(source_relative, dest_relative, role, provenance)


def copy_tree(source_relative: str, dest_relative: str, role: str, provenance: str = "P13") -> None:
    source_root = ROOT / source_relative
    if not source_root.is_dir():
        raise FileNotFoundError(f"required directory missing: {source_relative}")
    for source in sorted(p for p in source_root.rglob("*") if p.is_file()):
        suffix = source.relative_to(source_root).as_posix()
        copy_file(source.relative_to(ROOT).as_posix(), f"{dest_relative}/{suffix}", role, provenance)


def find_case_mp4(case_name: str) -> Path:
    matches = sorted((ROOT / "logs" / "p13" / "runs" / case_name / "recording").glob("round_*/output.mp4"))
    if len(matches) != 1:
        raise RuntimeError(f"expected exactly one MP4 for {case_name}, found {len(matches)}")
    return matches[0]


CASE_TOP_FILES = [
    "case_plan.json",
    "case_result.json",
    "media_qc.json",
    "performance.json",
    "performance_summary.md",
    "event_timeline.json",
    "frame_identity.csv",
    "latency_all_frames.csv",
    "DataDrivenTestQT.NetworkConfig.ini",
    "VideoDisplay.NetworkConfig.ini",
    "display_atmosphere_weather_evidence.txt",
    "board_preflight.log",
    "board.log",
    "receiver.out.log",
    "receiver.err.log",
    "sender.out.log",
    "sender.err.log",
    "sender_original_1.png",
    "sender_original_1.png.json",
    "receiver_normal_material.png",
    "receiver_normal_material.png.layout.json",
]


def copy_case(case_name: str, classification: str, include_received: bool = False) -> None:
    case_root = ROOT / "logs" / "p13" / "runs" / case_name
    if not case_root.is_dir():
        raise FileNotFoundError(f"required case missing: {case_name}")
    base = f"evidence/cases/{case_name}"
    provenance = "P13_FINAL" if classification == "FINAL_SAME_ELF" else classification
    for filename in CASE_TOP_FILES:
        copy_optional(
            f"logs/p13/runs/{case_name}/{filename}",
            f"{base}/{filename}",
            f"{classification} case evidence",
            provenance,
        )
    if include_received:
        copy_file(
            f"logs/p13/runs/{case_name}/received.h264",
            f"{base}/received.h264",
            "received Annex-B H.264 stream",
            provenance,
        )
    keyframes = case_root / "keyframes"
    if keyframes.is_dir():
        for source in sorted(p for p in keyframes.iterdir() if p.is_file()):
            copy_file(
                source.relative_to(ROOT).as_posix(),
                f"{base}/keyframes/{source.name}",
                f"{classification} keyframe",
                provenance,
            )
    recording = case_root / "recording"
    for source in sorted(recording.glob("round_*/*")):
        if source.is_file() and source.name != "output.mp4":
            rel = source.relative_to(case_root).as_posix()
            copy_file(
                source.relative_to(ROOT).as_posix(),
                f"{base}/{rel}",
                f"{classification} recording metadata",
                provenance,
            )


def assert_source_hash(relative: str, expected: str) -> None:
    actual = sha256(require_file(relative))
    if actual != expected:
        raise RuntimeError(f"identity mismatch for {relative}: {actual} != {expected}")


def locate_media_tools() -> tuple[str, str]:
    bundled = ROOT / ".deps" / "ffmpeg-n8.1-win64-gpl-shared" / "ffmpeg-n8.1-latest-win64-gpl-shared-8.1" / "bin"
    ffmpeg = shutil.which("ffmpeg") or (str(bundled / "ffmpeg.exe") if (bundled / "ffmpeg.exe").is_file() else "")
    ffprobe = shutil.which("ffprobe") or (str(bundled / "ffprobe.exe") if (bundled / "ffprobe.exe").is_file() else "")
    if not ffmpeg or not ffprobe:
        raise RuntimeError("ffmpeg and ffprobe are required")
    return ffmpeg, ffprobe


def verify_mp4(path: Path, ffmpeg: str, ffprobe: str) -> dict[str, object]:
    probe = subprocess.run(
        [
            ffprobe,
            "-v", "error",
            "-count_frames",
            "-select_streams", "v:0",
            "-show_entries", "stream=codec_name,width,height,r_frame_rate,avg_frame_rate,duration,nb_frames,nb_read_frames",
            "-show_entries", "format=duration,size,format_name",
            "-of", "json",
            str(path),
        ],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    decoded = subprocess.run(
        [ffmpeg, "-v", "error", "-i", str(path), "-f", "null", "NUL"],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if decoded.returncode != 0 or decoded.stderr.strip():
        raise RuntimeError(f"full decode failed for {path.name}: {decoded.stderr.strip()}")
    payload = json.loads(probe.stdout)
    stream = payload.get("streams", [{}])[0]
    if stream.get("codec_name") != "h264" or int(stream.get("width", 0)) != 800 or int(stream.get("height", 0)) != 800:
        raise RuntimeError(f"unexpected media shape for {path.name}: {stream}")
    return {
        "path": path.relative_to(OUT).as_posix(),
        "sha256": sha256(path),
        "bytes": path.stat().st_size,
        "probe": payload,
        "fullDecode": "PASS",
        "stderr": "",
    }


def extract_frame(video: Path, zero_based_frame: int, destination: Path, ffmpeg: str) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        [
            ffmpeg,
            "-v", "error",
            "-i", str(video),
            "-vf", f"select=eq(n\\,{zero_based_frame})",
            "-vsync", "0",
            "-frames:v", "1",
            str(destination),
        ],
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if result.returncode != 0 or not destination.is_file():
        raise RuntimeError(f"frame extraction failed: {video.name} n={zero_based_frame}: {result.stderr}")


def run_regression_checks() -> dict[str, object]:
    checks = [
        (
            "stage3_modtran_tau_loader_strict",
            ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools/stage3_modtran_tau_loader_check.ps1"), "-Strict"],
        ),
        (
            "stage4_hotspot_strict",
            ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools/stage4_hotspot_check.ps1"), "-Strict"],
        ),
        (
            "stage5_plume_strict",
            ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools/stage5_plume_check.ps1"), "-Strict"],
        ),
        (
            "p13_original_track_query",
            ["powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(ROOT / "tools/p13_track_query_check.ps1")],
        ),
    ]
    results = []
    for name, command in checks:
        completed = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding="utf-8", errors="replace")
        item = {
            "name": name,
            "returnCode": completed.returncode,
            "result": "PASS" if completed.returncode == 0 else "FAIL",
            "stdout": completed.stdout,
            "stderr": completed.stderr,
        }
        results.append(item)
        if completed.returncode != 0:
            raise RuntimeError(f"regression check failed: {name}: {completed.stderr}")
    return {
        "schema": "HwaSimIR.P13.DeliveryRegressionValidation.1",
        "result": "PASS",
        "checks": results,
    }


def build_html(video_rows: list[dict[str, str]], final_status: dict[str, object]) -> str:
    cards = []
    for item in video_rows:
        label = html.escape(item["label"])
        path = html.escape(item["path"])
        badge = html.escape(item["classification"])
        cards.append(
            f'<article><span class="badge">{badge}</span><h3>{label}</h3>'
            f'<video controls preload="metadata" width="400" src="{path}"></video>'
            f'<p><a href="{path}">打开 MP4</a></p></article>'
        )
    image_cards = []
    for name, label in [
        ("clear.png", "晴天组合"),
        ("cloudy_visibility.png", "云 / 能见度"),
        ("rain.png", "雨"),
        ("snow.png", "雪"),
        ("material_swir.png", "SWIR 正常材质"),
        ("material_mwir.png", "MWIR 正常材质"),
        ("plume_on.png", "通用热源 ON"),
        ("plume_off.png", "通用热源 OFF"),
    ]:
        image_cards.append(f'<figure><img src="images/{name}" alt="{label}"><figcaption>{label}</figcaption></figure>')
    return f"""<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HwaSimIR P13 离线交付</title>
<style>
body{{font:15px/1.55 system-ui,Segoe UI,sans-serif;margin:0;background:#10141c;color:#e8eef7}}main{{max-width:1180px;margin:auto;padding:28px}}
h1,h2{{color:#fff}}a{{color:#8bc7ff}}.summary{{background:#17202d;border:1px solid #314158;border-radius:12px;padding:18px}}
.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(390px,1fr));gap:16px}}article,figure{{margin:0;background:#17202d;border:1px solid #314158;border-radius:12px;padding:14px}}video,img{{max-width:100%;height:auto;background:#000}}.badge{{font-size:12px;background:#2d6a4f;padding:3px 8px;border-radius:99px}}code{{color:#bde0fe}}
</style></head><body><main>
<h1>HwaSimIR P13 离线交付</h1>
<section class="summary"><p><strong>工程结果：</strong>ENGINEERING_PASS；<strong>标定：</strong>NOT_VERIFIED_CALIBRATION。</p>
<p>原始 <code>1.txt</code> 未修改；原轨迹真实范围与独立 50 km 通用覆盖分开声明。最终性能轮次使用同一 ELF/配置。所有下列 MP4 已 ffprobe 且逐帧完整解码。</p>
<p><a href="docs/HwaSimIR_P13_Closeout.md">关闭报告</a> · <a href="docs/HwaSimIR_P13_Ordinary_Runbook.md">普通回放手册</a> · <a href="final_status.json">机器可读状态</a> · <a href="SHA256SUMS.txt">全部哈希</a></p></section>
<h2>混合关键帧</h2><section class="grid">{''.join(image_cards)}</section>
<h2>真实 DDS 与专项视频</h2><section class="grid">{''.join(cards)}</section>
<h2>说明</h2><p>“FINAL_SAME_ELF” 是最终同一程序/配置轮次；“REUSED_UNAFFECTED” 是按要求复用且明确标记的旧云/雨证据，不计入最终性能结论。诊断 raw 与普通性能采集分开。</p>
</main></body></html>"""


def main() -> int:
    if OUT.exists() or ZIP_PATH.exists() or ZIP_SHA_PATH.exists():
        raise RuntimeError("delivery output already exists; refusing to overwrite")

    assert_source_hash("DataDrivenTestQT/1.txt", INPUT_SHA)
    assert_source_hash("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv", LUT_SHA)
    assert_source_hash("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/p13_coverage_manifest.json", COVERAGE_SHA)
    assert_source_hash("HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini", RUNTIME_INI_SHA)
    assert_source_hash("logs/p13/build/slot-keepalive-v1/HwaSim_IR.aarch64", FINAL_ELF_SHA)

    OUT.mkdir(parents=True)

    docs = [
        "HwaSimIR_P13_Execution_State.md",
        "HwaSimIR_P13_OriginalInput_And_Atmosphere_Coverage.md",
        "HwaSimIR_P13_Image_RootCause_And_Fix.md",
        "HwaSimIR_P13_Ordinary_Runbook.md",
        "HwaSimIR_P13_Closeout.md",
        "HwaSimIR_P13_Issue_Ledger.csv",
        "HwaSimIR_P13_P12_Audit.md",
        "HwaSimIR_P13_Codex_Prompt.md",
    ]
    for name in docs:
        copy_file(f"docs/{name}", f"docs/{name}", "documentation")

    reproducibility_tools = [
        "p13_original_input_audit.py",
        "p13_track_query.cpp",
        "p13_track_query_check.ps1",
        "p13_original_dds_case.ps1",
        "p13_media_qc.py",
        "p12_performance_analyze.py",
        "p13_generate_performance_fixture.py",
        "p13_modtran_grid.py",
        "p13_modtran_run.ps1",
        "p13_modtran_qc.py",
        "p13_modtran_publish.py",
        "p13_material_pixel_qc.py",
        "p13_plume_pixel_qc.py",
        "p13_build_delivery.py",
    ]
    for name in reproducibility_tools:
        copy_file(f"tools/{name}", f"tools/{name}", "reproducibility tool")

    for source, dest, role in [
        ("DataDrivenTestQT/1.txt", "inputs/DataDrivenTestQT/1.txt", "immutable original replay"),
        ("logs/p13/input_audit_final/input_1_audit.json", "inputs/audit/input_1_audit.json", "full input audit"),
        ("logs/p13/input_audit_final/input_1_audit.md", "inputs/audit/input_1_audit.md", "full input audit"),
        ("logs/p13/input_audit_final/input_1_query_manifest.csv", "inputs/audit/input_1_query_manifest.csv", "all SWIR/MWIR queries"),
        ("logs/p13/input_audit_final/query_check/p13_track_query.log", "inputs/audit/p13_track_query.log", "production LUT query result"),
        ("DataDrivenTestQT/p13_performance_highalt_300s.txt", "inputs/performance/p13_performance_highalt_300s.txt", "performance-only fixture"),
        ("DataDrivenTestQT/p13_performance_highalt_300s.txt.json", "inputs/performance/p13_performance_highalt_300s.txt.json", "performance fixture manifest"),
    ]:
        copy_file(source, dest, role)

    for source, dest, role in [
        ("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv", "atmosphere/band_lut_si.csv", "published processed LUT"),
        ("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/p13_coverage_manifest.json", "atmosphere/p13_coverage_manifest.json", "shared runtime coverage identity"),
        ("logs/p13/atmosphere/track50/formal_publish/publish_manifest.json", "atmosphere/publish_manifest.json", "incremental publish provenance"),
        ("logs/p13/atmosphere/track50/qc_results.json", "atmosphere/qc_results.json", "MODTRAN grid QC"),
        ("logs/p13/atmosphere/track50/qc_results.csv", "atmosphere/qc_results.csv", "MODTRAN grid QC"),
        ("logs/p13/atmosphere/track50/generation_provenance.json", "atmosphere/generation_provenance.json", "generation provenance"),
        ("logs/p13/atmosphere/track50/engine_and_license_evidence.json", "atmosphere/engine_and_license_evidence.json", "engine identity without licensed binary"),
        ("logs/p13/atmosphere/track50/case_manifest.json", "atmosphere/case_manifest.json", "720 component cases"),
        ("logs/p13/atmosphere/track50/case_manifest.csv", "atmosphere/case_manifest.csv", "720 component cases"),
        ("logs/p13/atmosphere/track50/run_manifest.csv", "atmosphere/run_manifest.csv", "component run ledger"),
        ("logs/p13/atmosphere/track50/formal_track50_rows.csv", "atmosphere/formal_track50_rows.csv", "336 published vertices"),
        ("logs/p13/atmosphere/track50/vertex_metrics.csv", "atmosphere/vertex_metrics.csv", "vertex metrics"),
        ("logs/p13/atmosphere/track50/spectral_qc.csv", "atmosphere/spectral_qc.csv", "spectral QC"),
        ("logs/p13/atmosphere/track50/flux_vertical_interpolation.csv", "atmosphere/flux_vertical_interpolation.csv", "real flux interpolation ledger"),
        ("logs/p13/atmosphere/track50/README.md", "atmosphere/README.md", "atmosphere evidence guide"),
    ]:
        copy_file(source, dest, role)

    for source, dest, role in [
        ("logs/p13/build/slot-keepalive-v1/HwaSim_IR.aarch64", "runtime/rk3588/HwaSim_IR.aarch64", "final RK3588 ELF"),
        ("logs/p13/build/slot-keepalive-v1/build_receipt.json", "runtime/rk3588/build_receipt.json", "final build receipt"),
        ("logs/p13/build/slot-keepalive-v1/source_build_plan.json", "runtime/rk3588/source_build_plan.json", "source build plan"),
        ("logs/p13/build/slot-keepalive-v1/source_manifest.sha256", "runtime/rk3588/source_manifest.sha256", "source tree manifest"),
        ("logs/p13/build/slot-keepalive-v1/HwaSimIR-p11-20260919-001738.source.tgz", "runtime/rk3588/HwaSimIR-final.source.tgz", "source archive"),
        ("logs/p13/build/slot-keepalive-v1/HwaSimIR-p11-20260919-001738.source.tgz.sha256", "runtime/rk3588/HwaSimIR-final.source.tgz.sha256", "source archive hash"),
        ("logs/p13/build/slot-keepalive-v1/vm_build.log", "runtime/rk3588/vm_build.log", "VM build log"),
        ("logs/p13/deploy/slot-keepalive-v1/deployment_final.txt", "runtime/deployment/deployment_final.txt", "final deployment identity"),
        ("logs/p13/deploy/slot-keepalive-v1/Config.20260919-002235.tgz", "runtime/deployment/Config.final.delta.tgz", "deployed config delta"),
        ("logs/p13/deploy/slot-keepalive-v1/delta_files.txt", "runtime/deployment/delta_files.txt", "deployment delta list"),
        ("logs/p13/deploy/slot-keepalive-v1/removed_files.txt", "runtime/deployment/removed_files.txt", "deployment removal list"),
        ("build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe", "runtime/windows/DataDrivenTestQT.exe", "Windows DDS sender"),
        ("HwaSim_IR_VideoDisplay/x64/Release/HwaSim_IR_VideoDisplay.exe", "runtime/windows/HwaSim_IR_VideoDisplay.exe", "Windows DDS receiver and recorder"),
        ("HwaSim_IR/Bin/HwaSim_IR.exe", "runtime/windows/HwaSim_IR.exe", "Windows renderer build"),
        ("DataDrivenTestQT/NetworkConfig.ini", "runtime/config/DataDrivenTestQT.NetworkConfig.ini", "ordinary sender config"),
        ("HwaSim_IR_VideoDisplay/x64/Release/NetworkConfig.ini", "runtime/config/VideoDisplay.NetworkConfig.ini", "receiver config"),
        ("HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini", "runtime/config/HwaSimIRRuntime.ini", "final runtime config"),
        ("HwaSim_IR/Bin/Config/Tests/P5/MaterialBandOptics_A.csv", "runtime/config/MaterialBandOptics_A.csv", "generic material A"),
        ("HwaSim_IR/Bin/Config/Tests/P5/MaterialBandOptics_B.csv", "runtime/config/MaterialBandOptics_B.csv", "generic material B"),
        ("HwaSim_IR/Bin/Config/Weather/weather_profiles.json", "runtime/config/weather_profiles.json", "weather profiles"),
        ("HwaSim_IR/Bin/Config/Weather/world_cloud_demo.json", "runtime/config/world_cloud_demo.json", "world-space cloud config"),
        ("HwaSim_IR/Bin/Config/Weather/world_cloud_game.json", "runtime/config/world_cloud_game.json", "world-space cloud config"),
    ]:
        copy_file(source, dest, role)

    final_cases = [
        "final13_original_1_SWIR_Clear",
        "final13_original_1_MWIR_Snow",
        "final13_MWIR_Snow_300s",
    ]
    reused_cases = ["original_1_SWIR_Cloudy_full", "original_1_MWIR_Rain_full"]
    material_cases = ["material_sample_SWIR_normal_view", "material_sample_MWIR_normal_view"]
    plume_cases = ["plume_generic_MWIR_on_final2", "plume_generic_MWIR_off_final"]
    diagnostic_cases = [
        "diag_final13_original_1_MWIR_Snow_fixedraw",
        "diag_final13_original_1_MWIR_Snow_agcraw",
    ]
    for case in final_cases:
        copy_case(case, "FINAL_SAME_ELF", include_received=True)
    for case in reused_cases:
        copy_case(case, "REUSED_UNAFFECTED", include_received=False)
    for case in material_cases:
        copy_case(case, "SPECIALIZED_MATERIAL", include_received=False)
    for case in plume_cases:
        copy_case(case, "SPECIALIZED_PLUME", include_received=False)
    for case in diagnostic_cases:
        copy_case(case, "DIAGNOSTIC_NOT_PERFORMANCE", include_received=False)
        copy_tree(
            f"logs/p13/runs/{case}/diagnostic_linear",
            f"diagnostics/{case}/linear",
            "linear radiance diagnostic",
            "DIAGNOSTIC_NOT_PERFORMANCE",
        )

    copy_file("logs/p13/material/material_normal_view_qc.json", "evidence/material/material_normal_view_qc.json", "normal material QC")
    copy_file("logs/p13/plume/plume_qc.json", "evidence/plume/plume_qc.json", "plume causal QC")
    for source, dest in [
        ("logs/p12/p12c/controlled_glass/evidence/audit.json", "evidence/p12_reused/controlled_glass/audit.json"),
        ("logs/p12/p12c/controlled_glass/evidence/audit.csv", "evidence/p12_reused/controlled_glass/audit.csv"),
        ("logs/p12/p12c/controlled_glass/evidence/contact_sheet_received_dds.png", "evidence/p12_reused/controlled_glass/contact_sheet_received_dds.png"),
    ]:
        copy_file(source, dest, "preserved P12 glass evidence", "P12_REUSED_UNAFFECTED")

    image_map = [
        ("logs/p13/runs/final13_original_1_SWIR_Clear/keyframes/middle.png", "images/clear.png", "final clear mixed frame", "P13_FINAL"),
        ("logs/p13/runs/original_1_SWIR_Cloudy_full/keyframes/middle.png", "images/cloudy_visibility.png", "reused cloudy/visibility mixed frame", "P13_REUSED_UNAFFECTED"),
        ("logs/p13/runs/original_1_MWIR_Rain_full/keyframes/middle.png", "images/rain.png", "reused rain mixed frame", "P13_REUSED_UNAFFECTED"),
        ("logs/p13/runs/final13_original_1_MWIR_Snow/keyframes/middle.png", "images/snow.png", "final snow mixed frame", "P13_FINAL"),
        ("logs/p13/runs/material_sample_SWIR_normal_view/keyframes/middle.png", "images/material_swir.png", "normal material SWIR", "P13_SPECIALIZED"),
        ("logs/p13/runs/material_sample_MWIR_normal_view/keyframes/middle.png", "images/material_mwir.png", "normal material MWIR", "P13_SPECIALIZED"),
        ("logs/p13/runs/plume_generic_MWIR_on_final2/keyframes/middle.png", "images/plume_on.png", "generic heat source on", "P13_SPECIALIZED"),
        ("logs/p13/runs/plume_generic_MWIR_off_final/keyframes/middle.png", "images/plume_off.png", "generic heat source off", "P13_SPECIALIZED"),
    ]
    for source, dest, role, provenance in image_map:
        copy_file(source, dest, role, provenance)

    video_specs = [
        ("final13_original_1_SWIR_Clear", "videos/01_original_1_SWIR_Clear_final.mp4", "原始 1.txt SWIR 晴天完整回放", "FINAL_SAME_ELF"),
        ("final13_original_1_MWIR_Snow", "videos/02_original_1_MWIR_Snow_final.mp4", "原始 1.txt MWIR 雪天完整回放", "FINAL_SAME_ELF"),
        ("final13_MWIR_Snow_300s", "videos/03_performance_MWIR_Snow_300s_final.mp4", "MWIR 雪天 300 秒复杂天气长测", "FINAL_SAME_ELF"),
        ("original_1_SWIR_Cloudy_full", "videos/04_original_1_SWIR_Cloudy_reused.mp4", "原始 1.txt SWIR 云和能见度", "REUSED_UNAFFECTED"),
        ("original_1_MWIR_Rain_full", "videos/05_original_1_MWIR_Rain_reused.mp4", "原始 1.txt MWIR 雨", "REUSED_UNAFFECTED"),
        ("material_sample_SWIR_normal_view", "videos/06_material_SWIR_normal_M1.mp4", "SWIR 正常材质正式 M1", "SPECIALIZED_MATERIAL"),
        ("material_sample_MWIR_normal_view", "videos/07_material_MWIR_normal_M1.mp4", "MWIR 正常材质正式 M1", "SPECIALIZED_MATERIAL"),
        ("plume_generic_MWIR_on_final2", "videos/08_plume_MWIR_generic_on.mp4", "MWIR 通用受控热源 ON", "SPECIALIZED_PLUME"),
        ("plume_generic_MWIR_off_final", "videos/09_plume_MWIR_generic_off.mp4", "MWIR 通用受控热源 OFF", "SPECIALIZED_PLUME"),
        ("diag_final13_original_1_MWIR_Snow_fixedraw", "videos/10_diagnostic_MWIR_fixed_mapping.mp4", "MWIR 固定映射 raw 诊断配套视频", "DIAGNOSTIC_NOT_PERFORMANCE"),
        ("diag_final13_original_1_MWIR_Snow_agcraw", "videos/11_diagnostic_MWIR_AGC.mp4", "MWIR AGC raw 诊断配套视频", "DIAGNOSTIC_NOT_PERFORMANCE"),
    ]
    video_rows: list[dict[str, str]] = []
    video_paths: dict[str, Path] = {}
    for case, dest, label, classification in video_specs:
        source = find_case_mp4(case)
        copied_video = copy_file(
            source.relative_to(ROOT).as_posix(),
            dest,
            label,
            classification,
        )
        video_rows.append({"case": case, "path": dest, "label": label, "classification": classification})
        video_paths[case] = copied_video

    ffmpeg, ffprobe = locate_media_tools()
    media_verification = [verify_mp4(path, ffmpeg, ffprobe) for path in video_paths.values()]
    write_json(
        OUT / "media_verification.json",
        {
            "schema": "HwaSimIR.P13.DeliveryMediaVerification.1",
            "result": "PASS",
            "method": "ffprobe plus full ffmpeg decode to null for every packaged MP4",
            "tools": {"ffmpeg": ffmpeg, "ffprobe": ffprobe},
            "videos": media_verification,
        },
    )

    for case in diagnostic_cases:
        mode = "fixed" if "fixedraw" in case else "agc"
        for seq in (1, 2159, 4318):
            frame = OUT / "diagnostics" / case / f"received_seq{seq}.png"
            extract_frame(video_paths[case], seq - 1, frame, ffmpeg)
            copied.append({
                "path": frame.relative_to(OUT).as_posix(),
                "bytes": frame.stat().st_size,
                "sha256": sha256(frame),
                "role": f"exact received frame seq {seq} ({mode})",
                "provenance": "GENERATED_FROM_PACKAGED_DIAGNOSTIC_MP4",
                "source": video_paths[case].relative_to(OUT).as_posix(),
            })

    raw_identity: dict[str, object] = {"schema": "HwaSimIR.P13.RawIdentity.1", "result": "PASS", "pairs": []}
    for seq in (1, 2159, 4318):
        fixed = next((OUT / "diagnostics" / diagnostic_cases[0] / "linear").glob(f"*seq{seq}.pfm"))
        agc = next((OUT / "diagnostics" / diagnostic_cases[1] / "linear").glob(f"*seq{seq}.pfm"))
        fixed_hash, agc_hash = sha256(fixed), sha256(agc)
        if fixed_hash != agc_hash:
            raise RuntimeError(f"raw PFM identity mismatch at seq {seq}")
        raw_identity["pairs"].append({
            "sourceSeq": seq,
            "fixedMappingPfm": fixed.relative_to(OUT).as_posix(),
            "agcPfm": agc.relative_to(OUT).as_posix(),
            "sha256": fixed_hash,
            "identical": True,
            "domain": "spectral_radiance",
            "unit": "W/(m^2_sr_um)",
        })
    write_json(OUT / "diagnostics" / "raw_identity.json", raw_identity)

    reused_manifest = {
        "schema": "HwaSimIR.P13.ReusedEvidence.1",
        "policy": "Unchanged evidence is preserved and labeled; it is not counted as final same-ELF performance evidence.",
        "cases": [
            {"case": case, "classification": "REUSED_UNAFFECTED", "reason": "weather path unaffected by final slot keepalive cold-start change"}
            for case in reused_cases
        ],
        "p12ControlledGlass": "preserved unchanged; not rerun",
    }
    write_json(OUT / "reused_evidence_manifest.json", reused_manifest)

    final_status = {
        "schema": "HwaSimIR.P13.FinalStatus.1",
        "generatedUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "engineeringResult": "PASS",
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
        "identities": {
            "original1TxtSha256": INPUT_SHA,
            "finalElfSha256": FINAL_ELF_SHA,
            "finalElfBuildId": FINAL_BUILD_ID,
            "configManifestSha256": FINAL_CONFIG_SHA,
            "runtimeIniSha256": RUNTIME_INI_SHA,
            "formalLutSha256": LUT_SHA,
            "coverageManifestSha256": COVERAGE_SHA,
        },
        "originalReplay": {
            "rows": 4318,
            "actualLosRangeKm": [2.427569473, 22.269183094],
            "contains50Km": False,
            "swirCase": final_cases[0],
            "mwirCase": final_cases[1],
        },
        "separateGenericCoverage": {"maxRangeKm": 50.0, "visibilityKm": [6.0]},
        "finalSameElfCases": final_cases,
        "reusedUnaffectedCases": reused_cases,
        "mp4Verification": "media_verification.json",
        "openIssues": [
            "NOT_VERIFIED_CALIBRATION",
            "formal visibility coverage is 6 km only",
            "Qt actual paint event has rare >80 ms gaps; board accepted-to-writer hard gate and sequences pass",
        ],
        "credentialPolicy": "No account passwords are included.",
        "licensedSoftwarePolicy": "No MODTRAN executable or licensed installation payload is included.",
    }
    write_json(OUT / "final_status.json", final_status)
    write_json(OUT / "validation" / "regression_checks.json", run_regression_checks())

    (OUT / "index.html").write_text(build_html(video_rows, final_status), encoding="utf-8")
    (OUT / "README.txt").write_text(
        "HwaSimIR P13 offline delivery\n"
        "Open index.html in a browser. Engineering result: PASS. "
        "Calibration remains NOT_VERIFIED_CALIBRATION.\n"
        "No passwords or licensed MODTRAN executable are included.\n",
        encoding="utf-8",
    )

    manifest_files = []
    for path in sorted(p for p in OUT.rglob("*") if p.is_file() and p.name not in {"evidence_manifest.json", "SHA256SUMS.txt"}):
        manifest_files.append({
            "path": path.relative_to(OUT).as_posix(),
            "bytes": path.stat().st_size,
            "sha256": sha256(path),
        })
    write_json(
        OUT / "evidence_manifest.json",
        {
            "schema": "HwaSimIR.P13.DeliveryManifest.1",
            "generatedUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "fileCountExcludingThisManifestAndShaList": len(manifest_files),
            "files": manifest_files,
            "copyLedger": copied,
        },
    )

    sha_lines = []
    for path in sorted(p for p in OUT.rglob("*") if p.is_file() and p.name != "SHA256SUMS.txt"):
        sha_lines.append(f"{sha256(path)}  {path.relative_to(OUT).as_posix()}")
    (OUT / "SHA256SUMS.txt").write_text("\n".join(sha_lines) + "\n", encoding="utf-8")

    with zipfile.ZipFile(ZIP_PATH, "w", allowZip64=True) as archive:
        for path in sorted(p for p in OUT.rglob("*") if p.is_file()):
            arcname = (Path("HwaSimIR_P13_Delivery") / path.relative_to(OUT)).as_posix()
            suffix = path.suffix.lower()
            compression = zipfile.ZIP_STORED if suffix in {".mp4", ".h264", ".png", ".tgz", ".zip"} else zipfile.ZIP_DEFLATED
            archive.write(path, arcname, compress_type=compression, compresslevel=6 if compression == zipfile.ZIP_DEFLATED else None)
    with zipfile.ZipFile(ZIP_PATH, "r") as archive:
        bad = archive.testzip()
        if bad:
            raise RuntimeError(f"ZIP CRC failure: {bad}")
        mp4_members = [name for name in archive.namelist() if name.lower().endswith(".mp4")]
        if len(mp4_members) != len(video_specs):
            raise RuntimeError(f"ZIP MP4 count mismatch: {len(mp4_members)} != {len(video_specs)}")

    zip_hash = sha256(ZIP_PATH)
    ZIP_SHA_PATH.write_text(f"{zip_hash}  {ZIP_PATH.name}\n", encoding="ascii")
    print(json.dumps({
        "result": "PASS",
        "directory": str(OUT),
        "zip": str(ZIP_PATH),
        "zipSha256": zip_hash,
        "zipBytes": ZIP_PATH.stat().st_size,
        "mp4Count": len(video_specs),
        "manifestFiles": len(sha_lines),
    }, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"P13 delivery build FAILED: {exc}", file=sys.stderr)
        raise
