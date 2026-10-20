#include <fstream>
#include <sstream>
#include <string>
#include "../HwaSim_IR/HwaSim_IR/IR/IRCloudTransparentTransport.h"
#include "../HwaSim_IR/HwaSim_IR/IR/IRDepthPrecision.h"
static std::string read(const std::string& p){std::ifstream f(p);std::ostringstream s;s<<f.rdbuf();return s.str();}
static void write(const std::string& p,const std::string& s){std::ofstream f(p);f<<s;}
int main(int argc,char** argv){
 const std::string out=argc>1?std::string(argv[1])+"/":"logs/p20/transparent_gas_fixture/",version="#version 130\n";
 std::string v=version+read("HwaSim_IR/Bin/Config/GameVFX/sprite.vert"),f=version+read("HwaSim_IR/Bin/Config/GameVFX/sprite.frag");
 std::string ov=v,of=f;IRDepthPrecision::apply(ov,of);write(out+"original.vert",ov);write(out+"original.frag",of);
 IRApplyPlumeCloudPrefix(v,f);IRDepthPrecision::apply(v,f);write(out+"prefix.vert",v);write(out+"prefix.frag",f);
 v=version+"uniform mat4 p3d_ModelViewProjectionMatrix;in vec4 p3d_Vertex;out vec2 particle_uv;out float particle_alpha;void main(){particle_uv=vec2(.5);particle_alpha=.5;gl_Position=p3d_ModelViewProjectionMatrix*vec4(p3d_Vertex.x,0,p3d_Vertex.y,1);}";
 f=version+read("HwaSim_IR/Bin/Config/Weather/precipitation.frag");IRApplyRainCloudPrefix(f);IRDepthPrecision::apply(v,f);write(out+"rain.vert",v);write(out+"rain.frag",f);
 return 0;
}
