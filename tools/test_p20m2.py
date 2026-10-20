"""Regressions for observed P20M2 false positives and evidence corruption."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
import p20m2_geometry_qc as geometry
import p20m2_review as review
import p20m_modtran as engine
import p20m2_trace_qc as trace

class QCRegression(unittest.TestCase):
    def test_repair_schema_requires_geometry(self):
        c = dict(mode="Transmittance", target=3, requested_observer=3, requested_range=.001)
        normalized = geometry.canonical_case(c)
        self.assertEqual(normalized["range"], .001)
        self.assertEqual(normalized["observer"], 3)
        del c["requested_range"]
        with self.assertRaisesRegex(ValueError, "required case"):
            geometry.canonical_case(c)

    def test_input_echo_does_not_certify_zero_solved_path(self):
        c = dict(mode="Transmittance", target=3, observer=3, range=.001)
        raw = ("6371.23 RADIUS OF THE EARTH\nH1ALT=3 H2ALT=3 HRANGE=.001\n"
               "SUMMARY OF THE GEOMETRY CALCULATION\nH1ALT=3 H2ALT=3 OBSZEN=90 "
               "HRANGE=0 BETA=0 PHI=90 HMIN=3 BENDING=0 LENN=0\n")
        result = geometry.actual_geometry(c, raw)
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["actual"]["HRANGE"], 0)

    def test_refraction_ignores_gas_profile_columns(self):
        raw = ("ATMOSPHERIC PROFILES\n"
               "1 0 1000 294 1 1 1 1 .0002667 1 1 1 1 1\n"
               "1 0 1000 294 1 1 1 1 88 1 1 1 1 1 1\n"
               "BOUNDARY LAYER\n")
        result = review.atmospheric_refraction_profile(raw)
        self.assertEqual(len(result), 1)
        self.assertAlmostEqual(result[0]["refractive_index"], 1.0002667)

    def test_computation_identity_excludes_qc(self):
        value = dict(engine="official", engine_sha256="a", database=[dict(sha256="b")],
                     engine_version="5.2.1", qc_code_sha256={"parser":"before"})
        first = engine.computation_identity(value)
        value["qc_code_sha256"]["parser"] = "after"
        self.assertEqual(first, engine.computation_identity(value))
        value["database"][0]["sha256"] = "changed"
        self.assertNotEqual(first, engine.computation_identity(value))

    def test_physical_trace_rejects_duplicate_or_contaminated_record(self):
        row = dict(schema="HwaSimIR.PhysicalTrace.1", runId="fixture", sourceSeq=1,
                   generation=1, simUtcMs=1792540800000, simRelativeSec=0,
                   timeValid=True, targetKey="34/3/3", band="SWIR", datasetId="fixture",
                   cellId="fixture", sourceCases="", cameraStatus="MATCHED_REFERENCE",
                   overall="PARTIAL", error="other_components_missing",
                   inputSnapshotId="fixture:1:1", input={"rangeKm":.001},
                   components={"tau":{"value":.99,"unit":"1","status":"VALID"}})
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)/"trace.jsonl"
            for content in (json.dumps(row)+"\n"+json.dumps(row)+"\n",
                            json.dumps(row)[:-1]+',"band":"[ProtocolRoute]"}\n',
                            "[ProtocolRoute] valid=20\n"):
                p.write_text(content)
                with self.assertRaises(ValueError):
                    trace.validate(p)
            invalid = copy.deepcopy(row)
            invalid["components"]["tau"]["value"] = None
            p.write_text(json.dumps(invalid)+"\n")
            with self.assertRaisesRegex(ValueError, "Nonfinite valid"):
                trace.validate(p)

if __name__ == "__main__":
    unittest.main()
