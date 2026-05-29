# PcModWin5 Aerosol Template Review

This note records the manual PcModWin5 settings expected for HwaSimIR NIR/MWIR MODTRAN visibility experiments. It is diagnostic guidance only; it does not change HwaSimIR runtime C++ or shaders.

## Production Candidate Template

Use this as the main Rural visibility sweep template:

- Aerosol Model Used: `Rural - VIS = 23km`
- Surface Meteorological Range (VIS): explicit numeric value, not `0`
- Initial template value: `23.00000`
- OD units: unchecked
- Clouds or rain: none
- Ground Altitude: `0 km`

Current hand templates:

- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/test/NIR_transmittance_Rural_SurfaceVIS23_modin.txt`
- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/test/MWIR_transmittance_Rural_SurfaceVIS23_modin.txt`

The automation may rewrite the explicit Surface Meteorological Range for smoke tests. This is intended to test whether the explicit VIS field overrides the dropdown's default Rural VIS value.

## Fixed Low Visibility Control

Use this only as a low-visibility control template:

- Aerosol Model Used: `Rural - VIS = 5km`
- Surface Meteorological Range (VIS): `0.00000`

Current hand templates:

- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/test/NIR_transmittance_RuralFixedVIS5_modin.txt`
- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/test/MWIR_transmittance_RuralFixedVIS5_modin.txt`

Do not use this fixed template as the production sweep template unless the explicit VIS override experiment fails and the production strategy is redesigned.

## Production Caution

Do not use `No Aerosols or Clouds` for production atmospheric LUTs. It is useful for isolating molecular absorption, but it removes the aerosol visibility behavior needed by HwaSimIR range/contrast modeling.

Before any production rerun, confirm:

- The PcModWin5 GUI shows the intended Rural aerosol model.
- The Surface Meteorological Range field is nonzero for sweep templates.
- MODOUT1 reports the requested meteorological range for representative visibility values.
- Low-altitude paths show a visible band-integrated transmittance response from VIS `0.5 km` to `50 km`.
- MODOUT2 radiance and irradiance units are still treated as `MODOUT2_native` unless local documentation explicitly confirms the spectral unit convention.
