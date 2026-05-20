#version 130

// Panda3D 自动传入的顶点属性（标准写法）
in vec3 p3d_Vertex;
in vec2 p3d_MultiTexCoord0;

// 输出 UV 给片元着色器
out vec2 uv;

// Panda3D 内置 MVP 矩阵（替代废弃的 gl_ModelViewProjectionMatrix）
uniform mat4 p3d_ModelViewProjectionMatrix;

void main() {
    gl_Position = p3d_ModelViewProjectionMatrix * vec4(p3d_Vertex, 1.0);
    uv = p3d_MultiTexCoord0;
}