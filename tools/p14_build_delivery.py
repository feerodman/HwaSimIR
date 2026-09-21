#!/usr/bin/env python3
"""Build the self-contained P14 delivery directory and archive.

The script deliberately packages only the verified deployment closure and final
evidence cases.  It excludes superseded/failed media and the multi-gigabyte raw
MODTRAN work directories; their signed publication/QC manifests are included.
"""

from __future__ import annotations

import hashlib
import html
import json
import shutil
import subprocess
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DELIVERABLES = ROOT / "deliverables"
OUT = DELIVERABLES / "HwaSimIR_P14_Delivery"
ARCHIVE = DELIVERABLES / "HwaSimIR_P14_Delivery.zip"
RECEIPT = DELIVERABLES / "HwaSimIR_P14_Delivery_receipt.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def checked_source(relative: str) -> Path:
    source = (ROOT / relative).resolve()
    if not source.exists():
        raise FileNotFoundError(relative)
    if ROOT not in source.parents and source != ROOT:
        raise RuntimeError(f"source escaped repository: {source}")
    return source


def destination(relative: str) -> Path:
    target = (OUT / relative).resolve()
    if OUT not in target.parents and target != OUT:
        raise RuntimeError(f"destination escaped delivery root: {target}")
    return target


def copy_file(source_relative: str, target_relative: str | None = None) -> Path:
    source = checked_source(source_relative)
    target = destination(target_relative or source_relative)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
    if sha256(source) != sha256(target):
        raise RuntimeError(f"copy hash mismatch: {source_relative}")
    return target


def copy_tree(source_relative: str, target_relative: str | None = None,
              ignore=None) -> Path:
    source = checked_source(source_relative)
    target = destination(target_relative or source_relative)
    if target.exists():
        raise RuntimeError(f"duplicate delivery target: {target}")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, target, ignore=ignore, copy_function=shutil.copy2)
    return target


def copy_windows_runtime(source_relative: str, target_relative: str,
                         excluded_directories: set[str]) -> None:
    source = checked_source(source_relative)
    target = destination(target_relative)
    target.mkdir(parents=True, exist_ok=True)
    debug_suffixes = {".o", ".obj", ".iobj", ".ipdb", ".pdb"}
    ignored_files = {"DataDrivenTestQT.exe.ddslog", "HwaSim_IR_VideoDisplay.exe.ddslog",
                     "moc_mainwindow.cpp", "moc_predefs.h"}
    for child in source.iterdir():
        if child.name in excluded_directories:
            continue
        if child.is_file():
            if child.suffix.lower() in debug_suffixes or child.name in ignored_files:
                continue
            if child.name.startswith("ordinary_demo_1km"):
                continue
            if child.name.startswith("NetworkConfig - ") or ".before_" in child.name:
                continue
            shutil.copy2(child, target / child.name)
        elif child.is_dir():
            shutil.copytree(child, target / child.name, copy_function=shutil.copy2)


CASES = [
    ("original_swir", "p14_final4_original_1_swir_clear", "原 1.txt · SWIR · 晴天"),
    ("original_mwir", "p14_final4_original_1_mwir_clear", "原 1.txt · MWIR · 晴天"),
    ("ground_swir_clear", "p14_final4_ground_swir_clear", "民用地面 · SWIR · 晴天"),
    ("ground_swir_cloud_visibility", "p14_final4c_ground_swir_cloud_visibility", "民用地面 · SWIR · 云/12 km 能见度"),
    ("ground_swir_rain", "p14_final4_ground_swir_rain", "民用地面 · SWIR · 雨"),
    ("ground_swir_snow", "p14_final4_ground_swir_snow", "民用地面 · SWIR · 雪"),
    ("ground_mwir_clear", "p14_final4_ground_mwir_clear", "民用地面 · MWIR · 晴天"),
    ("ground_mwir_cloud_visibility", "p14_final4_ground_mwir_cloud_visibility", "民用地面 · MWIR · 云/12 km 能见度"),
    ("ground_mwir_rain", "p14_final4_ground_mwir_rain", "民用地面 · MWIR · 雨"),
    ("ground_mwir_snow", "p14_final4_ground_mwir_snow", "民用地面 · MWIR · 雪"),
    ("mwir_snow_300s", "p14_final4_mwir_snow_300s", "MWIR · 雪 · 300 s 长测"),
    ("plume_mwir_on", "p14_final4_plume_truck_mwir_on_raw", "民用通用热源 · MWIR · On"),
    ("plume_mwir_off", "p14_final4_plume_truck_mwir_off_raw", "民用通用热源 · MWIR · Off"),
    ("first_valid_swir", "p14_final5_first_valid_swir_r2", "外部首有效位置边界 · SWIR"),
    ("first_valid_mwir", "p14_final5_first_valid_mwir", "外部首有效位置边界 · MWIR"),
]


