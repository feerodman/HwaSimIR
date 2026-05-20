#version 130

// 接收顶点着色器传递的 UV
in vec2 uv;

// 纹理采样器（与C++代码完全匹配）
uniform sampler2D baseColorTex;
uniform sampler2D materialIDTex;

// ===================== 【关键修复】正确声明数组，无任何语法错误 =====================
uniform vec4 material_colors[3];      // 对应 C++ PTA_LVecBase4f（3个四维向量）
uniform vec3 material_emissivity;     // 对应 C++ LVecBase3f

// 输出最终颜色
out vec4 fragColor;
	
void main() {
    // 1. 采样基础纹理
    vec4 baseColor = texture(baseColorTex, uv);
    
    // 2. 采样材质ID（灰度图 R 通道 = 材质编号）
    float matID = texture(materialIDTex, uv).r;
    int index = int(matID * 2.0); // 0~1 → 索引 0/1/2
    
    // 3. 混合颜色
    vec4 finalColor = baseColor * material_colors[index];
    
    // 输出
    fragColor = finalColor;
}