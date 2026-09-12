in vec2 test_uv;
out vec4 fragColor;
void main(){
    // Artistic environment haze, common linear units; no screen blur.
    float y=test_uv.y;
    float g=mix(.30,.12,smoothstep(.22,.82,y));
    g+=.045*exp(-pow((y-.5)/.16,2.0));
    fragColor=vec4(vec3(g),1.0);
}
