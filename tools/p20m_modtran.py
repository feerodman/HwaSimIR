#!/usr/bin/env python3
"""P20M isolated official MODTRAN execution, per-case identity and QC.

python tools/p20m_modtran.py prepare
python tools/p20m_modtran.py run --resume
python tools/p20m_modtran.py qc

Pilot results are development evidence, never a production coverage claim.
"""
from __future__ import annotations
import argparse
import collections
import contextlib
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

import p13_modtran_grid as grid
import p11_modtran_humidity_grid as humidity
import p11_modtran_solar_heating_grid as heating
import p11_modtran_humidity_qc as metrics
import modtran_convert_to_si as convert
from modtran_flux import parse_flux_at_target
from p20m2_geometry_qc import canonical_case, actual_geometry

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "logs/p20m/20261021_full_domain"
BIN = Path("F:/Programs/PcModWin5/Bin")
QC_FILES = ("p20m_modtran.py", "modtran_convert_to_si.py", "modtran_flux.py",
            "p11_modtran_humidity_qc.py", "p20m2_geometry_qc.py")
OUTPUTS = {"tape6": "MODOUT1.txt", "tape7": "MODOUT2.txt",
           "tape8": "MODOUT3.txt", "specflux": "spectral_flux.flx",
           "tape7.scn": "tape7.scn", "warnings.txt": "warnings.txt"}


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024*1024), b""):
            h.update(chunk)
    return h.hexdigest()


def atomic_json(path, value):
    path = Path(path)
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False),
                    encoding="utf-8")
    os.replace(temp, path)


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def database_deck(lines):
    """Insert documented CARD 1A4 after optional 1A1/1A2/1A3."""
    lines = list(lines)
    card = list(lines[1])
    insert = 2 + sum(card[i].upper() in ("T", "2", "4") for i in (41, 43, 45))
    if card[49].upper() == "T":
        raise ValueError("Deck already defines CDTDIR")
    card[49] = "T"
    lines[1] = "".join(card)
    lines.insert(insert, BIN.joinpath("Data").as_posix() + "/")
    return lines


