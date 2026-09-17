#!/usr/bin/env python3
"""Static and model contracts for P11 OrderedQueue output and float readback."""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "HwaSim_IR/HwaSim_IR/HwaSimIR.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "HwaSim_IR/HwaSim_IR/HwaSimIR.h").read_text(encoding="utf-8")
P6 = (ROOT / "HwaSim_IR/HwaSim_IR/IR/P6GraphicsTest.inl").read_text(encoding="utf-8")
AGC = (ROOT / "HwaSim_IR/HwaSim_IR/IR/IRAgcSampler.inl").read_text(encoding="utf-8")
READBACK = (ROOT / "HwaSim_IR/HwaSim_IR/IR/IRLinearReadback.h").read_text(encoding="utf-8")
STAGE7_TEXTURE = (ROOT / "HwaSim_IR/HwaSim_IR/IR/IRStage7TextureCompat.h").read_text(
    encoding="utf-8"
)
MATERIAL_MAPPER = (ROOT / "HwaSim_IR/HwaSim_IR/IR/IRSceneMaterialMapper.cpp").read_text(
    encoding="utf-8"
)
ACCEPTANCE = (ROOT / "tools/p11_rk3588_band_acceptance.ps1").read_text(encoding="utf-8")


def require(condition: bool, detail: str) -> None:
    if not condition:
        raise AssertionError(detail)


def modeled_outputs(policy: str, inputs: list[int | None]) -> list[int]:
    """Model the run-loop business-frame gate, not the renderer itself."""
    current = 0
    output: list[int] = []
    frame_driven = policy == "OrderedQueue"
    for sample in inputs:
        has_display_frame = sample is not None
        if has_display_frame:
            current = sample
        business_frame_active = not frame_driven or has_display_frame
        if business_frame_active and current:
            output.append(current)
    return output


