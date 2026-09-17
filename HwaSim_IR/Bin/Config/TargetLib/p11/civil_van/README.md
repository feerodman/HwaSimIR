# P11 Civil Panel Van

This is a project-authored, low-complexity civil panel-van asset for P11 SWIR/MWIR test scenes. It is independent of, and does not contain or derive from, the GMC Van asset. The canonical editable geometry is `p11_civil_van.obj`; `p11_civil_van.egg` and `p11_civil_van.bam` are deterministic Panda3D derivatives produced by `tools/p11_civil_van_asset_check.ps1`.

## Geometry contract

- Units: metres.
- Coordinate system: Z-up, X is vehicle width, Y is vehicle length, and the front points toward +Y.
- Nominal size: 2.29 m wide including wheels, 5.40 m long including tailpipe/bumpers, and 2.34 m high.
- The asset deliberately uses simple boxes and low-segment cylinders so every region remains easy to inspect and render on RK3588.

## Material partitions

| ID | Region | Material database key | Intended locality |
|---:|---|---|---|
| 11 | `car_paint` | `BM_PAINT` | outer body and cab panels |
| 12 | `metal` | `BM_METAL` | bumpers, grille, wheel hubs |
| 13 | `glass` | `BM_GLASS` | windshield and front side windows |
| 14 | `rubber` | `BM_RUBBER` | four tyres |
| 15 | `engine_bay` | `BM_METAL-IRON` | localized hood/engine sample only |
| 16 | `exhaust_tailpipe` | `BM_METAL-IRON` | localized muffler and tailpipe only |

`p11_civil_van_material_id.pgm` is an exact 8-bit, six-cell ID atlas. Every OBJ vertex has a UV at the center of its region's atlas cell, so it can be bound as Panda3D texture stage 1 using nearest filtering. `p11_civil_van_material_id.pgm.xml` follows the existing `IRSceneMaterialMapper` XML layout.

The visible colours in the MTL/PPM files are for orientation and preview only. They are not SWIR reflectance, MWIR emissivity, temperature, or calibration data. Explicit model-local band assumptions and nominal temperatures are in `p11_civil_van_band_optics.csv`; each band enforces `rho + epsilon + tau = 1`. These are engineering assumptions pending traceable coupon or manufacturer measurements.

The engine bay and exhaust partitions are intentionally local. Both use a 303 K engine-off nominal; only `engineState=true` selects 345 K for the engine bay and 475 K for the tailpipe. A consumer must not apply either engine-on temperature to the complete vehicle. The same explicit band fields and temperatures are carried by both `p11_civil_van_band_optics.csv` and the parseable material XML. The model contains no damage-state semantics and does not alter any protocol identity or target mapping.

## Rebuild and verify

From the repository root in PowerShell:

```powershell
.\tools\p11_civil_van_asset_check.ps1
```

The check regenerates the OBJ and companion data, creates/validates EGG and BAM using Panda3D 1.10.15 tools, verifies all six SWIR/MWIR energy balances, renders a close offscreen preview, and writes hashes under `logs/p11/work/civil_model`.
