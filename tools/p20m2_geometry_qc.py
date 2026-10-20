"""MODTRAN 5.2.1 actual-path QC, separate from spectrum/source acceptance.

Reference: local manual CARD 3, pp. 38-40; ITYPE=1 is flat homogeneous.
Printed precision is evidence resolution, never an interpolation tolerance.
"""
import math
import re

NUMBER = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[EeDd][-+]?\d+)?"


def canonical_case(case):
    result = dict(case)
    for old, new in (("requested_observer", "observer"), ("requested_range", "range")):
        if old in result:
            if new in result and result[new] != result[old]:
                raise ValueError("Conflicting case fields: " + old + "/" + new)
            result[new] = result.pop(old)
    mode = result.get("mode")
    if mode not in ("Transmittance", "ThermalRadiance", "RadianceWithScattering",
                    "DirectSolarIrradiance", "SpectralFlux", "OfficialTransmittance"):
        raise ValueError("Unknown case mode: "+str(mode))
    required = ("observer", "target", "range") if mode in (
        "Transmittance", "ThermalRadiance", "RadianceWithScattering") else (
        ("target", "sza") if mode in ("DirectSolarIrradiance", "SpectralFlux") else ())
    for name in required:
        if name not in result or not math.isfinite(float(result[name])):
            raise ValueError("Missing/nonfinite required case field: " + name)
    result["case_schema"] = "HwaSimIR.P20M2.Case.1"
    return result


def parameters(text):
    return {key: float(value.replace("D", "E")) for key, value in re.findall(
        r"\b(H1ALT|H2ALT|OBSZEN|HRANGE|BETA|PHI|HMIN|BENDING|LENN)\s*=\s*(" + NUMBER + ")", text)}


def actual_geometry(case, text):
    case = canonical_case(case)
    result = dict(status="NOT_VERIFIED", model="MODTRAN5.2.1_spherical_refractive",
                  earth_intersection="UNKNOWN", precision_budget=dict(
                      endpoint_print_km=5.1e-6, range_print_km=5.1e-6,
                      beta_print_deg=5.1e-6, solver_range_relative=1e-5),
                  requested={key: case[key] for key in ("observer", "target", "range", "sza") if key in case},
                  actual={}, mapping_status="NOT_VERIFIED_WGS84_TO_REFRACTED_PATH")
    match = re.search(r"(" + NUMBER + r")\s+RADIUS OF THE EARTH", text)
    if not match:
        result["reason"] = "missing_actual_earth_radius"
        return result
    radius = float(match.group(1))
    result["earth_radius_km"] = radius
    horizontal = re.search(
        r"HORIZONTAL PATH AT ALTITUDE\s*=\s*(" + NUMBER +
        r")\s*KM WITH HRANGE\s*=\s*(" + NUMBER + r")\s*KM", text)
    if horizontal:
        altitude, distance = map(float, horizontal.groups())
        result.update(model="ITYPE1_flat_constant_pressure", earth_intersection=False)
        result["actual"] = dict(observer=altitude, target=altitude, range=distance)
        # The actual (post-profile) report prints only three decimals.
        # Sub-metre cases cannot be geometrically certified by an input echo.
        result["precision_budget"]["actual_horizontal_print_km"] = .0005
        if "range" not in case:
            result["reason"] = "missing_horizontal_request"
        elif case["range"] < .001:
            result["reason"] = "actual_horizontal_output_resolution_insufficient"
        elif (abs(altitude-case["observer"]) > .0005001 or
              abs(altitude-case["target"]) > .0005001 or
              abs(distance-case["range"]) > .0005001):
            result.update(status="FAIL", reason="actual_horizontal_mismatch")
        else:
            result.update(status="PASS_ACTUAL_PRINTED_GEOMETRY", reason="none")
        return result
    marker = "SUMMARY OF THE GEOMETRY CALCULATION"
    if marker not in text:
        result["reason"] = "missing_actual_geometry_summary"
        return result
    solved = parameters(text.split(marker)[-1][:1500])
    result["actual"] = solved
    required = {"H1ALT", "H2ALT", "OBSZEN", "HRANGE", "BETA", "HMIN", "BENDING"}
    if not required.issubset(solved) or not all(math.isfinite(v) for v in solved.values()):
        result["reason"] = "incomplete_or_nonfinite_actual_geometry"
        return result
    result["earth_intersection"] = solved["HMIN"] < -5.1e-6
    if result["earth_intersection"]:
        result.update(status="FAIL", reason="solved_path_inside_earth")
        return result
    # Stable for tiny beta and almost equal endpoint radii.
    chord = math.hypot(solved["H2ALT"]-solved["H1ALT"],
                       2*math.sqrt((radius+solved["H1ALT"])*(radius+solved["H2ALT"])) *
                       math.sin(math.radians(solved["BETA"])/2))
    result.update(derived_chord_km=chord, arc_minus_chord_km=solved["HRANGE"]-chord)
    mode = case["mode"]
    if mode in ("Transmittance", "ThermalRadiance", "RadianceWithScattering"):
        endpoint_ok = abs(solved["H1ALT"]-case["observer"]) <= 5.1e-6 and abs(solved["H2ALT"]-case["target"]) <= 5.1e-6
        tolerance = 5.1e-6 + abs(case["range"])*1e-5
        result["range_residual_km"] = solved["HRANGE"]-case["range"]
        result["range_solver_budget_km"] = tolerance
        result.update(status="PASS_ACTUAL_PRINTED_GEOMETRY" if endpoint_ok and abs(result["range_residual_km"]) <= tolerance else "FAIL",
                      reason="none" if endpoint_ok and abs(result["range_residual_km"]) <= tolerance else "actual_endpoint_or_range_mismatch")
        result["range_semantics"] = "legacy_requested_MODTRAN_refracted_path_length_not_ECEF_chord"
    elif mode == "DirectSolarIrradiance":
        good = abs(solved["H1ALT"]-case["target"]) <= 5.1e-6 and abs(solved["OBSZEN"]-case["sza"]) <= 5.1e-6 and solved["H2ALT"] >= 99.999
        result.update(status="PASS_ACTUAL_PRINTED_GEOMETRY" if good else "FAIL",
                      reason="none" if good else "actual_solar_path_mismatch",
                      angle_semantics="apparent_solar_zenith_at_H1_not_astronomical_angle")
    elif mode == "SpectralFlux":
        good = abs(solved["H1ALT"]-case["target"]) <= 5.1e-6
        result.update(status="PASS_ACTUAL_PRINTED_GEOMETRY" if good else "FAIL",
                      reason="none" if good else "actual_flux_reference_altitude_mismatch")
    else:
        result["reason"] = "unclassified_official_example"
    return result
