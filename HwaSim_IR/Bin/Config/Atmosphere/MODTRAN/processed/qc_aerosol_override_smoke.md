# Aerosol Override Smoke QC

- overall_status: PASS_WITH_WARNINGS
- band_lut_rows: 80
- low_altitude_strong_eps: 0.001
- slant_visible_eps: 1e-05
- production_lut_overwritten: no

## Visibility Sweep

| status | check |
| --- | --- |
| PASS | MWIR obs=0.1 target=0.1 range=20: low-altitude visibility response spread=0.1315556914 |
| PASS | MWIR obs=0.1 target=0.1 range=50: low-altitude visibility response spread=0.04088712234 |
| PASS | MWIR obs=3 target=3 range=20: low-altitude visibility response spread=0.0018751303 |
| PASS | MWIR obs=3 target=3 range=50: low-altitude visibility response spread=0.0033470864 |
| WARNING | MWIR obs=10 target=10 range=20: high_altitude_low_sensitivity spread=0 |
| WARNING | MWIR obs=10 target=10 range=50: high_altitude_low_sensitivity spread=0 |
| PASS | MWIR obs=20 target=3 range=20: slant visibility response spread=0.0001904928 |
| PASS | MWIR obs=20 target=3 range=50: slant visibility response spread=0.0003603519 |
| PASS | NIR obs=0.1 target=0.1 range=20: low-altitude visibility response spread=0.3233847592 |
| PASS | NIR obs=0.1 target=0.1 range=50: low-altitude visibility response spread=0.09467862812 |
| PASS | NIR obs=3 target=3 range=20: low-altitude visibility response spread=0.120726173 |
| PASS | NIR obs=3 target=3 range=50: low-altitude visibility response spread=0.1827431891 |
| WARNING | NIR obs=10 target=10 range=20: high_altitude_low_sensitivity spread=0 |
| WARNING | NIR obs=10 target=10 range=50: high_altitude_low_sensitivity spread=0 |
| PASS | NIR obs=20 target=3 range=20: slant visibility response spread=0.0087791117 |
| PASS | NIR obs=20 target=3 range=50: slant visibility response spread=0.0197120575 |

## Tau Matrix

