in vec2 test_uv;
out vec4 fragColor;
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
    fragColor=vec4(vec3(g),1.0);
}
