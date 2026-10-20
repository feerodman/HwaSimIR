"""Development-only spectral local-transport overlap test, never band-mean power."""
import argparse
from dataclasses import asdict, replace
import hashlib
import math
from pathlib import Path
import p20m_modtran as engine
import modtran_convert_to_si as convert
import p11_modtran_humidity_qc as metrics
from p20m2_template import Atmosphere, Request, deck

def prepare(root, resolution=1., bands=("SWIR","MWIR")):
    cases=[]
    for band in bands:
        for altitude in ((20.,) if resolution==.1 else (3.,10.,20.)):
            for rh in ((30.,) if resolution==.1 else (30.,85.)):
                for visibility in ((6.,) if resolution==.1 else (6.,23.)):
                    atmosphere=replace(Atmosphere(),surface_rh_percent=rh,visibility_km=visibility)
                    requests=[(Request(band,mode,altitude,altitude,.001,geometry="horizontal"),"coefficient")
                              for mode in ("Transmittance","ThermalRadiance")]
                    for distance in (.0001,.0005,.001):
                        for slope in (.5,1.):
                            target=altitude+(1 if altitude<20 else -1)*distance*slope
                            for mode in ("Transmittance","ThermalRadiance"):
                                requests.append((Request(band,mode,altitude,target,distance,
                                  geometry="endpoints_angle" if slope==1 else "endpoints_range"),"independent_overlap"))
                    for request,role in requests:
                        lines=deck(atmosphere,request)
                        if resolution==.1:
                            lines[2]="p1_2009"
                            lines[-2]=lines[-2][:20]+f"{.1:10.3f}{.1:10.3f}"+lines[-2][40:]
                            if band=="SWIR":
                                lines[-2]=f"{3999.:10.3f}"+lines[-2][10:]
                        content="\n".join(engine.database_deck(lines))+"\n"
                        sha=hashlib.sha256(content.encode("ascii")).hexdigest()
                        case_id=band+"_"+request.mode+"_"+sha[:16]
                        path=root/"generated"/case_id/"input.tp5"
                        path.parent.mkdir(parents=True,exist_ok=True)
                        if path.exists() and path.read_bytes()!=content.encode("ascii"):
                            raise ValueError("Immutable deck differs")
                        path.write_bytes(content.encode("ascii"))
                        cases.append(dict(case_id=case_id,**asdict(request),atmosphere=asdict(atmosphere),
                                          rh=rh,visibility=visibility,role=role,
                                          spectral_resolution_cm1=resolution,
                                          input_file=str(path),input_sha256=sha,
                                          template_id=atmosphere.identity()))
    result=dict(schema="HwaSimIR.P20M2.Cases.1",status="NEAR_FIELD_DEVELOPMENT_NOT_HOLDOUT",cases=cases)
    path=root/"case_manifest.json"
    if path.exists() and engine.read_json(path)!=result:
        raise ValueError("Immutable cases differ")
    engine.atomic_json(path,result)
    print("Prepared",len(cases))

