uniform mat4 p3d_ModelViewProjectionMatrix;
uniform vec2 u_precip_time;
uniform vec4 u_precip_state; // type, density, speed, common-linear source
uniform vec2 u_precip_fov; // tan(horizontal/vertical half FOV)
uniform vec3 u_precip_velocity; // world wind and vertical fall transformed to camera axes
uniform vec3 u_precip_up;
uniform vec3 u_precip_height; // camera public-world altitude, maximum, transition thickness
uniform vec2 u_precip_viewport; // final sensor pixels, not the window/widget size
in vec4 p3d_Vertex;
in vec3 p3d_Normal;
in vec2 p3d_MultiTexCoord0;
out vec2 particle_uv;
out float particle_alpha;
void main() {
    bool snow=u_precip_state.x>1.5;
    float depthPhase=fract(p3d_Normal.z+u_precip_velocity.y*u_precip_time.x/79.0);
    float distance=mix(6.0,85.0,depthPhase*depthPhase);
    vec2 halfExtent=max(vec2(.5),u_precip_fov*distance*1.12);
    // Deterministic phase survives texture/weather changes. No wall-clock animation.
    vec2 drift=u_precip_velocity.xz*u_precip_time.x/(halfExtent*2.0);
    vec2 phase=fract(p3d_Normal.xy+drift);
    vec3 center=vec3((phase.x*2.0-1.0)*halfExtent.x,distance,(phase.y*2.0-1.0)*halfExtent.y);
    float fadeEdge=smoothstep(0.0,.055,phase.x)*(1.0-smoothstep(.945,1.0,phase.x));
    fadeEdge*=smoothstep(0.0,.055,phase.y)*(1.0-smoothstep(.945,1.0,phase.y));
    fadeEdge*=smoothstep(0.0,.025,depthPhase)*(1.0-smoothstep(.975,1.0,depthPhase));
    float altitude=u_precip_height.x+dot(center,u_precip_up);
    float top=u_precip_height.y,thickness=max(1.0,u_precip_height.z);
    // Maximum is the zero-precipitation ceiling; transition lies below it.
    float heightFade=top>0.0?1.0-smoothstep(top-thickness,top,altitude):0.0;
    float size=snow?mix(.026,.075,p3d_Normal.z):mix(.0015,.0035,p3d_Normal.z);
    vec2 along=u_precip_velocity.xz;
    along=length(along)>.0001?normalize(along):vec2(0,-1);
    // Preserve the front-facing quad winding for every wind direction.
    vec2 crossAxis=vec2(along.y,-along.x);
    float lengthScale=snow?1.0:mix(40.0,70.0,p3d_Normal.z);
    // Preserve authored metric size, but keep a rain shaft from disappearing
    // between raster samples.  This is a screen-space coverage floor, not an
    // opacity/radiance gain, and follows the actual sensor resolution.
    vec2 viewport=max(u_precip_viewport,vec2(1.0));
    float onePixelWorldX=(halfExtent.x*2.0)/viewport.x;
    float crossSize=snow?size:max(size,onePixelWorldX*.72);
    float alongSize=snow?size:size*lengthScale;
    vec2 corner=crossAxis*p3d_Vertex.x*crossSize+along*p3d_Vertex.z*alongSize;
    if(snow) {
        float a=p3d_Normal.z*6.2831853+u_precip_time.x*.35;
        corner=mat2(cos(a),-sin(a),sin(a),cos(a))*p3d_Vertex.xz*size;
    }
    center.xz+=corner;
    gl_Position=p3d_ModelViewProjectionMatrix*vec4(center,1);
    particle_uv=p3d_MultiTexCoord0;
    particle_alpha=fadeEdge*heightFade*clamp(u_precip_state.y,0.0,1.0)*(snow?.48:.23)*mix(1.0,.45,p3d_Normal.z);
}
