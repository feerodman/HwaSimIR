uniform sampler2D p3d_Texture0;
uniform vec4 u_precip_state;
uniform int u_stage6_raw_si_domain;
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
    // Formal mode receives a sensor-plane band-mean source function in
    // W/(m^2 sr um).  The texture supplies coverage only.  Legacy mode retains
    // the authored display-only snow value.
    float source=u_stage6_raw_si_domain==1?max(0.0,u_precip_state.w):
        (u_precip_state.x>1.5?0.60:u_precip_state.w);
    fragColor=vec4(vec3(source),alpha);
}
