"""Regression tests for failures observed in the real P20M official pilots."""
import tempfile
from pathlib import Path
import unittest
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import modtran_convert_to_si as convert
from modtran_flux import parse_flux_at_target
from unittest.mock import patch


class ParserRegression(unittest.TestCase):
    def test_nan_transmittance_must_not_shift_to_h2o_column(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/"tape7"
            path.write_text("FREQ COMBIN TRANS\n 4000.00 NaN 1.0000 0.0000\n")
            with self.assertRaisesRegex(ValueError, "Non-finite COMBIN"):
                convert.parse_transmittance(path)

    def test_nan_direct_must_not_shift_to_toa(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/"tape7"
            path.write_text("FREQ SOL TR SOLAR\n 4000.00 1.0000 NaN 0.005\n")
            with self.assertRaisesRegex(ValueError, "Non-finite SOL TR"):
                convert.parse_solar(path)

    def test_unrelated_flux_level_overflow_does_not_shift_selected_columns(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/"specflux"
            path.write_text("ALTITUDES: 0.00000 KM 10.00000 KM\n"
                            "------- -----------\n"
                            "1500.00 1E-9 2E-9 *********** 4E-9 5E-9 6E-9\n")
            rows = convert.parse_flux(path, 10.)
            self.assertAlmostEqual(rows[0]["downward_diffuse_W_m2_um"], .05)
            with self.assertRaisesRegex(ValueError, "Invalid flux at actual altitude"):
                convert.parse_flux(path, 0.)
            partial = convert.parse_flux(path, 0., ("downward",))[0]
            self.assertAlmostEqual(partial["downward_diffuse_W_m2_um"], .02)
            self.assertEqual(partial["direct_solar_W_m2_um"], "")
            self.assertFalse(partial["direct_valid"])

    def test_high_altitude_is_bracketed_not_extrapolated_from_surface(self):
        fields = ("upward_diffuse_native_W_cm2_nm", "downward_diffuse_native_W_cm2_nm",
                  "direct_solar_native_W_cm2_nm", "upward_diffuse_W_m2_um",
                  "downward_diffuse_W_m2_um", "direct_solar_W_m2_um")
        def read_level(path, altitude, required_components):
            # Nonlinear vertical field exposes erroneous 0/1 km extrapolation.
            return [dict(wavelength_um=1.5, **{key: altitude**2 for key in fields})]
        with patch("modtran_flux.flux_altitudes", return_value=[0., 1., 10., 20.]), \
                patch("modtran_flux.convert.parse_flux", side_effect=read_level):
            rows, meta = parse_flux_at_target(Path("synthetic_unit_fixture"), 15.)
            self.assertEqual(meta["lowLevelKm"], 10.)
            self.assertEqual(meta["highLevelKm"], 20.)
            self.assertEqual(rows[0][fields[0]], 250.)
            rows, meta = parse_flux_at_target(Path("synthetic_unit_fixture"), 20.)
            self.assertEqual(rows[0][fields[0]], 400.)
            self.assertEqual(meta["method"], "exact_real_flux_level")
            with self.assertRaisesRegex(ValueError, "outside real flux"):
                parse_flux_at_target(Path("synthetic_unit_fixture"), 21.)


if __name__ == "__main__":
    unittest.main()
