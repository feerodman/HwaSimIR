uniform sampler2D p3d_Texture0;
uniform vec4 u_precip_state;
in vec2 particle_uv;
in float particle_alpha;
out vec4 fragColor;
void main() {
    // The SGI RGBA alpha contains the droplet/flake mask. Its gray RGB background
    // is not density, and a .rgba extension does not imply raw pixel bytes.
    float mask=texture(p3d_Texture0,particle_uv).a;
    vec2 d=abs(particle_uv*2.0-1.0);
    float edge=(1.0-smoothstep(.75,1.0,d.x))*(1.0-smoothstep(.8,1.0,d.y));
    float alpha=mask*edge*particle_alpha;
    if(alpha<.001)discard;
    // Authored game snow source in the common-linear display domain, not temperature.
    float source=u_precip_state.x>1.5?0.60:u_precip_state.w;
    fragColor=vec4(vec3(source),alpha);
}
