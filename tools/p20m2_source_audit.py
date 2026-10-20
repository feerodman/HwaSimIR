"""Identify real default solar source from printed TOA spectrum, without reruns."""
from decimal import Decimal
from pathlib import Path
import argparse
import p20m_modtran as engine

def audit(root):
    base = root/"g1_v2"
    cases = engine.read_json(base/"case_manifest.json")["cases"]
    evidence = []
    for band in ("SWIR", "MWIR"):
        case = next(c for c in cases if c["band"] == band and
                    c["mode"] == "DirectSolarIrradiance" and c["sza"] == 0)
        raw = base/"raw"/case["case_id"]/"MODOUT2.txt"
        text = raw.read_text().split("SOL TR  SOLAR", 1)[1]
        candidates = []
        for name in ("SUN01kurucz1997.dat", "SUN01kurucz2005.dat"):
            path = engine.BIN/"Data"/name
            source = {}
            for line in path.read_text().splitlines():
                try:
                    wn, value = line.split()
                    source[float(wn)] = Decimal(value)
                except ValueError:
                    continue
            residuals = []
            for line in text.splitlines():
                fields = line.split()
                if len(fields) < 4:
                    continue
                wn = float(fields[0])
                if wn not in source:
                    continue
                actual, ref = Decimal(fields[3]), source[wn]
                # Both independent text products are rounded. Half last printed
                # digit of each is the absolute uncertainty, not an error waiver.
                tolerance = (Decimal(10)**actual.as_tuple().exponent +
                             Decimal(10)**ref.as_tuple().exponent)*Decimal(".5001")
                residuals.append(float(abs(actual-ref)/tolerance))
            if not residuals:
                raise ValueError("No matched source spectrum")
            candidates.append(dict(name=name, sha256=engine.digest(path),
                                   points=len(residuals), outside_print_resolution=sum(x>1 for x in residuals),
                                   worst_print_resolution_ratio=max(residuals)))
        if candidates[0]["outside_print_resolution"] != 0 or candidates[1]["outside_print_resolution"] == 0:
            raise ValueError("Solar identity not discriminated")
        evidence.append(dict(band=band, case_id=case["case_id"], raw=str(raw),
                             raw_sha256=engine.digest(raw), candidates=candidates))
    result = dict(status="PASS_ACTUAL_DEFAULT_SOURCE_IDENTIFIED",
                  source="SUN01kurucz1997.dat", selector="LSUNFL=F, 1 cm-1 band model",
                  original_template_label="INCORRECT_2005_METADATA_NOT_USED_TO_SELECT_ENGINE_SOURCE",
                  correction="metadata only; no immutable raw/input changes; no engine calls",
                  manual="MODTRAN_R_5.2.1 CARD 1A LSUNFL pp16-17",
                  evidence=evidence)
    engine.atomic_json(root/"source_audit.json", result)
    print(result["status"])

if __name__ == "__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--root", type=Path, required=True)
    audit(p.parse_args().root)
