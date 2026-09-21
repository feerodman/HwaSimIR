uniform vec3 u_sprite_art; // length, radius, luminous art multipliers only
uniform sampler2D u_sprite_atlas;
uniform sampler2D u_weather_smoke;
uniform int u_plume_enabled;
uniform int u_plume_layer;
uniform vec4 u_game_sprite;
uniform float u_plume_gray;
uniform float u_plume_opacity;
uniform float u_stage7_fog_gray;
uniform float u_stage7_fog_density;
uniform float u_stage7_target_contrast_scale;
uniform int u_stage6_raw_si_domain;
in vec2 sprite_uv;
in float sprite_weight;
out vec4 fragColor;
void main() {
    if (u_plume_enabled != 1) discard;
    bool smoke = u_plume_layer == 2;
    // Core keeps the authored thermal sprite.  The diffuse halo uses the
    // Weather smoke alpha strictly as spatial coverage; RGB never becomes a
    // radiance, temperature, gain, or atmospheric substitute.
    vec2 smokeUv=vec2(clamp((sprite_uv.x-.5)*2.0,0.0,1.0),sprite_uv.y);
    float mask = smoke ? texture(u_weather_smoke,smokeUv).a
                       : texture(u_sprite_atlas,sprite_uv).r;
    float opacity = clamp(u_plume_opacity,0.0,1.0);
    float densityWeight = max(0.0,mask*sprite_weight);
    // The profile value is a layer opacity, not optical depth.  This conversion
    // makes alpha==opacity for a unit-density texel and applies the spatial mask
    // exactly once.  RGB remains the unpremultiplied source function.
    float alpha = 1.0-pow(max(1.0-opacity,0.000001),densityWeight);
    if (alpha < .001) discard;
    float source;
    if(u_stage6_raw_si_domain==1){
        // W/(m^2 sr um), rectangular-band mean, already propagated through
        // the target-to-sensor LOS by the CPU.  Do not clamp to display gray,
        // reapply empirical fog, or multiply alpha into RGB.
        source=max(0.0,u_plume_gray);
    }else{
        // Explicit legacy display-only path.
        source=clamp(u_plume_gray,0.0,1.0)*(smoke?.65:1.0);
        if(u_game_sprite.x>.5)source=clamp(u_plume_gray,0.0,1.0);
        if(!smoke)source*=u_sprite_art.z;
        source=clamp(u_stage7_fog_gray+(source-u_stage7_fog_gray)*clamp(u_stage7_target_contrast_scale,.05,1.5),0.0,1.0);
        source=mix(source,clamp(u_stage7_fog_gray,0.0,1.0),clamp(u_stage7_fog_density,0.0,.78));
    }
    fragColor = vec4(vec3(source),alpha);
}
