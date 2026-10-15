"""Independent protocol camera path around an existing CustomerDemo cloud.

Only camera input changes. No cloud descriptors, shader overrides, brightness
or original trajectory are modified. NIR legacy visualization; not calibration.
"""
import json
import math
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'logs/p17/review/fixtures'
out.mkdir(parents=True, exist_ok=True)
lat, lon = map(math.radians, (40., 100.))
a, e2 = 6378137., 6.69437999014e-3
n = a / math.sqrt(1 - e2 * math.sin(lat)**2)
origin = (n*math.cos(lat)*math.cos(lon), n*math.cos(lat)*math.sin(lon), n*(1-e2)*math.sin(lat))


def geodetic(x, y, z):
    xx = origin[0]-math.sin(lon)*x-math.sin(lat)*math.cos(lon)*y+math.cos(lat)*math.cos(lon)*z
    yy = origin[1]+math.cos(lon)*x-math.sin(lat)*math.sin(lon)*y+math.cos(lat)*math.sin(lon)*z
    zz = origin[2]+math.cos(lat)*y+math.sin(lat)*z
    p = math.hypot(xx, yy)
    phi = math.atan2(zz, p*(1-e2))
    for _ in range(10):
        nn = a/math.sqrt(1-e2*math.sin(phi)**2)
        h = p/math.cos(phi)-nn
        phi = math.atan2(zz, p*(1-e2*nn/(nn+h)))
    return [math.degrees(phi), math.degrees(math.atan2(yy, xx)), h]


center = (1827.813, -1496.287, 9676.007)
positions = [(0, 1827.813, -7000, 8500), (6, 1827.813, -7000, 8500),
             (12, -1000, -6500, 11200), (18, 1827.813, -3000, 9676.007),
             (24, 1827.813, -1496.287, 9676.007), (30, 1827.813, 1000, 9676.007)]
frames = []
for t, x, y, z in positions:
    dx, dy, dz = (center[0]-x, center[1]-y, center[2]-z)
    # Maintain northward view during traversal; after exit the cloud is behind
    # the camera and is legitimately no longer visible.
    yaw = math.degrees(math.atan2(dx, dy)) if t < 18 else 0.
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy))) if t < 18 else 0.
    frames.append([t, *geodetic(x, y, z), 0, yaw, pitch])
fixture = dict(Schema='ordinary_weather_camera_1', SimulationEpochMs=1788665400000,
               Keyframes=frames, Description=__doc__,
               ExpectedNearbyCloudIds=['D4CB564594352F28'],
               ReferenceCloudENU=list(center), ReferenceCloudSource='nir_cloudy_wide_v4/board.log',
               ScenarioPhases=['0..6 below', '12 above with horizontal parallax',
                               '18..24 approach and enter', '24..30 pass through and exit'])
(out/'cloud_views.json').write_text(json.dumps(fixture, indent=2)+'\n')
reference = geodetic(center[0], 15000, center[2])
reference_fixture = dict(fixture,
    SyntheticTelemetryTargets=[[0x12,100,1,*reference,90,0,0,0,0]],
    SyntheticTelemetryKeyframes=[[t,*reference,90,0,0,0,0] for t in (0,31)],
    Description='Same cloud camera path with an explicit ordinary F22 reference target, engine off; atmosphere range remains above 5 km.')
(out/'cloud_views_reference.json').write_text(json.dumps(reference_fixture, indent=2)+'\n')
(out/'cloud_audit.json').write_text(json.dumps({'WorldCloudAudit': '1'})+'\n')
# Independent opaque-object check: actual F22, fixed camera and fixed weather.
# The target moves between held positions before/inside/behind the same cloud;
# the original replay and target mapping files are untouched.
camera = geodetic(center[0], center[1]-2000, center[2])
target_positions = [geodetic(center[0], center[1]+offset, center[2]) for offset in (-1650, 0, 1400)]
occlusion = dict(Schema='ordinary_weather_camera_1', SimulationEpochMs=1788665400000,
    Keyframes=[[0, *camera, 0, 0, 0], [16, *camera, 0, 0, 0]],
    SyntheticTelemetryTargets=[[0x12, 100, 1, *target_positions[0], 90, 0, 0, 0, 0]],
    SyntheticTelemetryKeyframes=[[t, *target_positions[j], 90, 0, 0, 0, 0]
        for t, j in [(0,0),(5,0),(5.1,1),(10,1),(10.1,2),(16,2)]],
    Description='Independent ordinary protocol F22/cloud depth test; held near, inside, behind; engine off.',
    ReferenceCloudENU=list(center), ExpectedNearbyCloudIds=['D4CB564594352F28'])
(out/'cloud_occlusion.json').write_text(json.dumps(occlusion, indent=2)+'\n')
print(out/'cloud_views.json')