def prepare(root):
    cases = []

    def add(name, mode, band, lines, **parameters):
        case = dict(case_id=name, mode=mode, band=band, **parameters)
        folder = root / "generated" / name
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / "input.tp5"
        content = "\n".join(database_deck(lines)) + "\n"
        if path.exists() and path.read_text(encoding="ascii") != content:
            raise ValueError(f"Immutable deck differs: {path}; use a new task directory")
        path.write_text(content, encoding="ascii")
        case.update(input_file=str(path), input_sha256=digest(path))
        cases.append(case)

    official = BIN.parent / "Usr/1500mGround.ltn"
    add("official_1500mGround", "OfficialTransmittance", "official",
        official.read_text(encoding="ascii").splitlines(),
        source_file=str(official), source_sha256=digest(official))
    # Preserve the successful P13 settings as an explicit comparison case.
    add("project_p13_success", "Transmittance", "MWIR",
        grid.los_lines("MWIR", 0, 11.5, 9.9, 20, 6, 85),
        observer=11.5, target=9.9, range=20, rh=85, visibility=6)
    geometries = [
        (3, 3, .0001), (3, 3, .001), (3, 3.0001, .0001),
        (10, 10.0005, .001), (20, 20, .001),
        (20, 19.999, .001), (3, 20, 17), (20, 3, 17),
        (3, 20, 50), (20, 3, 50), (3, 3, 50), (20, 20, 50),
        (9.913, 12.7073, 20),
    ]
    for band in ("SWIR", "MWIR"):
        for i, (obs, tar, distance) in enumerate(geometries):
            for mode, iemsct in (("Transmittance", 0), ("ThermalRadiance", 1)):
                add(f"geometry_{band}_{i:02d}_{iemsct}", mode, band,
                    grid.los_lines(band, iemsct, obs, tar, distance, 6, 85),
                    observer=obs, target=tar, range=distance, rh=85, visibility=6)
        for sza in (0, 73.318, 88, 90, 92, 94, 96, 100, 120, 180):
            for mode in ("DirectSolarIrradiance", "SpectralFlux"):
                maker = humidity.solar_lines if mode == "DirectSolarIrradiance" else humidity.flux_lines
                add(f"sun_{band}_{sza:g}_{mode}", mode, band, maker(band, 20, 6, sza, 85),
                    target=20, rh=85, visibility=6, sza=sza,
                    template="historical_p11_for_audit_not_harmonized")
        for azimuth in (-180, -90, 0, 45, 90, 135, 180):
            lines = grid.scattering_lines(band, 9.913, 12.7073, 20, 6, 73.318, 85)
            lines[5] = f"{azimuth:10.3f}{73.318:10.3f}" + f"{0:10.3f}"*6
            add(f"azimuth_{band}_{azimuth:g}", "RadianceWithScattering", band, lines,
                observer=9.913, target=12.7073, range=20, rh=85, visibility=6,
                sza=73.318, relative_azimuth=azimuth)
    for alt in (3, 10, 12.7073, 20):
        for mode, maker in (("DirectSolarIrradiance", heating.direct_lines),
                            ("SpectralFlux", heating.flux_lines)):
            add(f"heating_{alt:g}_{mode}", mode, "SOLAR_SHORTWAVE",
                maker(alt, 6, 73.318, humidity.humidity_profile(85), 85),
                target=alt, rh=85, visibility=6, sza=73.318)
    manifest = dict(schema="HwaSimIR.P20M.Cases.1", dataset_id=root.name,
                    status="DEVELOPMENT_PILOT_NOT_RUNTIME_DATA", cases=cases)
    path = root / "case_manifest.json"
    if path.exists() and read_json(path) != manifest:
        raise ValueError("Case manifest immutable: choose a new task directory")
    atomic_json(path, manifest)
    print(f"Prepared {len(cases)} official/development component cases", flush=True)


def prepare_repair(root):
    """Independent geometry experiments; never overwrite original failed pilots."""
    cases = []
    for band in ("SWIR", "MWIR"):
        for altitude in (3, 10, 20):
            for distance in (.0001, .001, .01, .1):
                for method in ("horizontal_itype1", "angle_range_2b"):
                    for mode, iemsct in (("Transmittance", 0), ("ThermalRadiance", 1)):
                        lines = grid.los_lines(band, iemsct, altitude, altitude, distance, 6, 85)
                        if method == "horizontal_itype1":
                            lines[0] = lines[0][:6] + f"{1:4d}" + lines[0][10:]
                        else:
                            angle = math.degrees(math.acos(-distance/(2*(6371.23+altitude))))
                            lines[3] = (f"{altitude:10.6f}{0:10.6f}{angle:10.6f}"
                                        f"{distance:10.7f}" + lines[3][40:])
                        name = f"{band}_alt{altitude}_range{distance:g}_{method}_{iemsct}"
                        folder = root/"generated"/name
                        folder.mkdir(parents=True, exist_ok=True)
                        path = folder/"input.tp5"
                        text = "\n".join(database_deck(lines))+"\n"
                        if path.exists() and path.read_text() != text:
                            raise ValueError("Immutable repair deck differs: "+str(path))
                        path.write_text(text, encoding="ascii")
                        cases.append(dict(case_id=name, mode=mode, band=band, target=altitude,
                                          observer=altitude, range=distance,
                                          geometry_method=method, rh=85, visibility=6,
                                          input_file=str(path), input_sha256=digest(path)))
    manifest = dict(schema="HwaSimIR.P20M.Cases.1", dataset_id=root.name,
                    status="GEOMETRY_REPAIR_DEVELOPMENT_ONLY", cases=cases)
    path = root/"case_manifest.json"
    if path.exists() and read_json(path) != manifest:
        previous = read_json(path)
        previous_normalized = dict(previous, cases=[canonical_case(c) for c in previous["cases"]])
        new_normalized = dict(manifest, cases=[canonical_case(c) for c in cases])
        if previous_normalized != new_normalized:
            raise ValueError("Immutable repair manifest differs")
        print("Existing legacy repair manifest retained; QC canonicalizes field names", flush=True)
        return
    atomic_json(path, manifest)
    print(f"Prepared {len(cases)} geometry repair experiments", flush=True)


