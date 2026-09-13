in vec2 test_uv;
out vec4 fragColor;
uniform int u_display_case;
uniform int u_frame_idx;
void main(){
    vec2 p=test_uv;float g=.18;
    if(p.y>.80)g=p.x;
    else if(p.y>.60)g=.08*p.x;
    else if(p.y>.40)g=.20+.005*floor(p.x*16.0);
    else if(p.y>.20){g=.18;if(p.x>.85&&p.x<.90&&p.y>.26&&p.y<.31)g=1.5;}
    else {
        // Analytic ordinary sphere-shading samples on a neutral plane.
        vec2 q=(p-vec2(.25,.10))/vec2(.08,.08);float r=dot(q,q);
        if(r<1.0){vec3 n=vec3(q,sqrt(1.0-r));g=.03+.27*max(0.0,dot(n,normalize(vec3(-.4,.6,1.0))));}
        q=(p-vec2(.60,.10))/vec2(.08,.08);r=dot(q,q);
        if(r<1.0){vec3 n=vec3(q,sqrt(1.0-r));g=.06+.5*max(0.0,dot(n,normalize(vec3(-.4,.6,1.0))));}
    }
    // Explicit ordinary diagnostic cases; values are dimensionless common linear inputs.
    if(u_display_case==1)g=4.0*p.x;
    if(u_display_case==2)g=.25;
    if(u_display_case==3)g=.25+.0001*p.x;
    bool brightPhase=(u_frame_idx>=60&&u_frame_idx<140);
    if(u_display_case==4||u_display_case==5){
        g=.02+.18*p.x;
        if(brightPhase&&p.y>.2&&p.y<.8&&p.x>(u_display_case==4?.985:.55))g=2.0+2.0*p.y;
    }
    if(u_display_case==6){
        g=.1;
        // Fine stripe/offset patch can miss a fixed 64x64 sampling lattice.
        if(mod(floor(p.x*800.0),25.0)<2.0)g=2.0;
        if(p.x>.501&&p.x<.504&&p.y>.501&&p.y<.504)g=8.0;
    }
    if(u_display_case==7)g=3.0+.05*p.x;
    if(u_display_case==8)g=.4+.05*p.x;
    fragColor=vec4(vec3(g),1.0);
}
