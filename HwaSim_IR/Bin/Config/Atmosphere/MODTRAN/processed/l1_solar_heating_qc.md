# L1 shortwave solar-heating MODTRAN QC

- rows: 93 (45 preserved legacy + 48 P11 civil-altitude)
- humidity-profile counts: {'default': 57, 'scaled_mls_surface_rh30': 12, 'scaled_mls_surface_rh60': 12, 'scaled_mls_surface_rh85': 12}
- spectral range: 0.30-2.50 um
- output unit: W/m^2 (band-integrated)
- direct source: target-level MODOUT2 SOL TR; TOA SOLAR rejected
- diffuse source: .flx DOWNWARD; .flx DIRECT retained as QC-only and never added to diffuse

## Standard case target=5 km, visibility=23 km, SZA=45 deg

- direct=1057.22377655 W/m^2
- diffuse=210.901635455 W/m^2
- .flx DIRECT horizontal diagnostic=654.492486525 W/m^2

## Grid

| humidity profile | target km | visibility km | SZA deg | direct W/m2 | diffuse W/m2 |
| --- | ---: | ---: | ---: | ---: | ---: |
| default | 0.001 | 6 | 20 | 458.564662 | 538.660528 |
| default | 0.001 | 6 | 45 | 354.578718 | 406.773198 |
| default | 0.001 | 6 | 70 | 142.433631 | 174.685830 |
| default | 0.001 | 23 | 20 | 746.253501 | 370.999790 |
| default | 0.001 | 23 | 45 | 656.951286 | 306.683556 |
| default | 0.001 | 23 | 70 | 408.207813 | 170.046464 |
| default | 1 | 6 | 20 | 753.272741 | 390.377923 |
| default | 1 | 6 | 45 | 661.332788 | 327.150129 |
| default | 1 | 6 | 70 | 405.678379 | 182.877838 |
| default | 1 | 23 | 20 | 856.207178 | 321.800861 |
| default | 1 | 23 | 45 | 780.493611 | 274.948208 |
| default | 1 | 23 | 70 | 551.415362 | 167.179563 |
| default | 3 | 5 | 20 | 1011.689891 | 256.410529 |
| default | 3 | 5 | 45 | 960.369205 | 231.093664 |
| default | 3 | 5 | 70 | 790.860624 | 160.199803 |
| default | 3 | 23 | 20 | 1011.689891 | 258.101909 |
| default | 3 | 23 | 45 | 960.369205 | 230.123121 |
| default | 3 | 23 | 70 | 790.860624 | 158.170400 |
| default | 3 | 50 | 20 | 1024.680785 | 248.651956 |
| default | 3 | 50 | 45 | 976.520457 | 221.523436 |
| default | 3 | 50 | 70 | 816.769791 | 153.348494 |
| default | 5 | 5 | 20 | 1095.138038 | 230.368806 |
| default | 5 | 5 | 45 | 1057.223777 | 211.555663 |
| default | 5 | 5 | 70 | 926.690386 | 155.079301 |
| default | 5 | 23 | 20 | 1095.138038 | 231.697647 |
| default | 5 | 23 | 45 | 1057.223777 | 210.901635 |
| default | 5 | 23 | 70 | 926.690386 | 153.589966 |
| default | 5 | 50 | 20 | 1095.138038 | 232.218729 |
| default | 5 | 50 | 45 | 1057.223777 | 210.462181 |
| default | 5 | 50 | 70 | 926.690386 | 152.500785 |
| default | 10 | 5 | 20 | 1194.777001 | 199.096186 |
| default | 10 | 5 | 45 | 1176.029109 | 187.153212 |
| default | 10 | 5 | 70 | 1105.854199 | 146.704069 |
| default | 10 | 23 | 20 | 1194.777001 | 199.941975 |
| default | 10 | 23 | 45 | 1176.029109 | 186.759692 |
| default | 10 | 23 | 70 | 1105.854199 | 145.733872 |
| default | 10 | 50 | 20 | 1194.777001 | 200.270759 |
| default | 10 | 50 | 45 | 1176.029109 | 186.504710 |
| default | 10 | 50 | 70 | 1105.854199 | 145.055170 |
| default | 15 | 5 | 20 | 1225.019798 | 18.430350 |
| default | 15 | 5 | 45 | 1213.929497 | 17.388865 |
| default | 15 | 5 | 70 | 1171.132327 | 14.337797 |
| default | 15 | 23 | 20 | 1225.019798 | 18.643817 |
| default | 15 | 23 | 45 | 1213.929497 | 17.329111 |
| default | 15 | 23 | 70 | 1171.132327 | 14.174093 |
| default | 15 | 50 | 20 | 1225.019798 | 18.720981 |
| default | 15 | 50 | 45 | 1213.929497 | 17.282295 |
| default | 15 | 50 | 70 | 1171.132327 | 14.048251 |
| default | 20 | 5 | 20 | 1239.929514 | 8.738777 |
| default | 20 | 5 | 45 | 1232.985340 | 8.293538 |
| default | 20 | 5 | 70 | 1206.066894 | 6.989457 |
| default | 20 | 23 | 20 | 1239.929514 | 8.836663 |
| default | 20 | 23 | 45 | 1232.985340 | 8.266931 |
| default | 20 | 23 | 70 | 1206.066894 | 6.915023 |
| default | 20 | 50 | 20 | 1239.929514 | 8.871961 |
| default | 20 | 50 | 45 | 1232.985340 | 8.246055 |
| default | 20 | 50 | 70 | 1206.066894 | 6.858075 |
| scaled_mls_surface_rh30 | 0.001 | 6 | 20 | 486.444254 | 533.574478 |
| scaled_mls_surface_rh30 | 0.001 | 6 | 45 | 380.334874 | 403.547822 |
| scaled_mls_surface_rh30 | 0.001 | 6 | 70 | 158.857983 | 175.348524 |
| scaled_mls_surface_rh30 | 0.001 | 23 | 20 | 782.800604 | 376.335322 |
| scaled_mls_surface_rh30 | 0.001 | 23 | 45 | 693.946481 | 311.744397 |
| scaled_mls_surface_rh30 | 0.001 | 23 | 70 | 442.155764 | 174.596058 |
| scaled_mls_surface_rh30 | 1 | 6 | 20 | 784.647008 | 395.670080 |
| scaled_mls_surface_rh30 | 1 | 6 | 45 | 693.109634 | 332.360327 |
| scaled_mls_surface_rh30 | 1 | 6 | 70 | 435.104063 | 187.626951 |
| scaled_mls_surface_rh30 | 1 | 23 | 20 | 889.727375 | 327.938690 |
| scaled_mls_surface_rh30 | 1 | 23 | 45 | 815.261946 | 280.922595 |
| scaled_mls_surface_rh30 | 1 | 23 | 70 | 587.006523 | 172.381474 |
| scaled_mls_surface_rh60 | 0.001 | 6 | 20 | 466.280186 | 526.195391 |
| scaled_mls_surface_rh60 | 0.001 | 6 | 45 | 361.650827 | 396.801159 |
| scaled_mls_surface_rh60 | 0.001 | 6 | 70 | 146.847390 | 170.723437 |
| scaled_mls_surface_rh60 | 0.001 | 23 | 20 | 756.429079 | 369.597692 |
| scaled_mls_surface_rh60 | 0.001 | 23 | 45 | 667.176896 | 305.404233 |
| scaled_mls_surface_rh60 | 0.001 | 23 | 70 | 417.400611 | 169.640848 |
| scaled_mls_surface_rh60 | 1 | 6 | 20 | 761.980082 | 390.825240 |
| scaled_mls_surface_rh60 | 1 | 6 | 45 | 670.135167 | 327.640064 |
| scaled_mls_surface_rh60 | 1 | 6 | 70 | 413.667721 | 183.594349 |
| scaled_mls_surface_rh60 | 1 | 23 | 20 | 865.547611 | 323.043054 |
| scaled_mls_surface_rh60 | 1 | 23 | 45 | 790.167892 | 276.164537 |
| scaled_mls_surface_rh60 | 1 | 23 | 70 | 561.152334 | 168.313237 |
| scaled_mls_surface_rh85 | 0.001 | 6 | 20 | 450.453381 | 554.729734 |
| scaled_mls_surface_rh85 | 0.001 | 6 | 45 | 346.135491 | 419.963216 |
| scaled_mls_surface_rh85 | 0.001 | 6 | 70 | 135.022712 | 180.447993 |
| scaled_mls_surface_rh85 | 0.001 | 23 | 20 | 739.987401 | 374.282547 |
| scaled_mls_surface_rh85 | 0.001 | 23 | 45 | 650.270927 | 309.734449 |
| scaled_mls_surface_rh85 | 0.001 | 23 | 70 | 400.820612 | 171.765474 |
| scaled_mls_surface_rh85 | 1 | 6 | 20 | 747.321556 | 399.968646 |
| scaled_mls_surface_rh85 | 1 | 6 | 45 | 654.839049 | 336.107010 |
| scaled_mls_surface_rh85 | 1 | 6 | 70 | 398.128456 | 188.253413 |
| scaled_mls_surface_rh85 | 1 | 23 | 20 | 850.999231 | 324.553823 |
| scaled_mls_surface_rh85 | 1 | 23 | 45 | 774.884862 | 277.551250 |
| scaled_mls_surface_rh85 | 1 | 23 | 70 | 544.993504 | 168.803893 |

## Result

PASS
