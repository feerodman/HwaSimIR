# MODTRAN MWIR SI QC

- raw_SI_roundtrip_max_relative_error: 0.000e+00
- roundtrip_status: PASS
- standard_table: `D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\audit_mwir_standard_20260906\mwir_standard_si_table.csv`
- existing_candidate_rows: 670

## Planck 3-5 um reference

| T (K) | mean spectral radiance W/(m^2 sr um) | integrated radiance W/(m^2 sr) |
| ---: | ---: | ---: |
| 250 | 0.108517504 | 0.217035007 |
| 300 | 0.932978195 | 1.86595639 |
| 500 | 83.7638878 | 167.527776 |
| 1000 | 3253.36696 | 6506.73392 |

## Standard-case values

- tau_los_mean: 0.666728313
- tau_down_mean: 0.668494358
- path_radiance_mean_W_m2_sr_um: 0.0360989067
- direct_solar_irradiance_mean_W_m2_um: 7.22522191
- solar_scatter_mean_W_m2_sr_um: 0.000313442913
- downward_sky_diffuse_flux_mean_W_m2_um: 0.76858342

## Existing candidate comparison

The existing candidate is compared as evidence only. Its solar column was built from MODOUT2 `SOLAR` (TOA), not `SOL TR` (transmitted at target), so it is not accepted as direct-target irradiance.
- matching_existing_rows: 1
- existing_tau_up_band: 0.6667283123
- existing_tau_down_band: 0.668494358
- existing_path_radiance_band_W_m2_sr_um: 0.03609890662
- existing_solar_irradiance_band_W_m2_um: 10.86727262

## Trend QC

- trend_table: `D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\audit_mwir_standard_20260906\trend_qc.csv`
- range tau non-increasing: PASS
- range path non-decreasing: PASS
- visibility tau non-decreasing: PASS
- visibility path non-increasing: PASS
- solar-zenith direct irradiance non-increasing: PASS

| axis | values | metric values |
| --- | --- | --- |
| range_km | [1.0, 10.0, 30.0, 50.0] | tau=[0.7894240368200568, 0.5738013227880596, 0.40714671503156963, 0.31955246964292505]; path=[0.05932370287342687, 0.1170494317230574, 0.16096259781045139, 0.18435885077863728] |
| visibility_km | [2.0, 5.0, 10.0, 23.0, 50.0] | tau=[0.04116232802122416, 0.13450006738906498, 0.19999150985382677, 0.26574660711784587, 0.2817752414316289]; path=[0.613210174633837, 0.5690244398307304, 0.5389220859122084, 0.5091256327089626, 0.5019135120709785] |
| solar_zenith_deg | [0.0, 30.0, 45.0, 60.0, 75.0] | direct solar=[7.581746064702976, 7.438794779795132, 7.225221908989829, 6.817617473406111, 5.903356842335591] |
