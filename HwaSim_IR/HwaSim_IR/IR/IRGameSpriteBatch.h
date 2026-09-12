#pragma once

#include "geomVertexData.h"
#include "geomVertexFormat.h"
#include "geomVertexWriter.h"
#include "geomTriangles.h"
#include "geomNode.h"
#include "boundingSphere.h"
#include "shader.h"
#include "texturePool.h"
#include "samplerState.h"
#include "transparencyAttrib.h"
#include <fstream>
#include <sstream>
#include <iostream>

// Artistic sprite geometry only. No calibrated plume or sensor parameters.
// A fixed pool of 32 quads per layer is expanded in eye space on the GPU.
namespace IRGameSpriteBatch {
inline PT(PandaNode) makeGeometry() {
    PT(GeomVertexData) data = new GeomVertexData("GameSpritePool32", GeomVertexFormat::get_v3n3t2(), Geom::UH_static);
    GeomVertexWriter vertex(data, "vertex"), seed(data, "normal"), uv(data, "texcoord");
    PT(GeomTriangles) triangles = new GeomTriangles(Geom::UH_static);
    for (int i = 0; i < 32; ++i) {
        for (int j = 0; j < 4; ++j) {
            const float x = (j == 1 || j == 2) ? 1.f : -1.f;
            const float y = j >= 2 ? 1.f : -1.f;
            vertex.add_data3f(x, y, 0.f);
            seed.add_data3f(float(i), float((i * 13) % 32) / 32.f, float((i * 7) % 32) / 32.f);
            uv.add_data2f(x * .5f + .5f, y * .5f + .5f);
        }
        triangles->add_vertices(i*4, i*4+1, i*4+2);
        triangles->add_vertices(i*4, i*4+2, i*4+3);
    }
    triangles->close_primitive();
    PT(Geom) geom = new Geom(data);
    geom->add_primitive(triangles);
    PT(GeomNode) node = new GeomNode("GameSpriteBatch32");
    node->add_geom(geom);
    node->set_bounds(new BoundingSphere(LPoint3f(0.f,-.5f,0.f), 2.5f));
    node->set_final(true);
    return node;
}
inline std::string read(const char* path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s; s << f.rdbuf(); return s.str();
}
inline bool apply(NodePath& node) {
    static PT(Shader) shader;
    static PT(Texture) atlas;
    if (!shader) {
#ifdef _WIN32
        const std::string version = "#version 130\n";
#else
        const std::string version = "#version 300 es\nprecision highp float;\nprecision highp int;\n";
#endif
        shader = Shader::make(Shader::SL_GLSL,
            version + read("Config/GameVFX/sprite.vert"), version + read("Config/GameVFX/sprite.frag"));
        atlas = TexturePool::load_texture("Config/GameVFX/soft_sprite_atlas.png");
        if (atlas) {
            atlas->set_minfilter(SamplerState::FT_linear);
            atlas->set_magfilter(SamplerState::FT_linear);
            atlas->set_wrap_u(SamplerState::WM_clamp);
            atlas->set_wrap_v(SamplerState::WM_clamp);
        }
    }
    if (!shader || !atlas) return false;
    static bool logged=false;
    if(!logged) {
        logged=true;
        std::cout<<"[GameSpriteResources] shaderReady=1 atlas="<<atlas->get_fullpath()<<" size="<<atlas->get_x_size()<<"x"<<atlas->get_y_size()<<" vertices=128 particles=32 domain=common_linear"<<std::endl;
    }
    node.set_shader(shader, 100);
    node.set_shader_input("u_sprite_atlas", atlas);
    node.set_shader_input("u_sprite_time", LVecBase2f(0,0));
    node.set_shader_input("u_sprite_lod", LVecBase2f(32,0));
    node.set_depth_test(true);
    node.set_depth_write(false);
    node.set_transparency(TransparencyAttrib::M_alpha);
    return true;
}
}
