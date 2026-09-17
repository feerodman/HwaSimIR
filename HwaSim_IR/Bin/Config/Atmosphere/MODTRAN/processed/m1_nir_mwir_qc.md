# HwaSimIR M1 NIR/SWIR/MWIR MODTRAN QC

- overall: PASS
- formal LUT: `D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\band_lut_si.csv`
- rows: 1412 (NIR=1005, SWIR=36, MWIR=371; legacy MWIR audit=335, P11 formal MWIR=36)
- generated local cases: 820; missing outputs: 0; input hash errors: 0
- P11 SWIR ground cases: 42 component runs -> 18 formal vertices; missing outputs: 0; input hash errors: 0
- P11 SWIR 1 km cases: 42 component runs -> 18 formal vertices; missing outputs: 0; input hash errors: 0
- P11 MWIR cases: 84 component runs -> 36 five-component formal vertices at equal-altitude 1 m and 1 km planes; missing outputs: 0; input hash errors: 0
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

## SWIR ground close-up case (1 m/1 m, 0.5 km, visibility 6 km, SZA 45 deg)

- tau_up=0.669487244932
- path thermal=0.000126641766741 W/(m^2 sr um)
- direct target SOL TR=80.4491142166 W/(m^2 um)
- downward sky diffuse=27.5536937906 W/(m^2 um)
- LOS SOL_SCAT=0.426719982182 W/(m^2 sr um)

## MWIR ground close-up case (1 m/1 m, 0.5 km, visibility 6 km, SZA 45 deg)

- tau_up=0.630848840714
- path thermal=0.295506921861 W/(m^2 sr um)
- direct target SOL TR=3.32339916819 W/(m^2 um)
- downward sky diffuse=1.97917294501 W/(m^2 um)
- LOS SOL_SCAT=0.007690580436 W/(m^2 sr um)

## MWIR 1 km equal-altitude comparison (1 km/1 km, 0.5 km, visibility 6 km, SZA 45 deg)

- tau_up=0.675661044902
- path thermal=0.21855906411 W/(m^2 sr um)
- direct target SOL TR=4.34155191417 W/(m^2 um)
- downward sky diffuse=1.44699496331 W/(m^2 um)
- LOS SOL_SCAT=0.00949735408075 W/(m^2 sr um)
- legacy 335 MWIR rows remain packaged for tau/path-thermal audit only; blank solar/sky/scattering rows are rejected by the production C++ loader.

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