| band | observer_alt_km | target_alt_km | range_km | visibility_km | tau_up_band |
| --- | ---: | ---: | ---: | ---: | ---: |
| MWIR | 0.1 | 0.1 | 20 | 0.5 | 0 |
| MWIR | 0.1 | 0.1 | 20 | 2 | 0.002251268126 |
| MWIR | 0.1 | 0.1 | 20 | 5 | 0.02834662924 |
| MWIR | 0.1 | 0.1 | 20 | 23 | 0.1087789138 |
| MWIR | 0.1 | 0.1 | 20 | 50 | 0.1315556914 |
| MWIR | 0.1 | 0.1 | 50 | 0.5 | 0 |
| MWIR | 0.1 | 0.1 | 50 | 2 | 0 |
| MWIR | 0.1 | 0.1 | 50 | 5 | 0.0008620512066 |
| MWIR | 0.1 | 0.1 | 50 | 23 | 0.02520695794 |
| MWIR | 0.1 | 0.1 | 50 | 50 | 0.04088712234 |
| MWIR | 3 | 3 | 20 | 0.5 | 0.3234554128 |
| MWIR | 3 | 3 | 20 | 2 | 0.3234554128 |
| MWIR | 3 | 3 | 20 | 5 | 0.3234554128 |
| MWIR | 3 | 3 | 20 | 23 | 0.3234554128 |
| MWIR | 3 | 3 | 20 | 50 | 0.3253305431 |
| MWIR | 3 | 3 | 50 | 0.5 | 0.180743294 |
| MWIR | 3 | 3 | 50 | 2 | 0.180743294 |
| MWIR | 3 | 3 | 50 | 5 | 0.180743294 |
| MWIR | 3 | 3 | 50 | 23 | 0.180743294 |
| MWIR | 3 | 3 | 50 | 50 | 0.1840903804 |
| MWIR | 10 | 10 | 20 | 0.5 | 0.7052243284 |
| MWIR | 10 | 10 | 20 | 2 | 0.7052243284 |
| MWIR | 10 | 10 | 20 | 5 | 0.7052243284 |
| MWIR | 10 | 10 | 20 | 23 | 0.7052243284 |
| MWIR | 10 | 10 | 20 | 50 | 0.7052243284 |
| MWIR | 10 | 10 | 50 | 0.5 | 0.6077509129 |
| MWIR | 10 | 10 | 50 | 2 | 0.6077509129 |
| MWIR | 10 | 10 | 50 | 5 | 0.6077509129 |
| MWIR | 10 | 10 | 50 | 23 | 0.6077509129 |
| MWIR | 10 | 10 | 50 | 50 | 0.6077509129 |
| MWIR | 20 | 3 | 20 | 0.5 | 0.6037876024 |
| MWIR | 20 | 3 | 20 | 2 | 0.6037876024 |
| MWIR | 20 | 3 | 20 | 5 | 0.6037876024 |
| MWIR | 20 | 3 | 20 | 23 | 0.6037876024 |
| MWIR | 20 | 3 | 20 | 50 | 0.6039780952 |
| MWIR | 20 | 3 | 50 | 0.5 | 0.4719670216 |
| MWIR | 20 | 3 | 50 | 2 | 0.4719670216 |
| MWIR | 20 | 3 | 50 | 5 | 0.4719670216 |
| MWIR | 20 | 3 | 50 | 23 | 0.4719670216 |
| MWIR | 20 | 3 | 50 | 50 | 0.4723273735 |
| NIR | 0.1 | 0.1 | 20 | 0.5 | 0 |
| NIR | 0.1 | 0.1 | 20 | 2 | 0 |
| NIR | 0.1 | 0.1 | 20 | 5 | 0.0003490515394 |
| NIR | 0.1 | 0.1 | 20 | 23 | 0.1293142291 |
| NIR | 0.1 | 0.1 | 20 | 50 | 0.3233847592 |
| NIR | 0.1 | 0.1 | 50 | 0.5 | 0 |
| NIR | 0.1 | 0.1 | 50 | 2 | 0 |
| NIR | 0.1 | 0.1 | 50 | 5 | 0 |
| NIR | 0.1 | 0.1 | 50 | 23 | 0.01070213632 |
| NIR | 0.1 | 0.1 | 50 | 50 | 0.09467862812 |
| NIR | 3 | 3 | 20 | 0.5 | 0.5677691504 |
| NIR | 3 | 3 | 20 | 2 | 0.5677691504 |
| NIR | 3 | 3 | 20 | 5 | 0.5677691504 |
| NIR | 3 | 3 | 20 | 23 | 0.5677691504 |
| NIR | 3 | 3 | 20 | 50 | 0.6884953234 |
| NIR | 3 | 3 | 50 | 0.5 | 0.2952809057 |
| NIR | 3 | 3 | 50 | 2 | 0.2952809057 |
| NIR | 3 | 3 | 50 | 5 | 0.2952809057 |
| NIR | 3 | 3 | 50 | 23 | 0.2952809057 |
| NIR | 3 | 3 | 50 | 50 | 0.4780240948 |
| NIR | 10 | 10 | 20 | 0.5 | 0.9569851257 |
| NIR | 10 | 10 | 20 | 2 | 0.9569851257 |
| NIR | 10 | 10 | 20 | 5 | 0.9569851257 |
| NIR | 10 | 10 | 20 | 23 | 0.9569851257 |
| NIR | 10 | 10 | 20 | 50 | 0.9569851257 |
| NIR | 10 | 10 | 50 | 0.5 | 0.9089247669 |
| NIR | 10 | 10 | 50 | 2 | 0.9089247669 |
| NIR | 10 | 10 | 50 | 5 | 0.9089247669 |
| NIR | 10 | 10 | 50 | 23 | 0.9089247669 |
| NIR | 10 | 10 | 50 | 50 | 0.9089247669 |
| NIR | 20 | 3 | 20 | 0.5 | 0.8966542677 |
| NIR | 20 | 3 | 20 | 2 | 0.8966542677 |
| NIR | 20 | 3 | 20 | 5 | 0.8966542677 |
| NIR | 20 | 3 | 20 | 23 | 0.8966542677 |
| NIR | 20 | 3 | 20 | 50 | 0.9054333794 |
| NIR | 20 | 3 | 50 | 0.5 | 0.7963899305 |
| NIR | 20 | 3 | 50 | 2 | 0.7963899305 |
| NIR | 20 | 3 | 50 | 5 | 0.7963899305 |
| NIR | 20 | 3 | 50 | 23 | 0.7963899305 |
| NIR | 20 | 3 | 50 | 50 | 0.816101988 |