def locate_mp4(case_target: Path) -> Path:
    matches = sorted(case_target.glob("recording/**/output.mp4"))
    if len(matches) != 1:
        raise RuntimeError(f"expected one MP4 in {case_target}, found {len(matches)}")
    if matches[0].stat().st_size <= 0:
        raise RuntimeError(f"empty MP4: {matches[0]}")
    return matches[0]


def ffprobe_video(path: Path) -> dict:
    probe = ROOT / ".deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffprobe.exe"
    if not probe.exists():
        raise RuntimeError("bundled ffprobe is missing")
    completed = subprocess.run(
        [str(probe), "-v", "error", "-select_streams", "v:0",
         "-show_entries", "stream=codec_name,width,height,avg_frame_rate:format=duration",
         "-of", "json", str(path)],
        check=True, capture_output=True, text=True, encoding="utf-8")
    payload = json.loads(completed.stdout)
    streams = payload.get("streams", [])
    if len(streams) != 1:
        raise RuntimeError(f"no unique video stream: {path}")
    stream = streams[0]
    if stream.get("codec_name") != "h264" or stream.get("width") != 800 or stream.get("height") != 800:
        raise RuntimeError(f"unexpected video identity: {path}: {stream}")
    return {
        "codec": stream["codec_name"],
        "width": stream["width"],
        "height": stream["height"],
        "avgFrameRate": stream.get("avg_frame_rate"),
        "durationSeconds": float(payload.get("format", {}).get("duration", 0.0)),
    }


def full_decode_video(path: Path) -> dict:
    ffmpeg = ROOT / ".deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe"
    if not ffmpeg.exists():
        raise RuntimeError("bundled ffmpeg is missing")
    completed = subprocess.run(
        [str(ffmpeg), "-v", "error", "-i", str(path), "-map", "0:v:0",
         "-c:v", "rawvideo", "-pix_fmt", "gray", "-f", "rawvideo", "-y", "NUL"],
        check=False, capture_output=True, text=True, encoding="utf-8", errors="replace")
    diagnostics = [line for line in completed.stderr.splitlines() if line.strip()]
    if completed.returncode != 0:
        raise RuntimeError(f"full MP4 decode failed ({completed.returncode}): {path}")
    return {
        "exitCode": completed.returncode,
        "diagnosticLineCount": len(diagnostics),
        "firstDiagnostic": diagnostics[0] if diagnostics else None,
        "sink": "decoded_gray8_rawvideo_to_null_device",
    }


def packet_timestamp_audit(path: Path) -> dict:
    probe = ROOT / ".deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffprobe.exe"
    completed = subprocess.run(
        [str(probe), "-v", "error", "-select_streams", "v:0", "-show_packets",
         "-show_entries", "packet=pts,dts,duration", "-of", "csv=p=0", str(path)],
        check=True, capture_output=True, text=True, encoding="utf-8", errors="replace")
    previous_pts = None
    previous_dts = None
    pts_duplicates = pts_regressions = dts_duplicates = dts_regressions = 0
    packet_count = 0
    for line in completed.stdout.splitlines():
        fields = line.strip().split(",")
        if len(fields) < 2 or fields[0] in {"", "N/A"} or fields[1] in {"", "N/A"}:
            raise RuntimeError(f"packet missing PTS/DTS: {path}: {line}")
        pts = int(fields[0])
        dts = int(fields[1])
        if previous_pts is not None:
            pts_duplicates += int(pts == previous_pts)
            pts_regressions += int(pts < previous_pts)
            dts_duplicates += int(dts == previous_dts)
            dts_regressions += int(dts < previous_dts)
        previous_pts, previous_dts = pts, dts
        packet_count += 1
    result = {
        "packetCount": packet_count,
        "ptsDuplicates": pts_duplicates,
        "ptsRegressions": pts_regressions,
        "dtsDuplicates": dts_duplicates,
        "dtsRegressions": dts_regressions,
    }
    if packet_count == 0 or any(result[key] for key in
                                ("ptsDuplicates", "ptsRegressions", "dtsDuplicates", "dtsRegressions")):
        raise RuntimeError(f"MP4 packet timestamp audit failed: {path}: {result}")
    return result


