# P11 Controlled IR Sample Rack

This project-authored CC0 asset is the formal P11A controlled blackbody,
graybody and material-sample target.  It uses the protocol's existing
`0x66`/`Resv2` identity; it does not change the protocol structure and does not
replace the independent `0x55` civil van.

## Geometry and view

The model is a 4-by-2 rack of eight separate 0.95 x 0.90 m panels.  Units are
metres, Z is up and the panel faces point toward +Y.  In increasing world-X
order the top row is IDs 21..24 and the bottom row is IDs 25..28; a camera on
+Y therefore sees each row in reverse screen order.  Large faces and
gaps are intentional so a 3--5 km, 800 x 800, 0.1-degree controlled view can
resolve every partition rather than reducing the target to a few pixels.

## Formal panel contract

| ID | Panel | Temperature K | SWIR rho/epsilon/tau | MWIR rho/epsilon/tau | Basis |
|---:|---|---:|---|---|---|
| 21 | blackbody | 250 | 0/1/0 | 0/1/0 | ideal reference |
| 22 | blackbody | 300 | 0/1/0 | 0/1/0 | ideal reference |
| 23 | blackbody | 350 | 0/1/0 | 0/1/0 | ideal reference |
| 24 | blackbody | 475 | 0/1/0 | 0/1/0 | ideal reference |
| 25 | graybody | 300 | .2/.8/0 | .2/.8/0 | ideal opaque reference |
| 26 | reflector | 300 | .8/.2/0 | .8/.2/0 | ideal diffuse reference |
| 27 | glass | 300 | .08/.07/.85 | .05/.90/.05 | generic engineering assumption |
| 28 | cold graybody | 275 | .3/.7/0 | .3/.7/0 | ideal opaque reference |

Every row obeys `rho + epsilon + tau = 1`.  Temperatures and band properties
are encoded independently in the CSV, XML and manifest.  The visible PPM/MTL
colours are false colours for orientation only and must never be interpreted as
SWIR reflectance, MWIR emissivity, temperature, or calibration data.  These
panels validate the simulator's mathematical and rendering chain; they do not
claim calibration against a physical laboratory standard.

## Rebuild and verify

From the repository root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\p11_controlled_samples_asset_check.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\p11_controlled_samples_integration_check.ps1
```

The first command deterministically regenerates OBJ/EGG/BAM and both atlases,
renders close previews, validates all eight energy balances and records SHA-256
hashes.  The second command verifies the unchanged wire sizes, full-key fixture,
renderer/stimulus/annotation mappings and explicit Resv2 capacity allocation.
