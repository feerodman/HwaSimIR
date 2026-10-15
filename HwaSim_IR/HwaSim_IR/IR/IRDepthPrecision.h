#pragma once
#include <cstdlib>
#include <string>

// A shared logarithmic depth contract for all depth-tested scene shaders.
// Keep protocol near/far clipping and color/coverage unchanged. At kilometre
// ranges, conventional 1..200000 m D24 perspective depth cannot distinguish
// the front and back surfaces of even an ordinary aircraft or missile.
namespace IRDepthPrecision {
inline void replaceAll(std::string& s,const std::string& from,const std::string& to) {
    size_t p=0;while((p=s.find(from,p))!=std::string::npos){s.replace(p,from.size(),to);p+=to.size();}
}
inline void apply(std::string& vertex,std::string& fragment) {
    // An explicit whole-scene A/B switch; never mix depth encodings.
    const char* linear=std::getenv("P17LinearDepthReference");
    if(linear && std::string(linear)=="1")return;
    if(vertex.find("#version 100")!=std::string::npos) {
#ifdef _WIN32
        const std::string version="#version 130";
#else
        const std::string version="#version 300 es\nprecision highp float;\nprecision highp int;";
#endif
        replaceAll(vertex,"#version 100",version);
        replaceAll(fragment,"#version 100",version);
        replaceAll(vertex,"attribute ","in ");replaceAll(vertex,"varying ","out ");
        replaceAll(fragment,"varying ","in ");replaceAll(fragment,"texture2D(","texture(");
        replaceAll(fragment,"gl_FragColor","irSceneColor");
        const size_t precision=fragment.find("precision mediump float;");
        if(precision!=std::string::npos)fragment.insert(precision+24,"\nout vec4 irSceneColor;\n");
    }
    // Panda's GLES version scanner requires the directive at byte zero;
    // raw C++ literals otherwise leave a newline before #version 300 es.
    vertex.erase(0,vertex.find("#version"));
    fragment.erase(0,fragment.find("#version"));
    vertex.insert(vertex.find('\n')+1,"uniform highp vec2 u_ir_log_depth;\n");
    if(fragment.find("uniform vec2 u_ir_log_depth;")==std::string::npos &&
       fragment.find("uniform highp vec2 u_ir_log_depth;")==std::string::npos)
        fragment.insert(fragment.find('\n')+1,"uniform highp vec2 u_ir_log_depth;\n");
    replaceAll(vertex,"void main()","void irOriginalMain()");
    replaceAll(fragment,"void main()","void irOriginalMain()");
    vertex += R"(
void main() {
    irOriginalMain();
    highp float w=gl_Position.w;
    if(w>0.0) {
        highp float depth=(log2(1.0+w)-u_ir_log_depth.x)*u_ir_log_depth.y;
        gl_Position.z=(2.0*depth-1.0)*w;
    }
}
)";
    fragment += R"(
void main() {
    irOriginalMain();
    highp float w=1.0/gl_FragCoord.w;
    gl_FragDepth=(log2(1.0+w)-u_ir_log_depth.x)*u_ir_log_depth.y;
}
)";
}
}
