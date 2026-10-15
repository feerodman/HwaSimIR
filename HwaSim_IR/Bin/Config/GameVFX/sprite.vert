// P17 one conservative proxy per emitter; continuous local gas.
uniform mat4 p3d_ModelViewMatrix;
uniform mat4 p3d_ProjectionMatrix;
uniform vec3 u_sprite_art;
uniform vec3 u_sprite_nozzle_offset;
uniform vec2 u_sprite_aspect;
uniform float u_sprite_emitters;
uniform float u_plume_radius_root;
uniform float u_plume_radius_tail;
uniform int u_plume_layer;
in vec4 p3d_Vertex;
in vec3 p3d_Normal;
out vec3 gas_ray_origin;
out vec3 gas_ray_point;
out vec3 gas_metric;
out float gas_aspect;
vec3 toLocalVector(vec3 v) {
    vec3 a=p3d_ModelViewMatrix[0].xyz,b=p3d_ModelViewMatrix[1].xyz,c=p3d_ModelViewMatrix[2].xyz;
    return vec3(dot(v,cross(b,c)),dot(v,cross(c,a)),dot(v,cross(a,b)))/dot(a,cross(b,c));
}
void main() {
    if(p3d_Normal.x>.5 || p3d_Vertex.z>=u_sprite_emitters) {
        gl_Position=vec4(2,2,2,1);gas_ray_origin=vec3(0);gas_ray_point=vec3(0);
        gas_metric=vec3(1);gas_aspect=1.0;return;
    }
    // Authored nozzle node scale is radiusX / .4 (the old unit sprite root).
    float root=u_sprite_aspect.y>.5?.4:1.0;
    float wakeEnd=u_plume_layer==2?2.6:1.2;
    float wakeWidth=u_plume_layer==2?2.6:1.0;
    float radius=root*max(1.0,clamp(u_plume_radius_tail/max(u_plume_radius_root,.001),.5,6.0))*u_sprite_art.y*wakeWidth;
    gas_aspect=u_sprite_aspect.y>.5?max(.1,u_sprite_aspect.x):1.0;
    vec3 offset=u_sprite_nozzle_offset*p3d_Vertex.z;
    vec3 center=offset+vec3(0,-.5*u_sprite_art.x*wakeEnd,0);
    vec4 eye=p3d_ModelViewMatrix*vec4(center,1);
    gas_metric=vec3(length(p3d_ModelViewMatrix[0].xyz),length(p3d_ModelViewMatrix[1].xyz),length(p3d_ModelViewMatrix[2].xyz));
    float sphere=length(vec3(radius,.5*u_sprite_art.x*wakeEnd,radius*gas_aspect)*gas_metric);
    float extent=sphere*max(abs(eye.z),.01)/max(abs(eye.z)-sphere,.01);
    eye.xy+=p3d_Vertex.xy*extent;
    gas_ray_origin=toLocalVector(-p3d_ModelViewMatrix[3].xyz)-offset;
    gas_ray_point=toLocalVector(eye.xyz-p3d_ModelViewMatrix[3].xyz)-offset;
    gl_Position=p3d_ProjectionMatrix*eye;
}