def main() -> int:
    rows: list[dict[str, object]] = []

    # Bursty input interleaved with idle render ticks must remain one-to-one in
    # OrderedQueue, while Latest deliberately keeps its historic free-running
    # repeated-state presentation contract.
    timeline = [1, None, 2, 3, None, None, 4, None]
    ordered = modeled_outputs("OrderedQueue", timeline)
    latest = modeled_outputs("Latest", timeline)
    require(ordered == [1, 2, 3, 4], f"ordered model duplicated input: {ordered}")
    require(latest == [1, 1, 2, 3, 3, 3, 4, 4], f"Latest contract changed: {latest}")
    rows.append({"case": "modeled_ordered_one_input_one_output", "result": "PASS"})
    rows.append({"case": "modeled_latest_semantics_retained", "result": "PASS"})

    require(CPP.count('m_asyncInputPolicy == "OrderedQueue"') >= 4,
            "frame-driven OrderedQueue is not applied across run/shader/capture")
    require("const bool businessFrameActive = !frameDriven || hasDisplayFrame;" in CPP,
            "run-loop business-frame gate missing")
    require(re.search(r"if \(businessFrameActive\)\s*\{\s*capture_task\(nullptr, this\);", CPP),
            "capture call is not guarded by the business-frame gate")
    require(re.search(r"if \(frameDriven &&\s*!self->m_syncFrameActive\.load\(\)\)", CPP),
            "capture_task lacks defensive OrderedQueue gate")
    require(re.search(r"if \(frameDriven &&\s*\(telemetry\.sourceSeq == 0 \|\|\s*"
                      r"telemetry\.sourceSeq == self->m_lastCapturedSourceSeq\)\)", CPP),
            "frame-driven duplicate guard missing")
    require(re.search(r"if \(frameDriven\)\s*\{\s*self->m_lastCapturedSourceSeq", CPP),
            "OrderedQueue last-captured accounting missing")
    require("m_isSimRunning.load() && !m_pendingDisplayFrames.empty()" in CPP,
            "INIT-to-START realtime samples can be consumed before START")
    require("if (!frameDriven || hasDisplayFrame)" in CPP,
            "OrderedQueue idle presentation ticks can inflate business render counts")
    require("[Stage6 FinalReadbackGate] phase=setup" in CPP and
            "[Stage6 FinalReadbackGate] phase=first_business_frame" in CPP and
            re.search(r"#ifndef _WIN32[\s\S]*?"
                      r"m_asyncInputPolicy == \"OrderedQueue\"[\s\S]*?"
                      r"hasDisplayFrame[\s\S]*?"
                      r"if \(m_stage6FinalCopyRamPending\)[\s\S]*?"
                      r"add_render_texture\([\s\S]*?RTM_copy_ram\);[\s\S]*?"
                      r"m_stage6FinalCopyRamPending = false;[\s\S]*?"
                      r"#endif", CPP),
            "RK final RTM_copy_ram is not attached by the first owned OrderedQueue frame")
    require(CPP.count("m_stage6FinalSensorBuffer->set_active(false);") >= 2,
            "RK final RTM_copy_ram is not dormant in both headless setup paths")
    require("m_stage6FinalCopyRamPending = false;" in HEADER and
            CPP.count("m_stage6FinalCopyRamPending =") >= 5 and
            CPP.count("!m_stage6FinalCopyRamPending)") >= 2,
            "RK final copy-RAM attachment is not explicitly deferred in both setup paths")
    require("frameDrivenReadbackOverride" in CPP and
            "reason=frame_driven_no_pixel_reuse_or_drop" in CPP,
            "frame-driven mode can reuse or discard EveryN/disabled readback frames")
    rows.append({"case": "source_ordered_queue_frame_driven_gate", "result": "PASS"})

    require(CPP.count("m_pTcpThread->setSyncMode(orderedOutput);") >= 2,
            "OrderedQueue no-overwrite policy is not propagated to TCP output")
    require('const bool orderedOutput = isSync || m_asyncInputPolicy == "OrderedQueue";' in CPP,
            "SetRenderMode does not preserve OrderedQueue output ordering")
    require('m_bSyncRenderMode.load() || m_asyncInputPolicy == "OrderedQueue";' in CPP,
            "InitTcpThread does not preserve OrderedQueue output ordering")
    rows.append({"case": "source_ordered_queue_output_no_overwrite", "result": "PASS"})

    require("RTM_triggered_copy_ram" in CPP and "Stage6LinearReadbackTex" in CPP,
            "full-resolution triggered RAM target missing")
    require("m_stage6RawSceneBuffer->trigger_copy();" in P6,
            "selected diagnostic frame is not triggering the copy")
    require("IsP6LinearCaptureRequested(seq)" in P6,
            "diagnostic sequence gate missing")
    require("ramCopyReady" in P6 and "ReadSceneLinearRamImage(m_stage6LinearReadbackTex" in P6,
            "formal SI capture is not fail-closed on the triggered RAM image")
    require("reason=triggered_ram_copy_not_ready" in P6 and "action=no_pfm_written" in P6,
            "missing triggered RAM copy is not recorded as a hard diagnostic failure")
    require("route=gles_rgba_float" in P6 and
            "m_stage6RawSceneTex,m_stage6RawSceneBuffer,width,height,linear,false" in P6,
            "RK3588 selected-frame diagnostic does not read the bound formal SI texture directly")
    require("m_stage6LinearCaptureCompletedSourceSeq" in HEADER and
            "m_stage6LinearCaptureCompletedSourceSeq==seq" in P6,
            "Latest mode can repeatedly overwrite one requested diagnostic sequence")
    read_scene_body = READBACK.split(
        "static bool ReadSceneLinear(GraphicsEngine* engine", 1
    )[1]
    require("ReadSceneLinearRamImage" in READBACK and
            read_scene_body.index("ReadSceneLinearRamImage(texture,width,height,result)") <
            read_scene_body.index("#ifdef _WIN32"),
            "readback does not consume an existing RAM image first")
    require("RTM_copy_ram" in AGC and "RTM_copy_texture" not in
            re.search(r"m_agcSampleBuffer->add_render_texture\([^;]+;", AGC).group(0),
            "small AGC target is not using its RAM copy")
    require("IsP6LinearCaptureRequested(m_currentFrameTelemetry.sourceSeq)" in AGC,
            "LinearDiagnosticSeqs does not force a same-frame AGC statistics sample")
    require("add_render_texture(m_stage6RawSceneTex, GraphicsOutput::RTM_copy_ram)" not in CPP,
            "production 800x800 raw texture was changed to every-frame RAM copy")
    require("add_render_texture(m_stage6RawSceneTex, GraphicsOutput::RTM_bind_or_copy)" in CPP and
            "add_render_texture(m_stage6RawSceneTex, GraphicsOutput::RTM_copy_texture)" not in CPP,
            "production floating raw texture is not directly bound on the GLES path")
    require("const bool formalSiHalfStorage = formalSiDomainRequested;" in CPP and
            "m_stage6RawSceneTex->setup_2d_texture(" in CPP and
            "formalSiDomainRequested ? Texture::F_rgba16 : Texture::F_rgb16" in CPP and
            "m_stage6RawSceneTex->clear_ram_image();" in CPP and
            "phase=post_create_pre_attach" in CPP and
            "ApplyStage6GlesHalfTextureNegotiationWorkaround" in CPP and
            "textureNegotiationProperties.set_float_color(false);" in CPP and
            "eglOutputRecreated=0" in CPP and
            "Panda_rebuild_bitplanes_component_override_avoided" in CPP and
            "phase=post_attach_restore" in CPP and
            "preHasRamImage=" in CPP and "postHasRamImage=" in CPP and
            "Panda_float_color_component_override_avoided" in CPP and
            'formalSiHalfStorage ? "RGBA16F_SI" : "RGBA32F_SI"' in CPP and
            "formalSiHalfStorage ? Texture::T_half_float : Texture::T_float" in CPP,
            "formal SI storage is not restored to GPU-only RGBA16F after Panda negotiation")
    require("quantizationModel=" in CPP and
            "quantizationRelativeErrorBound=" in CPP and
            "quantizationMaxFinite=" in CPP and
            '<< " unit="' in CPP,
            "formal raw storage precision/unit is not auditable")
    require("VerifyStage6RawAttachment();" in CPP and
            "get_rtm_mode(index)" in CPP and
            "[Stage6 RawAttachment]" in CPP and
            "failure=formal_si_attachment_not_direct" in CPP and
            "actualComponentType == Texture::T_half_float" in CPP and
            "actualComponentWidth == 2 && actualComponents == 4" in CPP and
            "textureStorageVerified=" in CPP and
            "m_stage6RawAttachmentChecked" in HEADER and
            "m_stage6RawAttachmentVerified" in HEADER,
            "actual bind-or-copy mode is not verified fail-closed after first render")
    stage7_planner = CPP.split("void HwaSimIR::UpdateStage7SkyHorizon", 1)[1].split(
        "double skyGrayBase", 1
    )[0]
    require("const bool formalSiDomainRequested = IsStage6FormalSiDomainRequested();" in
            stage7_planner and "!formalSiDomainRequested" in stage7_planner,
            "Stage7 weather planner can route formal SI through direct_final")
    require("Texture::T_unsigned_byte,Texture::F_rgb" in AGC and
            "RGB8_UNORM" in AGC and "rawSceneFormatUnchanged=1" in AGC,
            "RK3588 AGC RAM sampler is not the normalized RGB8 compatibility surface")
    require("sourceFrameActive=m_syncFrameActive.load()&&m_currentFrameTelemetry.sourceSeq>0" in AGC,
            "AGC RAM sampler is active before a protocol frame owns valid raw pixels")
    require(re.search(r"Setup can run before START[\s\S]*?"
                      r"m_agcSampleBuffer->set_active\(false\);\s*\n}", AGC),
            "AGC RAM sampler setup is not dormant until a valid protocol frame")
    require("m_agcSampleCopyRamPending=false;" in HEADER and
            "[Stage6 AgcReadbackGate] phase=setup" in AGC and
            "[Stage6 AgcReadbackGate] phase=first_sample_frame" in AGC and
            re.search(r"#ifndef _WIN32[\s\S]*?"
                      r"if\(sampleFrameActive&&m_agcSampleCopyRamPending\)[\s\S]*?"
                      r"add_render_texture\(m_agcSampleTexture,GraphicsOutput::RTM_copy_ram\);[\s\S]*?"
                      r"m_agcSampleCopyRamPending=false;", AGC),
            "RK AGC copy-RAM attachment is not deferred until an owned sample frame")
    require("m_stage6LinearReadbackTex" in HEADER,
            "triggered readback lifetime is not owned by HwaSimIR")
    rows.append({"case": "source_triggered_float_readback_only", "result": "PASS"})

    require("formal_raw_storage" in ACCEPTANCE.replace("-", "_") or
            "p11_rk3588_acceptance_analyze.py" in ACCEPTANCE,
            "acceptance route no longer invokes the strict analyzer")
    analyzer = (ROOT / "tools/p11_rk3588_acceptance_analyze.py").read_text(encoding="utf-8")
    require('gates.add("formal_raw_storage"' in analyzer and
            'gates.add("formal_raw_gles_compatibility"' in analyzer and
            'gates.add("formal_raw_attachment"' in analyzer and
            'gates.add("formal_dual_pass_route"' in analyzer and
            'gates.add("formal_raw_cpu_binary16_quantization"' in analyzer and
            'row["actual_texture_component_type"] == "half_float"' in analyzer and
            'row["actual_texture_component_width"] == 2' in analyzer and
            'metric(line, "textureStorageVerified") == "1"' in analyzer and
            '"acceptance_relative_error_limit": 0.02' in analyzer and
            'quantization_reference_summary["maximum_relative_error"] <= 0.02' in analyzer and
            'row["half_lattice_mismatch_count"] == 0' in analyzer and
            '"half_lattice_max_abs_error"]' in analyzer and
            'len(framebuffer_compat_lines) == 1' in analyzer and
            'len(valid_framebuffer_compat_lines) == 1' in analyzer and
            'metric(line, "preFloatProperty") == "1"' in analyzer and
            'metric(line, "postFloatProperty") == "0"' in analyzer and
            'metric(line, "eglOutputRecreated") == "0"' in analyzer and
            'metric(line, "valid") == "1"' in analyzer and
            'r"\\[Stage6 RawFramebufferCompat\\]\\[ERROR\\]"' in analyzer,
            "acceptance analyzer does not fail closed on storage/route/quantization evidence")

    require('set_ram_image_as(assetVoxels,"RGBA")' not in CPP,
            "3-D RGBA volume still uses Panda's legacy set_ram_image_as conversion")
    require("IRStage7TextureCompat::RgbaToPandaBgra(assetVoxels)" in CPP and
            "texture->set_ram_image(pandaNativeImage);" in CPP,
            "Stage7 3-D RGBA upload does not use explicit Panda-native bytes")
    require("bgra[offset] = rgba[offset + 2u];" in STAGE7_TEXTURE and
            "bgra[offset + 1u] = rgba[offset + 1u];" in STAGE7_TEXTURE and
            "bgra[offset + 2u] = rgba[offset];" in STAGE7_TEXTURE and
            "bgra[offset + 3u] = rgba[offset + 3u];" in STAGE7_TEXTURE,
            "Stage7 compatibility conversion changes channels beyond R/B")
    require("sourceOrder=RGBA ramOrder=BGRA conversion=cpu_explicit" in CPP,
            "Stage7 compatibility route is not auditable in runtime logs")
    rows.append({"case": "source_stage7_3d_rgba_native_upload", "result": "PASS"})

    require("Texture::F_luminance" not in CPP and
            "Texture::F_luminance" not in MATERIAL_MAPPER,
            "material-id texture still requests legacy GLES luminance")
    require("Texture::T_unsigned_byte,Texture::F_red" in CPP and
            "materialIdTexture->set_format(Texture::F_red);" in MATERIAL_MAPPER,
            "fallback and asset material-id textures are not GLES-native R8")
    require("[MaterialIdTexture] name=EmptyMaterialId" in CPP and
            "[MaterialIdTexture] name=asset" in MATERIAL_MAPPER and
            "reason=gles3_legacy_luminance_unsupported" in CPP and
            "reason=gles3_legacy_luminance_unsupported" in MATERIAL_MAPPER,
            "material-id GLES compatibility route is not auditable")
    rows.append({"case": "source_material_id_r8_gles3_upload", "result": "PASS"})

    require('"--acceptance-exit-ms=$(($DurationSec + 40) * 1000)"' in ACCEPTANCE and
            '"--acceptance-exit-ms=$(($DurationSec + 120) * 1000)"' not in ACCEPTANCE,
            "DDS receiver acceptance timer cannot expire inside the runner wait")
    post_producer_wait = ACCEPTANCE.rsplit("Start-Sleep -Seconds 5", 1)[1].split(
        "$receiverExit = Get-ExitedProcessCode", 1
    )[0]
    require("WaitForExit(30000)" in post_producer_wait and
            "$receiver.CloseMainWindow()" not in post_producer_wait,
            "DDS receiver is not given a bounded normal Qt shutdown/ledger flush")
    rows.append({"case": "tool_receiver_normal_exit_before_cleanup", "result": "PASS"})

    report = ROOT / "logs/p11/tests/rk3588_frame_contract.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps({"result": "PASS", "tests": rows}, indent=2) + "\n",
                      encoding="utf-8")
    print(json.dumps({"result": "PASS", "report": str(report), "tests": len(rows)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
