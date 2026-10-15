#include <fstream>
#include <sstream>
#include <string>
// Export the actual GLES wrapper on the Windows build host.
#undef _WIN32
#include "../HwaSim_IR/HwaSim_IR/IR/IRDepthPrecision.h"
int main(int argc,char** argv){
    if(argc!=4)return 2;
    auto read=[](const char* p){std::ifstream f(p);std::ostringstream s;s<<f.rdbuf();return s.str();};
    auto v=read(argv[1]),f=read(argv[2]);IRDepthPrecision::apply(v,f);
    std::ofstream(std::string(argv[3])+".vert")<<v;
    std::ofstream(std::string(argv[3])+".frag")<<f;
}