def analyze(root):
    cases=engine.read_json(root/"case_manifest.json")["cases"]
    def key(c): return c["band"],c["observer"],c["rh"],c["visibility"]
    coefficients={}; coefficient_errors={}
    def read(case):
        raw=root/"raw"/case["case_id"]
        cp=engine.read_json(raw/"checkpoint.json")
        if cp["status"]!="PASS" or cp["qc"]["geometry"]["status"]!="PASS_ACTUAL_PRINTED_GEOMETRY":
            raise ValueError("Unreliable independent reference: "+case["case_id"])
        for name,sha in cp["output_sha256"].items():
            if engine.digest(raw/name)!=sha: raise ValueError("Changed raw")
        return (convert.parse_transmittance if case["mode"]=="Transmittance" else convert.parse_radiance)(raw/"MODOUT2.txt")
    for c in cases:
        if c["role"]=="coefficient":
            try:
                coefficients.setdefault(key(c),{})[c["mode"]]=(c,read(c))
            except ValueError as exc:
                coefficient_errors[key(c)]=str(exc)
    results=[]
    for c in cases:
        if c["role"]!="independent_overlap": continue
        result=dict(case_id=c["case_id"],band=c["band"],mode=c["mode"],
                    range_km=c["range"],observer_km=c["observer"],target_km=c["target"],
                    rh=c["rh"],visibility=c["visibility"],release_eligible=False)
        try:
            if key(c) in coefficient_errors:
                raise ValueError(coefficient_errors[key(c)])
            reference=read(c)
            tau_case,tau_rows=coefficients[key(c)]["Transmittance"]
            thermal_case,thermal_rows=coefficients[key(c)]["ThermalRadiance"]
            tau={r["wavenumber_cm1"]:r["tau_los"] for r in tau_rows}
            thermal={r["wavenumber_cm1"]:r["path_thermal_W_m2_sr_um"] for r in thermal_rows}
            ratio=c["range"]/.001
            output=[]
            field="tau_los" if c["mode"]=="Transmittance" else "path_thermal_W_m2_sr_um"
            for row in reference:
                wn=row["wavenumber_cm1"]; t=tau[wn]
                if not 0<t<=1:
                    raise ValueError("Opaque coefficient bin prevents validated short-range extension")
                lost=-math.expm1(math.log(t)*ratio)
                v=(1-lost) if field=="tau_los" else thermal[wn]*(lost/(1-t) if t<1 else ratio)
                output.append(dict(wavelength_um=row["wavelength_um"],value=v))
            lo,hi=engine.humidity.BANDS[c["band"]]["lo_um"],engine.humidity.BANDS[c["band"]]["hi_um"]
            ref=metrics.response_integral(reference,field,lo,hi)/(hi-lo)
            test=metrics.response_integral(output,"value",lo,hi)/(hi-lo)
            result.update(status="MEASURED_NOT_ACCEPTED",reference=ref,test=test,
                          absolute_error=abs(test-ref),relative_error=abs(test-ref)/abs(ref) if ref else None,
                          coefficient_cases=[tau_case["case_id"],thermal_case["case_id"]])
        except ValueError as exc:
            result.update(status="NOT_VERIFIED",error=str(exc))
        results.append(result)
    measured=[r for r in results if r["status"]=="MEASURED_NOT_ACCEPTED"]
    report=dict(schema="HwaSimIR.P20M2.LocalSpectralTransport.1",
                analysis_sha256=engine.digest(__file__),
                formula="tau(lambda,r)=exp(log(tau(lambda,1m))*r/1m); thermal uses matched spectral source with transparent-bin limit",
                spectral_resolution_cm1=sorted(set(c.get("spectral_resolution_cm1",1.) for c in cases)),
                limitations=["Finite band-model bins are not monochromatic; overlap error includes unresolved spectral averaging",
                             "No claimed continuous height/weather/direction domain; no runtime implementation",
                             "Final component/AGC-before error budgets not frozen; development points are not holdout"],
                component_comparison_count=len(results),
                physical_state_count=len({(r["observer_km"],r["target_km"],r["range_km"],
                                           r["rh"],r["visibility"]) for r in results}),
                measured=len(measured),results=results,
                worst_tau_absolute=max((r["absolute_error"] for r in measured if r["mode"]=="Transmittance"),default=None),
                worst_thermal_relative=max((r["relative_error"] for r in measured if r["mode"]=="ThermalRadiance"),default=None),
                release_eligible=False)
    engine.atomic_json(root/"overlap_report.json",report)
    print({k:v for k,v in report.items() if k!="results"})

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("action",choices=("prepare","analyze"))
    p.add_argument("--root",type=Path,required=True)
    p.add_argument("--resolution",type=float,choices=(1.,.1),default=1.)
    p.add_argument("--bands",choices=("SWIR","MWIR"),nargs="+",default=["SWIR","MWIR"])
    a=p.parse_args();root=a.root.resolve();root.relative_to(engine.ROOT/"logs/p20m2")
    root.mkdir(parents=True,exist_ok=True)
    if a.action=="prepare": prepare(root,a.resolution,a.bands)
    else: analyze(root)
