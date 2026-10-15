// P17 bounded analytic-density integral: no atlas seam, respawn or LOD phase.
uniform vec3 u_sprite_art;
uniform float u_sprite_time;
uniform vec2 u_sprite_aspect;
uniform int u_plume_enabled;
uniform int u_plume_layer;
uniform float u_plume_gray;
uniform float u_plume_opacity;
uniform float u_plume_radius_root;
uniform float u_plume_radius_tail;
uniform float u_plume_axial_decay;
uniform float u_plume_radial_decay;
uniform float u_plume_noise_scale;
uniform float u_plume_noise_strength;
uniform int u_stage6_raw_si_domain;
in vec3 gas_ray_origin;
in vec3 gas_ray_point;
in vec3 gas_metric;
in float gas_aspect;
out vec4 fragColor;
void main() {
    if(u_plume_enabled!=1)discard;
    float tail=clamp(u_plume_radius_tail/max(u_plume_radius_root,.001),.5,6.0);
    float root=u_sprite_aspect.y>.5?.4:1.0;
    bool wake=u_plume_layer==2;
    float wakeEnd=wake?2.6:1.2;
    float bound=root*max(1.0,tail)*u_sprite_art.y*(wake?2.6:1.0);
    vec3 extent=vec3(bound,.5*u_sprite_art.x*wakeEnd,bound*gas_aspect);
    vec3 center=vec3(0,-extent.y,0);
    vec3 ray=normalize(gas_ray_point-gas_ray_origin);
    vec3 safeRay=vec3(abs(ray.x)<1e-7?1e-7:ray.x,abs(ray.y)<1e-7?1e-7:ray.y,abs(ray.z)<1e-7?1e-7:ray.z);
    vec3 nearT=(center-extent-gas_ray_origin)/safeRay;
    vec3 farT=(center+extent-gas_ray_origin)/safeRay;
    vec3 lo=min(nearT,farT),hi=max(nearT,farT);
    float enter=max(0.0,max(lo.x,max(lo.y,lo.z)));
    float leave=min(hi.x,min(hi.y,hi.z));
    if(leave<=enter)discard;
    int steps=wake?40:24;
    float stepSize=(leave-enter)/float(steps),integral=0.0;
    for(int i=0;i<40;++i) {
        if(i>=steps)break;
        vec3 p=gas_ray_origin+ray*(enter+(float(i)+.5)*stepSize);
        float axial=max(0.0,-p.y/max(u_sprite_art.x,.001));
        float downstream=max(0.0,axial-.6);
        float radius=root*mix(1.0,tail,min(axial,1.0))*u_sprite_art.y*(wake?1.0+.65*downstream:1.0);
        // Advected, continuous broadening; no respawn slices or hard end cap.
        float phase=axial*6.0-u_sprite_time*1.8;
        vec2 drift=wake?vec2(sin(phase),sin(phase*.73+1.2))*radius*.10*smoothstep(.5,1.6,axial):vec2(0);
        vec2 radial=(p.xz-drift)/vec2(radius,radius*gas_aspect);
        float r2=dot(radial,radial);
        float density=exp(-axial*max(.001,u_plume_axial_decay)-r2*max(.001,u_plume_radial_decay));
        density*=(1.0-smoothstep(.65,1.0,r2));
        density*=wake?exp(-3.2*downstream*downstream)*(1.0-smoothstep(1.65,2.6,axial)):
            (1.0-smoothstep(.45,1.2,axial));
        float n=sin(p.x*u_plume_noise_scale+axial*7.0-u_sprite_time*1.7)
               *sin(p.z*u_plume_noise_scale-axial*5.0+u_sprite_time*1.1);
        density*=max(0.0,1.0+n*clamp(u_plume_noise_strength,0.0,.55)*axial);
        integral+=density*stepSize;
    }
    // Optical depth per root diameter, independent of integration step count.
    integral*=length(ray*gas_metric)/max(2.0*root*gas_metric.x,.0001);
    float alpha=1.0-exp(log(max(1.0-clamp(u_plume_opacity,0.0,1.0),.000001))*integral);
    if(alpha<.00001)discard;
    float source;
    if(u_stage6_raw_si_domain==1) { source=max(0.0,u_plume_gray); }
    else { source=clamp(u_plume_gray,0.0,1.0); }
    fragColor=vec4(vec3(source),alpha);
}