def write_index(video_records: list[dict]) -> None:
    image_names = [
        ("original_1_visibility_transition.png", "原 1.txt 显示边界"),
        ("first_valid_boundary_2band.png", "外部首有效位置，两波段"),
        ("weather_received_2band.png", "云雨雪 received，两波段"),
        ("weather_fixed_mapping_2band.png", "云雨雪固定映射，两波段"),
        ("plume_root_cause_and_signed_raw.png", "尾焰根因与带符号 raw"),
        ("target_type_ui_click.png", "目标类型 UI 实际点击"),
    ]
    cards = []
    for record in video_records:
        rel = record["path"]
        cards.append(
            '<article class="card"><h3>{}</h3><video controls preload="metadata" src="{}"></video>'
            '<p>{:.3f} s · 800×800 H.264 · <a href="{}">打开 MP4</a> · '
            '<a href="{}">case_result</a> · <a href="{}">media_qc</a></p></article>'.format(
                html.escape(record["title"]), html.escape(rel), record["probe"]["durationSeconds"],
                html.escape(rel), html.escape(record["caseResult"]), html.escape(record["mediaQc"])))
    figures = []
    for name, caption in image_names:
        rel = f"media/images/{name}"
        figures.append(f'<figure><a href="{rel}"><img src="{rel}" alt="{html.escape(caption)}"></a><figcaption>{html.escape(caption)}</figcaption></figure>')
    page = f"""<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HwaSimIR P14 交付入口</title><style>
body{{font-family:system-ui,'Microsoft YaHei',sans-serif;margin:0;background:#111827;color:#e5e7eb}}main{{max-width:1320px;margin:auto;padding:28px}}
h1,h2{{color:#f9fafb}}a{{color:#67e8f9}}.notice{{background:#1f2937;border-left:4px solid #22c55e;padding:14px 18px;border-radius:8px}}
.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(360px,1fr));gap:18px}}.card,figure{{background:#1f2937;padding:14px;border-radius:10px;margin:0}}
video,img{{width:100%;height:auto;background:#000;border-radius:6px}}code{{color:#fde68a}}.links a{{margin-right:18px;line-height:2}}
</style></head><body><main><h1>HwaSimIR P14 最终交付</h1>
<div class="notice">最终工程验收：PASS_WITH_DISCLOSED_OPEN_ITEMS。真实标定状态：<code>NOT_VERIFIED_CALIBRATION</code>。原 <code>1.txt</code> 未修改；普通回放前 659 行按源 <code>ViewValid=0</code> 隐藏，第 660 行开始显示。</div>
<h2>文档与完整性</h2><p class="links"><a href="docs/HwaSimIR_P14_Closeout.md">收口报告</a><a href="docs/HwaSimIR_P14_ExternalInput_FirstValidReference.md">首有效位置参考</a><a href="docs/HwaSimIR_P14_Ordinary_Runbook.md">运行手册</a><a href="docs/HwaSimIR_P14_Effects_Radiance_Contract.md">辐射度合成契约</a><a href="docs/HwaSimIR_P14_Issue_Ledger.csv">问题台账</a><a href="status/final_status.json">机器状态</a><a href="manifest.json">文件 manifest</a><a href="SHA256SUMS.txt">SHA-256</a></p>
<h2>最终版本真实 DDS 视频</h2><div class="grid">{''.join(cards)}</div>
<h2>关键图片</h2><div class="grid">{''.join(figures)}</div>
<h2>证据结构</h2><p><code>media/cases</code> 含 MP4、received H.264、raw PFM、固定映射/AGC、标注、frame index、事件表和 QC；<code>evidence</code> 含输入审计、大气发布、首包 fixture、UI、生命周期、构建、部署和回滚；<code>deployment</code> 含板端 ELF、Windows 运行闭包、外置 shader、LUT/manifest 和民用资产。</p>
</main></body></html>"""
    target = destination("index.html")
    target.write_text(page, encoding="utf-8", newline="\n")


