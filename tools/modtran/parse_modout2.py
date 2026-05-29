#!/usr/bin/env python3
"""Parse PcModWin5/MODTRAN5 MODOUT2 spectral tables into HwaSimIR LUT CSV rows."""

from __future__ import annotations

import argparse
import csv
import re
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, Iterable, List, Sequence


NUMBER_RE = re.compile(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?")

MODE_ALIASES = {
    "transmittance": "Transmittance",
    "thermalradiance": "ThermalRadiance",
    "thermal": "ThermalRadiance",
    "directsolarirradiance": "DirectSolarIrradiance",
    "solar": "DirectSolarIrradiance",
    "radiancewithscattering": "RadianceWithScattering",
    "scattering": "RadianceWithScattering",
}

PATH_SCHEMA = [
    "case_id",
    "band",
    "mode",
    "atmosphere_model",
    "aerosol_model",
    "humidity_profile",
    "visibility_km",
    "observer_alt_km",
    "target_alt_km",
    "range_km",
    "wavenumber_cm",
    "wavelength_um",
    "tau_up",
    "path_radiance",
    "unit_radiance",
    "source_file",
]

SOLAR_SCHEMA = [
    "case_id",
    "band",
    "mode",
    "atmosphere_model",
    "aerosol_model",
    "humidity_profile",
    "visibility_km",
    "target_alt_km",
    "solar_zenith_deg",
    "wavenumber_cm",
    "wavelength_um",
    "tau_down",
    "solar_irradiance",
    "unit_irradiance",
    "source_file",
]

SKY_SCHEMA = [
    "case_id",
    "band",
    "mode",
    "atmosphere_model",
    "aerosol_model",
    "humidity_profile",
    "visibility_km",
    "observer_alt_km",
    "target_alt_km",
    "range_km",
    "solar_zenith_deg",
    "view_zenith_deg",
    "wavenumber_cm",
    "wavelength_um",
    "sky_radiance",
    "path_scattering_radiance",
    "unit_radiance",
    "source_file",
]

TABLE_SOURCE_COLUMNS = {
    "transmittance": ["FREQ/CM-1", "COMBIN TRANS"],
    "radiance": [
        "FREQ",
        "TOT_TRANS",
        "PTH_THRML",
        "THRML_SCT",
        "SURF_EMIS",
        "SOL_SCAT",
        "SING_SCAT",
        "GRND_RFLT",
        "DRCT_RFLT",
        "TOTAL_RAD",
        "REF_SOL",
        "SOL@OBS",
        "DEPTH",
        "DIR_EM",
        "TOA_SUN",
        "BBODY_T[K]",
    ],
    "solar": ["FREQ", "TRANS", "SOL TR", "SOLAR", "optional trailing DEPTH"],
}


class ParseError(RuntimeError):
    pass


def normalize_mode(value: str) -> str:
    key = re.sub(r"[^A-Za-z0-9]", "", value).lower()
    if key not in MODE_ALIASES:
        allowed = ", ".join(sorted(set(MODE_ALIASES.values())))
        raise ParseError(f"Unsupported mode '{value}'. Allowed modes: {allowed}")
    return MODE_ALIASES[key]


def parse_number_tokens(line: str) -> List[float]:
    return [float(match.group(0)) for match in NUMBER_RE.finditer(line)]


def find_table(lines: Sequence[str]) -> Dict[str, object]:
    for index, line in enumerate(lines):
        upper = line.upper()
        if "FREQ" not in upper:
            continue
        if "COMBIN" in upper:
            return {
                "type": "transmittance",
                "header": [line.rstrip(), lines[index + 1].rstrip() if index + 1 < len(lines) else ""],
                "data_start": index + 2,
            }
        if "TOT_TRANS" in upper:
            return {
                "type": "radiance",
                "header": [line.rstrip()],
                "data_start": index + 1,
            }
        if "SOL TR" in upper and "SOLAR" in upper:
            return {
                "type": "solar",
                "header": [line.rstrip()],
                "data_start": index + 1,
            }
    raise ParseError("Could not find a supported MODOUT2 spectral table header. Paste the table header if this is a new PcModWin output shape.")


def read_spectral_rows(lines: Sequence[str], data_start: int) -> List[List[float]]:
    rows: List[List[float]] = []
    for line in lines[data_start:]:
        tokens = parse_number_tokens(line)
        if len(tokens) < 2:
            continue
        stripped = line.strip()
        if not stripped or not re.match(r"^[-+]?(?:\d|\.)", stripped):
            continue
        rows.append(tokens)
    if not rows:
        raise ParseError("Found a table header but no numeric spectral rows.")
    return rows


def wavelength_um(wavenumber_cm: float) -> float:
    if wavenumber_cm == 0:
        raise ParseError("Encountered wavenumber 0; cannot compute wavelength_um.")
    return 10000.0 / wavenumber_cm


def value_or_blank(value: object) -> object:
    return "" if value is None else value


def fmt_float(value: object, digits: int = 10) -> str:
    if value is None or value == "":
        return ""
    return f"{float(value):.{digits}g}"


def base_metadata(args: argparse.Namespace) -> Dict[str, object]:
    source_file = args.source_file if args.source_file else str(Path(args.input))
    return {
        "case_id": args.case_id if args.case_id else Path(args.input).stem,
        "band": args.band,
        "mode": args.mode,
        "atmosphere_model": args.atmosphere_model,
        "aerosol_model": args.aerosol_model,
        "humidity_profile": args.humidity_profile,
        "visibility_km": value_or_blank(args.visibility_km),
        "observer_alt_km": value_or_blank(args.observer_alt_km),
        "target_alt_km": value_or_blank(args.target_alt_km),
        "range_km": value_or_blank(args.range_km),
        "solar_zenith_deg": value_or_blank(args.solar_zenith_deg),
        "view_zenith_deg": value_or_blank(args.view_zenith_deg),
        "source_file": source_file,
    }


def rows_for_mode(args: argparse.Namespace, table_type: str, raw_rows: Iterable[List[float]]) -> tuple[List[str], List[Dict[str, object]], List[str]]:
    meta = base_metadata(args)
    output_rows: List[Dict[str, object]] = []
    source_columns = TABLE_SOURCE_COLUMNS[table_type]
    radiance_unit = "MODOUT2_native"
    irradiance_unit = "MODOUT2_native"

    if args.mode == "Transmittance":
        if table_type not in {"transmittance", "radiance", "solar"}:
            raise ParseError(f"Transmittance mode cannot use table type {table_type}.")
        for tokens in raw_rows:
            if len(tokens) < 2:
                continue
            wn = tokens[0]
            row = dict.fromkeys(PATH_SCHEMA, "")
            row.update(meta)
            row["wavenumber_cm"] = fmt_float(wn)
            row["wavelength_um"] = fmt_float(wavelength_um(wn))
            row["tau_up"] = fmt_float(tokens[1])
            output_rows.append(row)
        return PATH_SCHEMA, output_rows, source_columns

    if args.mode == "ThermalRadiance":
        if table_type != "radiance":
            raise ParseError("ThermalRadiance requires a MODOUT2 table with TOT_TRANS/PTH_THRML columns.")
        for tokens in raw_rows:
            if len(tokens) < 3:
                raise ParseError("ThermalRadiance row did not include FREQ, TOT_TRANS, and PTH_THRML.")
            wn = tokens[0]
            row = dict.fromkeys(PATH_SCHEMA, "")
            row.update(meta)
            row["wavenumber_cm"] = fmt_float(wn)
            row["wavelength_um"] = fmt_float(wavelength_um(wn))
            row["tau_up"] = fmt_float(tokens[1])
            row["path_radiance"] = fmt_float(tokens[2])
            row["unit_radiance"] = radiance_unit
            output_rows.append(row)
        return PATH_SCHEMA, output_rows, source_columns

    if args.mode == "DirectSolarIrradiance":
        if table_type != "solar":
            raise ParseError("DirectSolarIrradiance requires a MODOUT2 table with TRANS/SOL TR/SOLAR columns.")
        for tokens in raw_rows:
            if len(tokens) < 4:
                raise ParseError("DirectSolarIrradiance row did not include FREQ, TRANS, SOL TR, and SOLAR.")
            wn = tokens[0]
            row = dict.fromkeys(SOLAR_SCHEMA, "")
            row.update(meta)
            row["wavenumber_cm"] = fmt_float(wn)
            row["wavelength_um"] = fmt_float(wavelength_um(wn))
            row["tau_down"] = fmt_float(tokens[1])
            row["solar_irradiance"] = fmt_float(tokens[3])
            row["unit_irradiance"] = irradiance_unit
            output_rows.append(row)
        return SOLAR_SCHEMA, output_rows, source_columns

    if args.mode == "RadianceWithScattering":
        if table_type != "radiance":
            raise ParseError("RadianceWithScattering requires a MODOUT2 table with SOL_SCAT/SING_SCAT/TOTAL_RAD columns.")
        for tokens in raw_rows:
            if len(tokens) < 10:
                raise ParseError("RadianceWithScattering row did not include enough radiance/scattering columns.")
            wn = tokens[0]
            row = dict.fromkeys(SKY_SCHEMA, "")
            row.update(meta)
            row["wavenumber_cm"] = fmt_float(wn)
            row["wavelength_um"] = fmt_float(wavelength_um(wn))
            row["path_scattering_radiance"] = fmt_float(tokens[5])
            row["sky_radiance"] = fmt_float(tokens[9])
            row["unit_radiance"] = radiance_unit
            output_rows.append(row)
        return SKY_SCHEMA, output_rows, source_columns

    raise ParseError(f"Unhandled mode {args.mode}")


def write_csv(path: Path, schema: Sequence[str], rows: Sequence[Dict[str, object]], append: bool) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    write_header = True
    mode = "w"
    if append and path.exists() and path.stat().st_size > 0:
        write_header = False
        mode = "a"
    with path.open(mode, newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=schema, extrasaction="ignore")
        if write_header:
            writer.writeheader()
        writer.writerows(rows)


def numeric_values(rows: Sequence[Dict[str, object]], column: str) -> List[float]:
    values: List[float] = []
    for row in rows:
        value = row.get(column, "")
        if value in {"", None}:
            continue
        try:
            values.append(float(value))
        except (TypeError, ValueError):
            continue
    return values


def min_max_text(values: Sequence[float]) -> str:
    if not values:
        return "n/a"
    return f"{min(values):.10g} .. {max(values):.10g}"


def append_qc_report(path: Path, args: argparse.Namespace, table_type: str, source_columns: Sequence[str], rows: Sequence[Dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    wavelength_values = numeric_values(rows, "wavelength_um")
    tau_values = numeric_values(rows, "tau_up") + numeric_values(rows, "tau_down")
    radiance_values = (
        numeric_values(rows, "path_radiance")
        + numeric_values(rows, "sky_radiance")
        + numeric_values(rows, "path_scattering_radiance")
    )
    irradiance_values = numeric_values(rows, "solar_irradiance")
    warnings: List[str] = []
    if len(rows) < 2:
        warnings.append("too few spectral rows")
    if args.mode in {"Transmittance", "ThermalRadiance"} and not tau_values:
        warnings.append("no tau values parsed")
    if args.mode == "ThermalRadiance" and not radiance_values:
        warnings.append("no thermal/path radiance values parsed")
    if args.mode == "DirectSolarIrradiance" and not irradiance_values:
        warnings.append("no solar irradiance values parsed")
    if args.mode == "RadianceWithScattering" and not radiance_values:
        warnings.append("no scattering radiance values parsed")

    with path.open("a", encoding="utf-8") as handle:
        handle.write("\n")
        handle.write(f"## Case {args.case_id if args.case_id else Path(args.input).stem}\n")
        handle.write(f"- parsed_at: {datetime.now().isoformat(timespec='seconds')}\n")
        handle.write(f"- case_id: {args.case_id if args.case_id else Path(args.input).stem}\n")
        handle.write(f"- band: {args.band}\n")
        handle.write(f"- mode: {args.mode}\n")
        handle.write(f"- source_file: {args.input}\n")
        handle.write(f"- table_type: {table_type}\n")
        handle.write(f"- wavelength_range_um: {min_max_text(wavelength_values)}\n")
        handle.write(f"- row_count: {len(rows)}\n")
        handle.write(f"- tau_min_max: {min_max_text(tau_values)}\n")
        handle.write(f"- radiance_min_max: {min_max_text(radiance_values)}\n")
        handle.write(f"- irradiance_min_max: {min_max_text(irradiance_values)}\n")
        handle.write(f"- parser_column_mapping: {', '.join(source_columns)}\n")
        handle.write(f"- warnings_errors: {', '.join(warnings) if warnings else 'none'}\n")
        handle.write("- trend_checks: sample count is insufficient for range/visibility trend conclusions\n")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="Path to MODOUT2 text file.")
    parser.add_argument("--band", required=True, help="VIS, NIR, SWIR, MWIR, or LWIR.")
    parser.add_argument("--mode", required=True, help="Transmittance, ThermalRadiance, DirectSolarIrradiance, or RadianceWithScattering.")
    parser.add_argument("--output", required=True, help="Output CSV path.")
    parser.add_argument("--append", action="store_true", help="Append rows to output CSV if it exists.")
    parser.add_argument("--case-id", default="", help="Case identifier to write into CSV rows.")
    parser.add_argument("--source-file", default="", help="Source file value to store in CSV.")
    parser.add_argument("--atmosphere-model", default="Mid-Latitude Summer")
    parser.add_argument("--aerosol-model", default="Rural")
    parser.add_argument("--humidity-profile", default="default")
    parser.add_argument("--visibility-km", type=float)
    parser.add_argument("--observer-alt-km", type=float)
    parser.add_argument("--target-alt-km", type=float)
    parser.add_argument("--range-km", type=float)
    parser.add_argument("--solar-zenith-deg", type=float)
    parser.add_argument("--view-zenith-deg", type=float, default=0.0)
    parser.add_argument("--qc-report", default="", help="Optional qc_report.md path to append parser evidence.")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    try:
        args.mode = normalize_mode(args.mode)
        input_path = Path(args.input)
        lines = input_path.read_text(encoding="utf-8", errors="replace").splitlines()
        table = find_table(lines)
        raw_rows = read_spectral_rows(lines, int(table["data_start"]))
        schema, parsed_rows, source_columns = rows_for_mode(args, str(table["type"]), raw_rows)
        if not parsed_rows:
            raise ParseError("No usable rows were parsed from MODOUT2.")
        write_csv(Path(args.output), schema, parsed_rows, args.append)
        if args.qc_report:
            append_qc_report(Path(args.qc_report), args, str(table["type"]), source_columns, parsed_rows)
        print(f"Parsed {len(parsed_rows)} rows from {input_path} as {args.band}/{args.mode}.")
        print(f"Source columns: {', '.join(source_columns)}")
        return 0
    except ParseError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
