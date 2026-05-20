
#include "texturePool.h"



Filename modelPath = Filename::from_os_specific("models/F35C.obj");
Filename texturePath = Filename::from_os_specific("models/f35c.jpg");
Filename materialtexturePath = Filename::from_os_specific("models/f35c_mat.tif");



#include "pandaFramework.h"
#include "pandaSystem.h"
#include "texture.h"
#include "shader.h"
#include "nodePath.h"
#include "samplerState.h"  // 必须包含：修复过滤类型错误
#include "lvecBase3.h"

#include "pta_LVecBase4.h"
#include "pta_float.h"
#include "lvecBase4.h"   


PandaFramework framework;
WindowFramework* window;
NodePath model_np;

// ============== 修复点2：拆分结构体为独立数组（Panda3D不支持传struct数组） ==============
// 材质颜色数组 (RGBA)
const LVecBase4f material_colors[3] = {
	LVecBase4f(1.0f, 0.8f, 0.8f, 1.0f),  // 材质0
	LVecBase4f(0.8f, 1.0f, 0.8f, 1.0f),  // 材质1
	LVecBase4f(0.8f, 0.8f, 1.0f, 1.0f)   // 材质2
};
// 材质红外发射率数组
const float material_emissivity[3] = { 0.9f, 0.5f, 0.3f };

void init_scene() {

	// 1. 加载带UV的模型
	model_np = window->load_model(framework.get_models(), modelPath);
	model_np.reparent_to(window->get_render());
	model_np.set_scale(1.0f);
	model_np.set_pos(0,40, 0);
	model_np.set_hpr(0,90,0);
	// 2. 加载【常规纹理】(外观贴图)
	PT(Texture) base_color_tex = TexturePool::load_texture(texturePath);
	if (!base_color_tex) {
		std::cerr << "未找到基础纹理" << std::endl;
		return;
	}
	// ============== 修复点1：使用 SamplerState::FilterType ==============
	base_color_tex->set_minfilter(SamplerState::FT_linear);
	base_color_tex->set_magfilter(SamplerState::FT_linear);

	// 3. 加载【材质ID纹理】(单通道灰度图，像素=材质编号)
	PT(Texture) material_id_tex = TexturePool::load_texture(materialtexturePath);
	if (!material_id_tex) {
		std::cerr << "未找到材质ID纹理" << std::endl;
		return;
	}
	// ============== 修复点1：使用 SamplerState::FT_nearest ==============
	material_id_tex->set_minfilter(SamplerState::FT_nearest);
	material_id_tex->set_magfilter(SamplerState::FT_nearest);

	// 4. 加载着色器
	PT(Shader) shader = Shader::load(
		Shader::SL_GLSL,
		"dual_tex_vert.glsl",
		"dual_tex_frag.glsl"
	);
	model_np.set_shader(shader);

	// 1. 创建 PTA 数组
	PTA_LVecBase4f pta_vec4color;
	pta_vec4color.push_back(LVecBase4f(1.0f, 0.8f, 0.8f, 1.0f));
	pta_vec4color.push_back(LVecBase4f(0.8f, 1.0f, 0.8f, 1.0f));
	pta_vec4color.push_back(LVecBase4f(0.8f, 0.8f, 1.0f, 1.0f));


	// 5. 绑定数据（拆分数组绑定，修复着色器输入错误）
	model_np.set_shader_input("baseColorTex", base_color_tex);    // 常规纹理
	model_np.set_shader_input("materialIDTex", material_id_tex);  // 材质ID纹理
	model_np.set_shader_input("material_colors", pta_vec4color);      // 颜色数组
	model_np.set_shader_input("material_emissivity", LVecBase3f(0.2f, 0.5f, 0.3f)); // 发射率数组
}

int main(int argc, char* argv[]) {
	framework.open_framework(argc, argv);
	framework.set_window_title("Panda3D 双纹理映射 - 修复版");
	window = framework.open_window();

	init_scene();

	framework.main_loop();
	framework.close_framework();
	return 0;
}
