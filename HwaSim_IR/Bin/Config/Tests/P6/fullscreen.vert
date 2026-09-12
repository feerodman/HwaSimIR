in vec4 p3d_Vertex;
in vec2 p3d_MultiTexCoord0;
out vec2 test_uv;
void main(){test_uv=p3d_MultiTexCoord0;gl_Position=vec4(p3d_Vertex.x,p3d_Vertex.z,.9999,1.0);}
