"""Generate immutable typed G1 experiments; execute with p20m_modtran.py run."""
import argparse
from dataclasses import asdict
import hashlib
import json
from pathlib import Path
import p20m_modtran as engine
from p20m2_template import Atmosphere, Request, deck

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--root", type=Path, required=True)
parser.add_argument("--suite", choices=("initial", "directional", "vertical", "solarclear"), default="initial")
args = parser.parse_args()
root = args.root.resolve()
root.relative_to(engine.ROOT/"logs/p20m2")
root.mkdir(parents=True, exist_ok=True)
cases = []
environment = Atmosphere()


def add(request):
    lines = deck(environment, request)
    content = "\n".join(engine.database_deck(lines))+"\n"
    sha = hashlib.sha256(content.encode("ascii")).hexdigest()
    case_id = request.band+"_"+request.mode+"_"+sha[:16]
    p = root/"generated"/case_id/"input.tp5"
    p.parent.mkdir(parents=True, exist_ok=True)
    if p.exists() and p.read_text(encoding="ascii") != content:
        raise ValueError("Immutable input changed")
    p.write_bytes(content.encode("ascii"))
    cases.append(dict(case_id=case_id, **asdict(request), atmosphere=asdict(environment),
                      rh=environment.surface_rh_percent, visibility=environment.visibility_km,
                      template_id=environment.identity(), input_file=str(p), input_sha256=sha))


if args.suite == "initial":
    for band in ("SWIR", "MWIR"):
        for h in (3., 10., 20.):
            for distance in (.0001, .001, .01, .1):
                for geometry in ("endpoints_beta", "endpoints_angle"):
                    for mode in ("Transmittance", "ThermalRadiance"):
                        add(Request(band, mode, h, h, distance, geometry=geometry))
        for solver in ("ISAACS", "DISORT", "SCALED_ISAACS"):
            for sza in (88., 89., 89.5, 89.9, 89.99, 90., 90.01, 90.1, 90.5, 91., 92., 94., 100., 180.):
                add(Request(band, "SpectralFlux", 3., 3., sza=sza, solver=solver))
            add(Request(band, "SpectralFlux", 3., 3., solar=False, solver=solver))
            add(Request(band, "SpectralFlux", 20., 20., solar=False, solver=solver))
        for sza in (0., 73.318, 88., 90., 94.):
            add(Request(band, "DirectSolarIrradiance", 3., 3., sza=sza))
elif args.suite == "directional":
    for band in ("SWIR", "MWIR"):
        for h in (3., 10., 20.):
            for distance in (.001, .01, .1, 1.):
                for slope in (.5, 1.):
                    target = h + (1 if h < 20 else -1)*distance*slope
                    for mode in ("Transmittance", "ThermalRadiance"):
                        add(Request(band, mode, h, target, distance))
elif args.suite == "vertical":
    for band in ("SWIR", "MWIR"):
        for h in (3., 10., 20.):
            for distance in (.0001, .001, .01, .1):
                target = h + (1 if h < 20 else -1)*distance
                for mode in ("Transmittance", "ThermalRadiance"):
                    add(Request(band, mode, h, target, distance, geometry="endpoints_angle"))
else:
    for band in ("SWIR", "MWIR"):
        for solver in ("ISAACS", "DISORT", "SCALED_ISAACS"):
            for sza in (88., 89., 89.5, 89.9, 89.99, 90., 90.01, 90.1, 90.5, 91., 92., 94., 100., 180.):
                add(Request(band, "SpectralFlux", 3., 3., sza=sza, solver=solver))
            for h in (3., 20.):
                add(Request(band, "SpectralFlux", h, h, solar=False, solver=solver))
manifest = dict(schema="HwaSimIR.P20M2.Cases.1", dataset_id=root.name,
                status="G1_DEVELOPMENT_NOT_RELEASE", physical_template=asdict(environment),
                cases=cases)
path = root/"case_manifest.json"
if path.exists() and engine.read_json(path) != manifest:
    raise ValueError("Immutable task manifest changed")
engine.atomic_json(path, manifest)
print(json.dumps(dict(cases=len(cases), root=str(root), template_id=environment.identity())))
