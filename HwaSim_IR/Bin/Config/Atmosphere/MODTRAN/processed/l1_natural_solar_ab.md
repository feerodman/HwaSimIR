# L1 Natural Solar Numerical A/B

Overall: **PASS**

No image gain, tone-map, body-floor, or empirical solar scale is used by this harness.

| Test | Case | Material | Component/Delta 1 | Component/Delta 2 | Sensor/Final delta | Unit | Status |
|---|---|---|---:|---:|---:|---|---|
| NIR | SZA20_front | BM_PAINT | 136.481944 | 22.8450151 | 150.785508 | W/(m^2 sr um) | PASS |
| NIR | SZA20_back | BM_PAINT | 0 | 22.8450151 | 23.4592253 | W/(m^2 sr um) | PASS |
| NIR | SZA20_geometric_shadow | BM_PAINT | 0 | 22.8450151 | 23.4592253 | W/(m^2 sr um) | PASS |
| NIR | SZA20_night | BM_PAINT | 0 | 0 | 0 | W/(m^2 sr um) | PASS |
| NIR | SZA45_front | BM_PAINT | 134.478991 | 21.344722 | 146.196816 | W/(m^2 sr um) | PASS |
| NIR | SZA45_back | BM_PAINT | 0 | 21.344722 | 20.7391222 | W/(m^2 sr um) | PASS |
| NIR | SZA45_geometric_shadow | BM_PAINT | 0 | 21.344722 | 20.7391222 | W/(m^2 sr um) | PASS |
| NIR | SZA45_night | BM_PAINT | 0 | 0 | 0 | W/(m^2 sr um) | PASS |
| NIR | SZA70_front | BM_PAINT | 126.860446 | 16.8476016 | 136.028124 | W/(m^2 sr um) | PASS |
| NIR | SZA70_back | BM_PAINT | 0 | 16.8476016 | 17.6778972 | W/(m^2 sr um) | PASS |
| NIR | SZA70_geometric_shadow | BM_PAINT | 0 | 16.8476016 | 17.6778972 | W/(m^2 sr um) | PASS |
| NIR | SZA70_night | BM_PAINT | 0 | 0 | 0 | W/(m^2 sr um) | PASS |
| MWIR_THERMAL | sun120_shadow120_night120 | BM_METAL-ALUMINIUM | 1.63335812 | 1.60969036 | 1.44038307 | K delta | PASS |
| MWIR_THERMAL | sun120_shadow120_night120 | BM_PAINT | 0.807118895 | 0.86296637 | 0.844409767 | K delta | PASS |
| MWIR_THERMAL | sun120_shadow120_night120 | BM_GLASS | 0.740825913 | 0.758715215 | 0.708224972 | K delta | PASS |

Thermal sequence: direct sun 120 s, geometric-shadow equivalent (direct blocked, diffuse retained) 120 s, then night/direct+diffuse off 120 s. Integration dt=0.1 s.

## MWIR physical ROI-equivalent radiance

Computed with the formal standard-case tau/path and the same 4 um spectral Planck function as the runtime shader. Contrast is against the same unheated material; these are radiance-domain ROI equivalents, not display-gray measurements.

| Material | Base | Sun 120 s | Shadow 120 s | Night 120 s | Contrast sun/shadow/night | Unit |
|---|---:|---:|---:|---:|---|---|
| BM_METAL-ALUMINIUM | 0.168395942 | 0.178039607 | 0.177895771 | 0.176870402 | 0.00964366497 / 0.00949982907 / 0.00847446079 | W/(m^2 sr um) |
| BM_PAINT | 0.256593965 | 0.264417668 | 0.264967527 | 0.264784701 | 0.00782370288 / 0.00837356209 / 0.00819073561 | W/(m^2 sr um) |
| BM_GLASS | 0.309512779 | 0.318406604 | 0.318624337 | 0.318010177 | 0.00889382523 / 0.00911155801 / 0.0084973976 | W/(m^2 sr um) |