def build() -> None:
    DELIVERABLES.mkdir(parents=True, exist_ok=True)
    expected_parent = DELIVERABLES.resolve()
    if OUT.resolve().parent != expected_parent or OUT.name != "HwaSimIR_P14_Delivery":
        raise RuntimeError("unsafe delivery directory")
    if OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir()
    if ARCHIVE.exists():
        ARCHIVE.unlink()
    if RECEIPT.exists():
        RECEIPT.unlink()

    docs = [
        "docs/HwaSimIR_P14_Closeout.md",
        "docs/HwaSimIR_P14_ExternalInput_FirstValidReference.md",
        "docs/HwaSimIR_P14_Ordinary_Runbook.md",
        "docs/HwaSimIR_P14_Effects_Radiance_Contract.md",
        "docs/HwaSimIR_P14_Issue_Ledger.csv",
        "docs/HwaSimIR_P14_Codex_Prompt.md",
        "docs/HwaSimIR_P14_P13_Audit.md",
    ]
    for item in docs:
        copy_file(item, f"docs/{Path(item).name}")
    copy_file("final_status.json", "status/final_status.json")

    inputs = [
        "DataDrivenTestQT/1.txt",
        "DataDrivenTestQT/p14_ground_truck_weather_30s.txt",
        "DataDrivenTestQT/p14_ground_truck_weather_30s.txt.json",
        "DataDrivenTestQT/p13_performance_highalt_300s.txt",
        "DataDrivenTestQT/p13_performance_highalt_300s.txt.json",
    ]
    for item in inputs:
        copy_file(item, f"inputs/{Path(item).name}")

    copy_file("logs/p14/build/stop_drain_final/HwaSim_IR.aarch64", "deployment/board/HwaSim_IR.aarch64")
    copy_file("logs/p14/build/first_valid_fixture_final_v2/HwaSimIRP14FirstValidFixture.aarch64", "deployment/board/HwaSimIRP14FirstValidFixture.aarch64")
    copy_file("logs/p14/build/first_valid_fixture_final_v2/source.tar", "deployment/board/HwaSimIRP14FirstValidFixture.source.tar")
    copy_file("HwaSim_IR/Bin/HwaSim_IR.exe", "deployment/windows/HwaSim_IR/HwaSim_IR.exe")
    copy_windows_runtime("build-DataDrivenTestQT-codex-mingw73_64-Release/release",
                         "deployment/windows/DataDrivenTestQT", set())
    copy_windows_runtime("HwaSim_IR_VideoDisplay/x64/Release",
                         "deployment/windows/HwaSim_IR_VideoDisplay", {"MP4"})

    config_files = [
        "HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini",
        "HwaSim_IR/Bin/Config/NetworkConfig.ini",
        "HwaSim_IR/Bin/Config/NetworkConfig_precise.ini",
        "HwaSim_IR/Bin/Config/NetworkConfig_coarse.ini",
        "HwaSim_IR/Bin/Config/TargetLib/Targets.json",
        "HwaSim_IR/Bin/Config/Annotation/annotation_profiles.json",
        "HwaSim_IR/Bin/Config/Materials/MaterialBandOptics.csv",
        "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv",
        "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/p14_coverage_manifest.json",
        "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv",
    ]
    for item in config_files:
        relative = Path(item).relative_to("HwaSim_IR/Bin/Config")
        copy_file(item, f"deployment/board/Config/{relative.as_posix()}")
    for item in ["DDS", "GameVFX", "Weather", "IRPlume", "IRHotspots", "IRRadiance", "Effects", "SensorWave"]:
        copy_tree(f"HwaSim_IR/Bin/Config/{item}", f"deployment/board/Config/{item}")
    copy_tree("HwaSim_IR/Bin/Config/TargetLib/p11/civil_van",
              "deployment/board/Config/TargetLib/p11/civil_van")

    copy_tree("logs/p14/deploy_stop_drain_final_r4", "evidence/deployment")
    copy_tree("logs/p14/rollback_final2/p14rb-20260921-final2", "evidence/rollback")
    copy_tree("logs/p14/verification", "evidence/verification")
    copy_tree("logs/p14/original_input_audit_final", "evidence/original_input_audit")
    copy_tree("logs/p14/ground_fixture_query", "evidence/ground_fixture_query")
    copy_tree("logs/p14/lifecycle_final4", "evidence/lifecycle")
    copy_tree("logs/p14/ui", "evidence/ui")
    copy_tree("logs/p14/plume_signed_raw_qc_final4", "evidence/plume_signed_raw")
    copy_file("logs/p12/p12a/ordinary-mixed-rk-20260917-200737/board_hwa.log",
              "evidence/plume_historical_reproduction/p12_board_hwa.log")
    copy_file("logs/p14/p14_first_valid_fixture_final5_mwir_check.json",
              "evidence/first_valid/p14_first_valid_fixture_final5_mwir_check.json")
    for fixture_log in sorted((ROOT / "logs/p14").glob("p14_first_valid_fixture_final5*")):
        if fixture_log.is_file():
            copy_file(fixture_log.relative_to(ROOT).as_posix(), f"evidence/first_valid/{fixture_log.name}")

    atmosphere_root = "logs/p14/atmosphere/highalt_vis23"
    atmosphere_files = [
        "README.md", "case_manifest.csv", "case_manifest.json", "engine_and_license_evidence.json",
        "formal_p14mix_rows.csv", "generation_provenance.json", "installation_restore_evidence.csv",
        "modout1_humidity_metrics.csv", "qc_results.csv", "qc_results.json", "run_manifest.csv",
        "spectral_qc.csv", "vertex_metrics.csv",
    ]
    for name in atmosphere_files:
        copy_file(f"{atmosphere_root}/{name}", f"evidence/atmosphere/{name}")
    copy_tree(f"{atmosphere_root}/formal_publish", "evidence/atmosphere/formal_publish")

    copy_tree("logs/p14/images_final", "media/images")
    video_records = []
    for slug, source_case, title in CASES:
        case_target = copy_tree(f"logs/p14/runs/{source_case}", f"media/cases/{slug}")
        mp4 = locate_mp4(case_target)
        probe = ffprobe_video(mp4)
        full_decode = full_decode_video(mp4)
        packet_timestamps = packet_timestamp_audit(mp4)
        relative_mp4 = mp4.relative_to(OUT).as_posix()
        case_result = (case_target / "case_result.json").relative_to(OUT).as_posix()
        media_qc_file = case_target / "media_qc.json"
        media_qc = media_qc_file.relative_to(OUT).as_posix() if media_qc_file.exists() else case_result
        video_records.append({
            "slug": slug,
            "title": title,
            "path": relative_mp4,
            "sha256": sha256(mp4),
            "bytes": mp4.stat().st_size,
            "caseResult": case_result,
            "mediaQc": media_qc,
            "probe": probe,
            "fullDecode": full_decode,
            "packetTimestamps": packet_timestamps,
        })

    source_files = [
        "DDS/Protocol/RealtimeSampleValidity.h",
        "DDS/Protocol/DdsStimClient.cpp", "DDS/Protocol/DdsStimClient.h",
        "DDS/Protocol/tests/realtime_sample_validity_test.cpp", "DDS/Protocol/tests/CMakeLists.txt",
        "DDS/HwaSimIRP14FirstValidFixture/main.cpp", "DDS/HwaSimIRP14FirstValidFixture/CMakeLists.txt",
        "DDS/HwaSimIRP14FirstValidFixture/README.md",
        "DataDrivenTestQT/mainwindow.cpp", "DataDrivenTestQT/mainwindow.h", "DataDrivenTestQT/DataDrivenTestQT.pro",
        "DataDrivenTestQT/NetworkConfig.ini", "DataDrivenTestQT/tests/p14_ui_interaction_test.cpp",
        "HwaSim_IR/HwaSim_IR/HwaSimIR.cpp", "HwaSim_IR/HwaSim_IR/HwaSimIR.h",
        "HwaSim_IR/HwaSim_IR/IR/IRGameSpriteBatch.h", "HwaSim_IR/HwaSim_IR/IR/IRPrecipitationBatch.h",
    ]
    for item in source_files:
        copy_file(item, f"source/{item}")
    for tool in sorted((ROOT / "tools").glob("p14_*")):
        if tool.is_file():
            copy_file(tool.relative_to(ROOT).as_posix(), f"source/tools/{tool.name}")
    for name in ["stage0_build.ps1", "stage0_check.ps1", "stage3_modtran_tau_loader_check.ps1",
                 "stage4_hotspot_check.ps1", "stage5_plume_check.ps1", "stage7_weather_check.ps1",
                 "rk3588_deploy_atomic.ps1", "rk3588_deploy_elf_verified_config.ps1"]:
        copy_file(f"tools/{name}", f"source/tools/{name}")

    write_index(video_records)

    files_before_manifest = sorted(p for p in OUT.rglob("*") if p.is_file())
    manifest = {
        "schema": "HwaSimIR.P14.DeliveryManifest.1",
        "createdUtc": datetime.now(timezone.utc).isoformat(),
        "result": "PASS",
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
        "originalInputSha256": "f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901",
        "finalElfSha256": "80267b01054d9fe8e8ef2c3fd8f3e726003794dafc6cc1086958e9713f18841c",
        "formalLutSha256": "48432459ac56dbb6980f3ca25e2d1e4701763ba3674703051cb0fd45da8c7cd7",
        "coverageManifestSha256": "f5d27c304b663218b94c9e45bd2bbbb968de1a66695f551ba5c4633f9c4c5d7b",
        "videoCount": len(video_records),
        "videos": video_records,
        "files": [
            {"path": p.relative_to(OUT).as_posix(), "bytes": p.stat().st_size, "sha256": sha256(p)}
            for p in files_before_manifest
        ],
    }
    manifest_path = destination("manifest.json")
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    sums_files = sorted(p for p in OUT.rglob("*") if p.is_file() and p.name != "SHA256SUMS.txt")
    sums_path = destination("SHA256SUMS.txt")
    sums_path.write_text("".join(f"{sha256(p)}  {p.relative_to(OUT).as_posix()}\n" for p in sums_files),
                         encoding="utf-8", newline="\n")

    required = [
        destination("index.html"), destination("manifest.json"), destination("SHA256SUMS.txt"),
        destination("deployment/board/HwaSim_IR.aarch64"),
        destination("deployment/board/Config/GameVFX/sprite.frag"),
        destination("deployment/board/Config/Weather/precipitation.frag"),
        destination("deployment/board/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"),
        destination("deployment/board/Config/Atmosphere/MODTRAN/processed/p14_coverage_manifest.json"),
        destination("inputs/1.txt"),
    ]
    for item in required:
        if not item.is_file() or item.stat().st_size == 0:
            raise RuntimeError(f"required delivery file missing/empty: {item}")

    with zipfile.ZipFile(ARCHIVE, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=1,
                         allowZip64=True) as archive:
        for item in sorted(p for p in OUT.rglob("*") if p.is_file()):
            archive.write(item, f"{OUT.name}/{item.relative_to(OUT).as_posix()}")

    with zipfile.ZipFile(ARCHIVE, "r") as archive:
        bad = archive.testzip()
        names = archive.namelist()
        if bad is not None:
            raise RuntimeError(f"zip CRC failure: {bad}")
        if not any(name.lower().endswith(".mp4") for name in names):
            raise RuntimeError("archive contains no MP4")

    receipt = {
        "schema": "HwaSimIR.P14.DeliveryReceipt.1",
        "createdUtc": datetime.now(timezone.utc).isoformat(),
        "result": "PASS",
        "directory": str(OUT),
        "directoryFiles": len(list(p for p in OUT.rglob("*") if p.is_file())),
        "directoryBytes": sum(p.stat().st_size for p in OUT.rglob("*") if p.is_file()),
        "archive": str(ARCHIVE),
        "archiveBytes": ARCHIVE.stat().st_size,
        "archiveSha256": sha256(ARCHIVE),
        "archiveEntries": len(names),
        "archiveCrcTest": "PASS",
        "mp4Count": len(video_records),
        "mp4Probe": "PASS_H264_800x800",
        "mp4FullDecode": "PASS_15_OF_15",
        "mp4PacketTimestamps": "PASS_STRICT_PTS_DTS_15_OF_15",
        "timestampValidationNote": "Decode uses a rawvideo sink; ffprobe independently proves strict input packet PTS/DTS. The ffmpeg null muxer was excluded because its output-timebase quantization can create diagnostic-only duplicate DTS warnings.",
        "originalInputSha256": sha256(destination("inputs/1.txt")),
        "manifestSha256": sha256(manifest_path),
        "sha256SumsSha256": sha256(sums_path),
    }
    RECEIPT.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(receipt, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    try:
        build()
    except Exception as exc:
        print(f"P14 delivery build failed: {exc}", file=sys.stderr)
        raise
