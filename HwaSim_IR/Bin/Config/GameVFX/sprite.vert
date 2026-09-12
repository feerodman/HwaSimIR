// Public-domain mathematical game effect; normalized model-local emission axis -Y.
uniform mat4 p3d_ModelViewMatrix;
uniform mat4 p3d_ProjectionMatrix;
uniform float u_sprite_time;
uniform float u_sprite_lod;
uniform int u_plume_layer;
uniform vec4 u_game_sprite; // explicit ordinary-nozzle art only, zero in production
uniform vec3 u_sprite_nozzle_offset; // normalized second mesh nozzle, zero for one
uniform vec2 u_sprite_aspect; // vertical/horizontal radius; second value enables asset attachment
uniform float u_sprite_emitters;
in vec4 p3d_Vertex;
in vec3 p3d_Normal; // pool index and deterministic phase, not surface normals
in vec2 p3d_MultiTexCoord0;
out vec2 sprite_uv;
out float sprite_weight;
void main() {
    float count = clamp(u_sprite_lod, 8.0, 32.0);
    float rank = p3d_Normal.x;
    // Smooth fractional last particle plus optical-depth normalization avoids LOD brightness doubling.
    float enabled = clamp(count - rank, 0.0, 1.0);
    enabled *= 1.0-step(u_sprite_emitters,p3d_Vertex.z);
    float phase = (p3d_Normal.z * 32.0 + .5) / 32.0;
    float age = fract(u_sprite_time * .65 + phase);
    float halo = u_plume_layer == 2 ? 1.0 : 0.0;
    float angle = p3d_Normal.y * 6.2831853 + age * 1.4;
    vec3 center = vec3(cos(angle), 0.0, sin(angle)) * age * (.10 + halo * .15);
    center.y = -age;
    center += u_sprite_nozzle_offset * p3d_Vertex.z;
    vec4 eye = p3d_ModelViewMatrix * vec4(center, 1.0);
    float radius = length(p3d_ModelViewMatrix[0].xyz) * mix(.32, .57 + halo*.18, age);
    if(u_game_sprite.x>0.5)radius=length(p3d_ModelViewMatrix[0].xyz)*mix(.6,.35+halo*.7,age);
    // Each soft sprite faces the view. The emission axis and center remain in model space.
    vec2 corner=p3d_Vertex.xy;
    if(u_sprite_aspect.y>0.5){
        vec2 axis=p3d_ModelViewMatrix[0].xy;
        axis=length(axis)>.0001?normalize(axis):vec2(1,0);
        corner=axis*corner.x+vec2(-axis.y,axis.x)*corner.y*u_sprite_aspect.x;
    }
    eye.xy += corner * radius;
    gl_Position = p3d_ProjectionMatrix * eye;
    sprite_uv = vec2((p3d_MultiTexCoord0.x + halo) * .5, p3d_MultiTexCoord0.y);
    sprite_weight = enabled * (32.0 / count) * smoothstep(0.0,.10,age) * (1.0-smoothstep(.60,1.0,age));
    if(u_game_sprite.x>0.5){
        sprite_weight*=u_game_sprite.y*exp(-age*u_game_sprite.z);
        if(halo>.5)sprite_weight*=smoothstep(.12,.38,age)*mix(.15,1.0,p3d_Normal.y);
    }
}
