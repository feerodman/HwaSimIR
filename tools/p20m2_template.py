"""Typed MODTRAN 5.2.1 template; serializers target the installed legacy cards."""
from dataclasses import asdict, dataclass
import hashlib
import json
import math

import p11_modtran_humidity_grid as humidity


@dataclass(frozen=True)
class Atmosphere:
    profile: str = "Mid-Latitude Summer"
    aerosol: str = "Rural"
    cloud_model: int = 0
    surface_rh_percent: float = 85
    visibility_km: float = 6
    co2_ppmv: float = 360
    earth_radius_km: float = 6371.23
    surface_altitude_km: float = 0
    surface_temperature_k: float = 294.2
    surface_albedo: float = .4
    humidity_semantics: str = "scaled_mls_surface_rh"
    solar_spectrum: str = "SUN01kurucz1997.dat"
    solar_selector: str = "LSUNFL=F_1cm-1_builtin"
    source_distance: str = "mean_earth_sun_distance_day0"

    def validate(self):
        if self.profile != "Mid-Latitude Summer" or self.aerosol != "Rural":
            raise ValueError("Unsupported production atmosphere")
        if self.cloud_model != 0:
            raise ValueError("Gas/aerosol baseline requires explicit ICLD=0; weather is independent")
        if self.solar_spectrum != "SUN01kurucz1997.dat" or self.solar_selector != "LSUNFL=F_1cm-1_builtin":
            raise ValueError("Unsupported solar source identity")
        if not (30 <= self.surface_rh_percent <= 85 and 6 <= self.visibility_km <= 23):
            raise ValueError("Meteorology outside declared baseline")
        if self.surface_altitude_km != 0 or not 0 <= self.surface_albedo <= 1:
            raise ValueError("Unsupported boundary")

    def identity(self):
        return hashlib.sha256(json.dumps(asdict(self), sort_keys=True).encode()).hexdigest()


@dataclass(frozen=True)
class Request:
    band: str
    mode: str
    observer: float
    target: float
    range: float = 0
    sza: float = 0
    relative_azimuth: float = 0
    geometry: str = "endpoints_range"
    solver: str = "ISAACS"
    solar: bool = True
    streams: int = 8


def spectrum(band, flux):
    lo, hi = {"SWIR": (4000, 9091), "MWIR": (1999, 3334),
              "SOLAR_SHORTWAVE": (3999, 33334)}[band]
    return (f"{lo:10.3f}{hi:10.3f}{1.:10.3f}{1.:10.3f}"+
            ("RN              T    0     0.000" if flux else " W                 0     0.000"))


def deck(environment, request):
    environment.validate()
    r, a = request, environment
    flux = r.mode == "SpectralFlux"
    direct = r.mode == "DirectSolarIrradiance"
    mode = {"Transmittance": 0, "ThermalRadiance": 1,
            "RadianceWithScattering": 2, "DirectSolarIrradiance": 3,
            "SpectralFlux": 2 if r.solar else 1}[r.mode]
    itype = 1 if r.geometry == "horizontal" else 2
    imult = 1 if flux or r.mode == "RadianceWithScattering" else 0
    card1 = "T F 2"+"".join(f"{v:5d}" for v in [itype, mode, imult]+[0]*9)
    card1 += f"{a.surface_temperature_k:8.3f}{a.surface_albedo:7.4f}"
    # All values originate in the typed common environment, including flux.
    c = list(humidity.card1a(a.surface_rh_percent, flux=True))
    c[0] = {"ISAACS": "f", "DISORT": "t", "SCALED_ISAACS": "s"}[r.solver]
    c[1] = "T" if r.mode == "RadianceWithScattering" and r.solver != "ISAACS" else "F"
    c[3:6] = f"{r.streams:3d}"
    c[10:20] = f"{a.co2_ppmv:10.3f}"
    c[30:40] = f"{0:10.6f}"  # standard ozone, no separate profile scale
    c[41] = "F"  # installed manual: 1 cm-1 default is Kurucz 1997, NOT 2005
    c[49] = "F"  # runner injects the installed database directory
    lines = [card1, "".join(c), "01_2009",
             "".join(f"{v:5d}" for v in (1, 0, 1, 0, a.cloud_model, 0))+
             f"{a.visibility_km:10.3f}"+f"{0.:10.3f}"*3+f"{a.surface_altitude_km:10.3f}"]
    if flux:
        lines.append(f"{r.target:10.6f}{a.surface_altitude_km:10.6f}{180.:10.6f}"+
                     f"{0.:10.6f}"*2+f"{a.earth_radius_km:10.3f}    0     "+f"{0.:10.6f}"*2)
    elif direct:
        lines.append(f"{r.target:10.6f}{0.:10.6f}{r.sza:10.5f}{0:5d}     "+
                     f"{a.earth_radius_km:10.3f}{0:5d}{0.:10.3f}")
    else:
        if r.range <= 0 or r.range < abs(r.target-r.observer)-1e-10:
            raise ValueError("Illegal positive propagation geometry")
        h1, h2 = a.earth_radius_km+r.observer, a.earth_radius_km+r.target
        # Stable chord-to-central-angle map at near vertical and short ranges.
        beta = 2*math.asin(math.sqrt(max(0., (r.range-abs(h2-h1))*(r.range+abs(h2-h1)))/(4*h1*h2)))
        angle = math.acos(max(-1., min(1., ((h2-h1)*(h2+h1)-r.range*r.range)/(2*h1*r.range))))
        distance, zenith, beta_deg = r.range, 0., 0.
        if r.geometry == "endpoints_beta":
            distance, beta_deg = 0., math.degrees(beta)
        elif r.geometry == "endpoints_angle":
            distance, zenith = 0., math.degrees(angle)
        elif r.geometry not in ("endpoints_range", "horizontal"):
            raise ValueError("Unsupported path representation")
        lenn = 1 if r.geometry == "endpoints_angle" and r.observer == r.target else 0
        lines.append(f"{r.observer:10.6f}{r.target:10.6f}{zenith:10.6f}{distance:10.7f}"
                     f"{beta_deg:10.7f}{a.earth_radius_km:10.3f}{lenn:5d}     "+f"{0.:10.6f}"*2)
    if mode in (2, 4):
        lines += ["".join(f"{v:5d}" for v in (2, 2, 0, 0)),
                  f"{r.relative_azimuth:10.4f}{r.sza:10.5f}"+f"{0.:10.3f}"*6]
    lines += [spectrum(r.band, flux), "    0"]
    return lines
