# P12 controlled glass rig

This additive CC0 engineering fixture has one independently tagged glass slab
in front of one larger, opaque background plate. It is selected only by the
explicit `P12ControlledGlassRig=1` diagnostic switch; ordinary 0x66 runs retain
the P11 sample rack. `P12ControlledGlassBackground=Bright|Dark` selects an
energy-balanced known background, and `P12ControlledGlassTransmission=On|Blocked`
selects the production single-pass composite or a declared non-physical
transmission ablation. The glass values are engineering assumptions, not coupon
or sensor calibration. No refraction, Fresnel term, or multiple reflection is
claimed.
