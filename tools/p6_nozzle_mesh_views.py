"""Inspect existing OBJ nozzle geometry in local coordinates, without asset edits."""
from pathlib import Path
from panda3d.core import *
from direct.showbase.ShowBase import ShowBase
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p6/nozzle_mesh';OUT.mkdir(parents=True,exist_ok=True)
loadPrcFileData('', 'window-type offscreen\nwin-size 800 800\naudio-library-name null\nload-file-type p3assimp')
app=ShowBase();app.setBackgroundColor(.13,.15,.17,1)
app.camLens.setFov(35);app.camLens.setNear(.01)
light=AmbientLight('ambient');light.setColor((.65,.65,.65,1));app.render.setLight(app.render.attachNewNode(light))
sun=DirectionalLight('sun');sun.setColor((.7,.7,.7,1));sn=app.render.attachNewNode(sun);sn.setHpr(35,-25,0);app.render.setLight(sn)
for name,file in [('f35','F35C.obj'),('f22','f22.obj'),('aim120','AIM120.obj'),('aim9x','aim9x.obj')]:
    model=app.loader.loadModel(Filename.fromOsSpecific(str(ROOT/'HwaSim_IR/Bin/Config/TargetLib/models'/name/file)))
    model.reparentTo(app.render);model.setTwoSided(True)
    bounds=model.getTightBounds();lo,hi=bounds;length=hi.y-lo.y
    print(name,'bounds',lo,hi)
    # These are model-local inspection views, not real equipment references.
    focus=Point3(0,lo.y+length*.12,(lo.z+hi.z)*.3)
    dist=length*.85
    for view,offset in [('rear',Vec3(0,-dist,0)),('side',Vec3(dist,0,0)),('oblique',Vec3(dist*.55,-dist*.85,dist*.2))]:
        app.camera.setPos(focus+offset);app.camera.lookAt(focus)
        for _ in range(4):app.graphicsEngine.renderFrame()
        app.win.saveScreenshot(Filename.fromOsSpecific(str(OUT/f'{name}_{view}.png')))
    model.removeNode()
app.destroy()
