#pragma once
#include "IRGameSpriteBatch.h"

// One immutable quad batch. Positions, wind, fall and altitude fading are
// evaluated in the vertex shader; no per-frame particle allocation or readback.
namespace IRPrecipitationBatch {
inline NodePath create(const NodePath& camera, int count) {
    PT(GeomVertexData) data = new GeomVertexData("WeatherParticlePool", GeomVertexFormat::get_v3n3t2(), Geom::UH_static);
    GeomVertexWriter vertex(data,"vertex"), seed(data,"normal"), uv(data,"texcoord");
    PT(GeomTriangles) triangles=new GeomTriangles(Geom::UH_static);
    for(int i=0;i<count;++i) {
        for(int j=0;j<4;++j) {
            const float x=(j==1||j==2)?1.f:-1.f,z=j>=2?1.f:-1.f;
            vertex.add_data3f(x,0,z);
            seed.add_data3f((float((i*73+19)%997)+.5f)/997.f,
                (float((i*193+43)%991)+.5f)/991.f,(float((i*331+61)%983)+.5f)/983.f);
            uv.add_data2f(x*.5f+.5f,z*.5f+.5f);
        }
        triangles->add_vertices(i*4,i*4+1,i*4+2);triangles->add_vertices(i*4,i*4+2,i*4+3);
    }
    triangles->close_primitive();PT(Geom) geom=new Geom(data);geom->add_primitive(triangles);
    PT(GeomNode) geometry=new GeomNode("WeatherParticleBatch");geometry->add_geom(geom);
    geometry->set_bounds(new BoundingSphere(LPoint3f(0,60,0),500));geometry->set_final(true);
    NodePath node=camera.attach_new_node(geometry);
#ifdef _WIN32
    const std::string version="#version 130\n";
#else
    const std::string version="#version 300 es\nprecision highp float;\nprecision highp int;\n";
#endif
    PT(Shader) shader=Shader::make(Shader::SL_GLSL,
        version+IRGameSpriteBatch::read("Config/Weather/precipitation.vert"),
        version+IRGameSpriteBatch::read("Config/Weather/precipitation.frag"));
    if(!shader) {node.remove_node();return NodePath();}
    node.set_shader(shader,100);node.set_transparency(TransparencyAttrib::M_alpha);
    node.set_depth_test(true);node.set_depth_write(false);node.set_bin("transparent",30);
    node.set_shader_input("u_precip_time",LVecBase2f(0,0));
    node.set_shader_input("u_precip_state",LVecBase4f(0,0,0,0));
    node.set_shader_input("u_precip_fov",LVecBase2f(.08f,.08f));
    node.set_shader_input("u_precip_velocity",LVecBase3f(0,0,-1));
    node.set_shader_input("u_precip_up",LVecBase3f(0,0,1));
    node.set_shader_input("u_precip_height",LVecBase3f(0,0,0));
    node.set_shader_input("u_stage6_raw_si_domain",LVecBase2i(0,0));
    node.hide();
    std::cout<<"[PrecipitationBatch] particles="<<count<<" draws=1 vertices="<<count*4
        <<" allocation=INIT priority=100 shader=Config/Weather/precipitation.frag"
        <<" rgbContract=formal_W_per_m2_sr_um_or_explicit_legacy_linear"
        <<" alpha=straight_coverage depthTest=1 depthWrite=0 time=protocol_simulation\n";
    return node;
}
}
