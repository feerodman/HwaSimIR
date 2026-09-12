uniform sampler2D u_sprite_atlas;
uniform int u_plume_enabled;
uniform int u_plume_layer;
uniform vec4 u_game_sprite;
uniform float u_plume_gray;
uniform float u_plume_opacity;
uniform float u_stage7_fog_gray;
uniform float u_stage7_fog_density;
uniform float u_stage7_target_contrast_scale;
in vec2 sprite_uv;
in float sprite_weight;
out vec4 fragColor;
void main() {
    if (u_plume_enabled != 1) discard;
    float mask = texture(u_sprite_atlas, sprite_uv).r;
    bool smoke = u_plume_layer == 2;
    float opticalDepth = mask * sprite_weight * clamp(u_plume_opacity,0.0,1.0) * (smoke ? .12 : .18);
    float alpha = 1.0-exp(-opticalDepth);
    if (alpha < .001) discard;
    // Common dimensionless linear scene scale, before Stage6 gamma/polarity.
    // Soft luminous color and absorptive smoke use bounded source-over, never white additive saturation.
    float source = clamp(u_plume_gray,0.0,1.0) * (smoke ? .65 : 1.0);
    if(u_game_sprite.x>.5)source=clamp(u_plume_gray,0.0,1.0);
    source = clamp(u_stage7_fog_gray+(source-u_stage7_fog_gray)*clamp(u_stage7_target_contrast_scale,.05,1.5),0.0,1.0);
    source = mix(source,clamp(u_stage7_fog_gray,0.0,1.0),clamp(u_stage7_fog_density,0.0,.78));
    fragColor = vec4(vec3(source),alpha);
}