def prepare_solar_repair(root):
    cases = []
    for band in ("SWIR", "MWIR"):
        for altitude in (3, 20):
            for sza in (73.318, 88, 90, 92, 94, 96, 100, 180):
                lines = humidity.flux_lines(band, altitude, 6, sza, 85)
                # MODTRAN 5.2.1 manual identifies spherical refractive
                # geometry support in the ISAACS multiple-scattering solver.
                card = list(lines[1])
                card[0] = "f"
                card[10:20] = f"{360:10.3f}"
                card[30:40] = f"{0:10.6f}"
                lines[1] = "".join(card)
                lines[6] = "    2    2    0    0"  # mean Earth-Sun distance
                name = f"{band}_alt{altitude}_sza{sza:g}_isaacs_flux"
                folder = root/"generated"/name
                folder.mkdir(parents=True, exist_ok=True)
                path = folder/"input.tp5"
                text = "\n".join(database_deck(lines))+"\n"
                if path.exists() and path.read_text() != text:
                    raise ValueError("Immutable solar experiment deck differs")
                path.write_text(text, encoding="ascii")
                cases.append(dict(case_id=name, mode="SpectralFlux", band=band,
                                  target=altitude, sza=sza, rh=85, visibility=6,
                                  scattering_solver="ISAACS", day=0, co2_ppmv=360,
                                  input_file=str(path), input_sha256=digest(path)))
    manifest = dict(schema="HwaSimIR.P20M.Cases.1", dataset_id=root.name,
                    status="SOLAR_REPAIR_DEVELOPMENT_ONLY", cases=cases)
    path = root/"case_manifest.json"
    if path.exists() and read_json(path) != manifest:
        raise ValueError("Immutable solar manifest differs")
    atomic_json(path, manifest)
    print(f"Prepared {len(cases)} spherical solar solver experiments", flush=True)


def identity():
    db = [{"path": p.relative_to(BIN/"Data").as_posix(),
           "bytes": p.stat().st_size, "sha256": digest(p)}
          for p in sorted((BIN/"Data").rglob("*")) if p.is_file()]
    result = dict(engine=str(BIN/"Mod5.2.1.0.exe"),
                  engine_sha256=digest(BIN/"Mod5.2.1.0.exe"),
                  database=db, engine_version="5.2.1 (confirmed in tape6)")
    result["identity_sha256"] = computation_identity(result)
    result["qc_code_sha256"] = {name: digest(ROOT/"tools"/name) for name in QC_FILES}
    result["qc_identity_sha256"] = hashlib.sha256(json.dumps(result["qc_code_sha256"], sort_keys=True).encode()).hexdigest()
    return result


def computation_identity(value):
    """QC code does not alter photons, nor consume an engine attempt."""
    return hashlib.sha256(json.dumps({k: value[k] for k in
        ("engine", "engine_sha256", "database", "engine_version")}, sort_keys=True).encode()).hexdigest()


@contextlib.contextmanager
def run_lock():
    # All P20M task roots share one serial lock. No installed fixed files written.
    import msvcrt
    path = ROOT / "logs/p20m/official_engine.lock"
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as f:
        if f.tell() == 0:
            f.write(b"0")
            f.flush()
        f.seek(0)
        msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
        try:
            yield
        finally:
            f.seek(0)
            msvcrt.locking(f.fileno(), msvcrt.LK_UNLCK, 1)


