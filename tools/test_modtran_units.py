#!/usr/bin/env python3
"""Regression tests for the MODTRAN native-wavenumber to SI-wavelength conversion."""

from __future__ import annotations

import math
import unittest

try:
    from modtran_convert_to_si import native_cm1_to_si, si_to_native_cm1
except ModuleNotFoundError:  # unittest discovery from repository root
    from tools.modtran_convert_to_si import native_cm1_to_si, si_to_native_cm1


class ModtranUnitConversionTests(unittest.TestCase):
    def test_known_factor_at_five_micrometres(self) -> None:
        self.assertAlmostEqual(native_cm1_to_si(1.0, 5.0), 4.0e6, places=6)

    def test_known_factor_at_three_micrometres(self) -> None:
        self.assertAlmostEqual(native_cm1_to_si(1.0, 3.0), 1.0e8 / 9.0, places=6)

    def test_known_factor_at_nir_point_nine_micrometres(self) -> None:
        self.assertAlmostEqual(native_cm1_to_si(1.0, 0.9), 1.0e8 / 0.81, places=6)

    def test_roundtrip(self) -> None:
        for wavelength in [0.70, 0.90, 1.10, 3.0, 3.5, 4.0, 4.5, 5.0]:
            native = 2.0542e-9
            recovered = si_to_native_cm1(native_cm1_to_si(native, wavelength), wavelength)
            self.assertTrue(math.isclose(native, recovered, rel_tol=1e-15, abs_tol=0.0))


if __name__ == "__main__":
    unittest.main()
