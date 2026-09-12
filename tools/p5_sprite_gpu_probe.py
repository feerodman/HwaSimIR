"""Exercise the shipped sprite shaders on a native Panda GPU context."""
from pathlib import Path
import math
from panda3d.core import *
from direct.showbase.ShowBase import ShowBase
ROOT=Path(__file__).resolve().parents[1]
loadPrcFileData('', 'window-type offscreen\nwin-size 512 512\naudio-library-name null\nframebuffer-srgb false')
app=ShowBase();app.setBackgroundColor(.2,.2,.2,1)
app.camLens.setFov(35);app.camLens.setNear(.1)
data=GeomVertexData('probe',GeomVertexFormat.getV3n3t2(),Geom.UHStatic)
v=GeomVertexWriter(data,'vertex');n=GeomVertexWriter(data,'normal');uv=GeomVertexWriter(data,'texcoord')
tri=GeomTriangles(Geom.UHStatic)
for i in range(32):
    for j in range(4):
        x=1 if j in (1,2) else -1;y=1 if j>=2 else -1
        v.addData3(x,y,0);n.addData3(i,((i*13)%32)/32,((i*7)%32)/32);uv.addData2(x*.5+.5,y*.5+.5)
    tri.addVertices(i*4,i*4+1,i*4+2);tri.addVertices(i*4,i*4+2,i*4+3)
tri.closePrimitive();g=Geom(data);g.addPrimitive(tri);gn=GeomNode('sprites');gn.addGeom(g)
gn.setBounds(BoundingSphere(Point3(0,-.5,0),2.5));gn.setFinal(True)
node=app.render.attachNewNode(gn);node.setScale(1,8,1);node.setTwoSided(True)
folder=ROOT/'HwaSim_IR/Bin/Config/GameVFX'
node.setShader(Shader.make(Shader.SL_GLSL,'#version 130\n'+(folder/'sprite.vert').read_text(),'#version 130\n'+(folder/'sprite.frag').read_text()))
node.setShaderInput('u_sprite_atlas',app.loader.loadTexture(Filename.fromOsSpecific(str(folder/'soft_sprite_atlas.png'))))
for k,x in dict(u_sprite_time=3.0,u_sprite_lod=32.0,u_plume_gray=.8,u_plume_opacity=.8,u_stage7_fog_gray=.2,u_stage7_fog_density=0.,u_stage7_target_contrast_scale=1.).items():node.setShaderInput(k,x)
node.setShaderInput('u_plume_enabled',1);node.setShaderInput('u_plume_layer',1)
node.setShaderInput('u_game_sprite',Vec4(0,1,1,0));node.setShaderInput('u_sprite_nozzle_offset',Vec3(0,0,0))
node.setShaderInput('u_sprite_aspect',Vec2(1,0));node.setShaderInput('u_sprite_emitters',1.0)
node.setTransparency(TransparencyAttrib.MAlpha);node.setDepthWrite(False);node.setDepthTest(True)
for name,pos in [('end',(0,-20,0)),('side',(20,-4,0)),('oblique',(14,-16,5))]:
    app.camera.setPos(*pos);app.camera.lookAt(0,-4,0)
    for _ in range(3):app.graphicsEngine.renderFrame()
    app.win.saveScreenshot(Filename.fromOsSpecific(str(ROOT/'logs/p5'/('probe_'+name+'.png'))))
app.destroy()
