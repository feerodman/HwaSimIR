# HwaSimIR M1 NIR/MWIR MODTRAN QC

- overall: PASS
- formal LUT: `D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\band_lut_si.csv`
- rows: 1340 (NIR=1005, MWIR=335)
- generated local cases: 820; missing outputs: 0; input hash errors: 0
- raw↔SI roundtrip max relative error: 0.000e+00
- units: tau dimensionless; radiance W/(m^2 sr um); irradiance W/(m^2 um)
- responseMode: RectangularBand
- forbidden substitutions: TOTAL_RAD=not used; TOA SOLAR=not used

## MWIR golden case

- tau=0.6667283123; absolute error=7.000e-10
- pathThermal=0.03609890662; absolute error=8.000e-11 W/(m^2 sr um)

## NIR standard case (10/5/10 km, visibility 23 km, SZA 45 deg)

- tau_up=0.9329166851
- direct target SOL TR=844.956419072 W/(m^2 um)
- downward sky diffuse (.flx DOWNWARD)=134.112843667 W/(m^2 um)
- LOS path scattering (SOL_SCAT)=0.8262748858 W/(m^2 sr um)

## Planck 3-5 um scale

| T K | mean W/(m^2 sr um) |
| ---: | ---: |
| 250 | 0.108517488 |
| 300 | 0.932978127 |
| 500 | 83.7638889 |
| 1000 | 3253.36698 |

## Candidate comparison

- candidate tau=0.9329166851 (retained)
- candidate LOS SOL_SCAT=0.8262748858 (retained for SZA45)
- candidate solar=967.2334292 was TOA SOLAR and is rejected; formal target SOL TR=844.956419072
- candidate sky_radiance duplicated TOTAL_RAD and is rejected; formal sky diffuse comes only from .flx DOWNWARD.

## Runtime A/B scenario table

- `D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\m1_nir_mwir_qc.csv`
- Range=30 km is OD/linear interpolation between formal 20 and 35 km cells.