def qc_case(case, folder):
    case = canonical_case(case)
    tape6 = (folder/"MODOUT1.txt").read_text(errors="replace")
    if "MODTRAN(R)svn-branch-5.2.1" not in tape6:
        raise ValueError("Missing expected official version in tape6")
    fatal = [line.strip() for line in tape6.splitlines()
             if re.search(r"\b(ERROR|IERROR|FATAL|STOP|ABORT|NaN)\b", line, re.I)]
    if fatal:
        raise ValueError("Engine diagnostics: " + "; ".join(fatal[:5]))
    mode = case["mode"]
    if mode == "SpectralFlux":
        # DOWNWARD is the required sky component. DIRECT is a diagnostic;
        # target direct irradiance is independently read from SOL TR.
        rows, levels = parse_flux_at_target(folder/"spectral_flux.flx", case["target"], ("downward",))
        field = "downward_diffuse_W_m2_um"
    else:
        parser, field = {
            "OfficialTransmittance": (convert.parse_transmittance, "tau_los"),
            "Transmittance": (convert.parse_transmittance, "tau_los"),
            "ThermalRadiance": (convert.parse_radiance, "path_thermal_W_m2_sr_um"),
            "RadianceWithScattering": (convert.parse_radiance, "solar_scatter_W_m2_sr_um"),
            "DirectSolarIrradiance": (convert.parse_solar, "direct_solar_W_m2_um"),
        }[mode]
        rows, levels = parser(folder/"MODOUT2.txt"), None
    if not rows or not all(math.isfinite(float(v)) for r in rows for v in r.values()
                           if isinstance(v, (int, float))):
        raise ValueError("Empty/nonfinite spectral output")
    values = [float(row[field]) for row in rows]
    if min(values) < -1e-12 or (field == "tau_los" and max(values) > 1.000001):
        raise ValueError(f"Invalid physical spectral range: {min(values)}..{max(values)}")
    wavelengths = [float(r["wavelength_um"]) for r in rows]
    lo, hi = ((min(wavelengths), max(wavelengths)) if case["band"] == "official" else
              (0.30, 2.50) if case["band"] == "SOLAR_SHORTWAVE" else
              (humidity.BANDS[case["band"]]["lo_um"], humidity.BANDS[case["band"]]["hi_um"]))
    # response_integral requires genuine endpoint bracketing.
    value = metrics.response_integral(rows, field, lo, hi)
    if case["band"] != "SOLAR_SHORTWAVE":
        value /= hi-lo
    geometry = actual_geometry(case, tape6)
    warnings = [line.strip() for line in tape6.splitlines() if "WARNING" in line.upper()]
    return dict(status="SPECTRUM_QC_PASS_GEOMETRY_REVIEW_REQUIRED", rows=len(rows),
                component=field, value=value, response_um=[lo, hi],
                response="integral" if case["band"] == "SOLAR_SHORTWAVE" else "mean",
                flux_levels=levels, geometry=geometry, warnings=warnings,
                diagnostic_direct_valid=all(r.get("direct_valid", True) for r in rows),
                minimum=min(values), maximum=max(values),
                gates=dict(input="PASS_SCHEMA", engine="EXECUTED", spectrum="PASS",
                           geometry=geometry["status"], source="NOT_VERIFIED",
                           cell="NOT_VERIFIED", accuracy="NOT_VERIFIED", release="NOT_ELIGIBLE"))


def run(root, resume, limit, timeout, max_attempts):
    with run_lock():
        ident = identity()
        # Keep the identity referenced by earlier checkpoints before advancing.
        history = root/"engine_identities"
        history.mkdir(exist_ok=True)
        qc_history = root/"qc_identities"
        qc_history.mkdir(exist_ok=True)
        previous = root/"engine_database_identity.json"
        if previous.exists():
            old = read_json(previous)
            atomic_json(history/(old["identity_sha256"]+".json"), old)
            if "qc_identity_sha256" in old:
                atomic_json(qc_history/(old["qc_identity_sha256"]+".json"), old.get("qc_code_sha256", {}))
        atomic_json(qc_history/(ident["qc_identity_sha256"]+".json"), ident["qc_code_sha256"])
        atomic_json(history/(ident["identity_sha256"]+".json"), ident)
        atomic_json(root/"engine_database_identity.json", ident)
        cases = read_json(root/"case_manifest.json")["cases"]
        planned_case_count = len(cases)
        if limit:
            cases = cases[:limit]
        completed = []
        for index, case in enumerate(cases):
            folder = root/"raw"/case["case_id"]
            folder.mkdir(parents=True, exist_ok=True)
            checkpoint = folder/"checkpoint.json"
            inp = Path(case["input_file"])
            if digest(inp) != case["input_sha256"]:
                raise ValueError("Input manifest hash mismatch: " + str(inp))
            prior = read_json(checkpoint) if checkpoint.exists() else None
            prior_identity_file = history/(prior.get("identity_sha256", "")+".json") if prior else None
            prior_identity = read_json(prior_identity_file) if prior_identity_file and prior_identity_file.exists() else None
            if (resume and prior and prior.get("exit_code") == 0 and
                    prior_identity and computation_identity(prior_identity) == ident["identity_sha256"] and
                    prior["input_sha256"] == case["input_sha256"] and
                    prior.get("output_sha256") and
                    all((folder/k).is_file() and digest(folder/k) == v
                        for k, v in prior["output_sha256"].items())):
                # Re-evaluate immutable raw even when the previous QC failed.
                # Deterministic numerical failures are not rerun blindly.
                review = dict(prior, qc_identity_sha256=ident["qc_identity_sha256"],
                              execution_action="REUSED_IMMUTABLE_RAW_NO_ENGINE_ATTEMPT")
                try:
                    review.update(qc=qc_case(case, folder), status="PASS")
                    review.pop("error", None)
                except ValueError as exc:
                    review.update(status="FAIL", error=str(exc))
                atomic_json(folder/"qc_review.json", review)
                completed.append(review)
                continue
            # Fresh attempt directory makes stale output reuse impossible.
            attempt = 1
            while (folder/f"attempt_{attempt:03d}").exists():
                attempt += 1
            if attempt > max_attempts:
                if prior:
                    completed.append(dict(prior, status="FAIL",
                                          error="MAX_ATTEMPTS_REACHED_NO_IDENTITY_VALID_RESUME"))
                else:
                    completed.append(dict(case_id=case["case_id"], status="FAIL",
                                          error="MAX_ATTEMPTS_REACHED_WITHOUT_CHECKPOINT"))
                atomic_json(root/"run_manifest.json", dict(cases=completed, complete=False))
                print(f"{index+1}/{len(cases)} {case['case_id']} MAX_ATTEMPTS_REACHED", flush=True)
                continue
            work = folder/f"attempt_{attempt:03d}"
            work.mkdir()
            shutil.copy2(inp, work/"tape5")
            shutil.copy2(inp, work/"modin")
            row = dict(case_id=case["case_id"], input_sha256=case["input_sha256"],
                       identity_sha256=ident["identity_sha256"], attempt=attempt,
                       qc_identity_sha256=ident["qc_identity_sha256"],
                       started_utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                       mode=case["mode"], status="RUNNING")
            atomic_json(checkpoint, row)
            start = time.monotonic()
            try:
                with (work/"stdout.txt").open("wb") as out, (work/"stderr.txt").open("wb") as err:
                    p = subprocess.run([ident["engine"]], cwd=work, stdout=out, stderr=err,
                                       timeout=timeout, creationflags=subprocess.CREATE_NO_WINDOW)
                row["exit_code"] = p.returncode
                for source, destination in OUTPUTS.items():
                    if (work/source).is_file():
                        shutil.copy2(work/source, folder/destination)
                if p.returncode:
                    raise ValueError(f"Engine exit {p.returncode}")
                if (work/"stderr.txt").stat().st_size:
                    raise ValueError("Engine stderr not empty")
                # Mandatory products must be from THIS attempt.
                for required in ("tape6", "specflux" if case["mode"] == "SpectralFlux" else "tape7"):
                    if not (work/required).is_file():
                        raise ValueError("Fresh product missing: "+required)
                row["qc"] = qc_case(case, folder)
                row["status"] = "PASS"
            except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
                row.update(status="FAIL", error=str(exc))
            row["elapsed_seconds"] = time.monotonic()-start
            row["output_sha256"] = {dst: digest(folder/dst) for src, dst in OUTPUTS.items()
                                     if (work/src).is_file()}
            row["bytes"] = sum(p.stat().st_size for p in work.iterdir() if p.is_file())
            atomic_json(checkpoint, row)
            completed.append(row)
            atomic_json(root/"run_manifest.json", dict(cases=completed, complete=False))
            print(f"{index+1}/{len(cases)} {case['case_id']} {row['status']} "
                  f"{row['elapsed_seconds']:.2f}s {row.get('error','')}", flush=True)
        atomic_json(root/"run_manifest.json", dict(cases=completed,
                    complete=len(completed)==planned_case_count,
                    selected_cases_complete=True, planned_case_count=planned_case_count))


def qc(root):
    cases = read_json(root/"case_manifest.json")["cases"]
    results = []
    for case in cases:
        path = root/"raw"/case["case_id"]/"checkpoint.json"
        result = read_json(path) if path.exists() else dict(case_id=case["case_id"], status="NOT_RUN")
        if result.get("exit_code") == 0 and result.get("output_sha256"):
            try:
                if digest(case["input_file"]) != case["input_sha256"] or result["input_sha256"] != case["input_sha256"]:
                    raise ValueError("Input identity mismatch")
                for name, sha in result["output_sha256"].items():
                    if digest(path.parent/name) != sha:
                        raise ValueError("Output hash mismatch: "+name)
                result["qc"] = qc_case(case, path.parent)
                result["status"] = "PASS"
                result.pop("error", None)
            except (ValueError, OSError) as exc:
                result.update(status="FAIL", error=str(exc))
        results.append(result)
    groups = collections.defaultdict(list)
    for r in results:
        if "elapsed_seconds" in r:
            groups[r["mode"]].append(r["elapsed_seconds"])
    summary = dict(status="PILOT_ONLY_NOT_FULL_DOMAIN",
                   counts=dict(collections.Counter(r["status"] for r in results)),
                   component_count=len(cases),
                   seconds_by_mode={k: dict(count=len(v), mean=sum(v)/len(v), maximum=max(v))
                                    for k, v in groups.items()},
                   required_domain_validated=False,
                   qc_code_sha256={name: digest(ROOT/"tools"/name) for name in QC_FILES},
                   cases=results,
                   failed=[r for r in results if r["status"] != "PASS"])
    atomic_json(root/"pilot_qc.json", summary)
    print(json.dumps(summary, indent=2))
    return 0 if all(r["status"] == "PASS" for r in results) else 2


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("action", choices=("prepare", "prepare-repair", "prepare-solar-repair", "run", "qc"))
    ap.add_argument("--root", type=Path, default=DEFAULT)
    ap.add_argument("--resume", action="store_true")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--timeout", type=float, default=120)
    ap.add_argument("--max-attempts", type=int, default=3,
                    help="Total attempts per immutable case, including earlier invocations")
    args = ap.parse_args()
    if args.max_attempts < 1 or args.timeout <= 0 or not math.isfinite(args.timeout):
        ap.error("max-attempts and timeout must be positive")
    root = args.root.resolve()
    if not any(root.is_relative_to(ROOT/part) for part in ("logs/p20m", "logs/p20m2")):
        ap.error("task root must be under logs/p20m or logs/p20m2")
    root.mkdir(parents=True, exist_ok=True)
    if args.action == "prepare":
        prepare(root)
    elif args.action == "prepare-repair":
        prepare_repair(root)
    elif args.action == "prepare-solar-repair":
        prepare_solar_repair(root)
    elif args.action == "run":
        run(root, args.resume, args.limit, args.timeout, args.max_attempts)
    else:
        return qc(root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
