#pragma once
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <iterator>
#include <iostream>

// Exact source identity selects an offline path-only derivative. No startup scan
// or source edits; a stale derivative can never substitute an updated user OBJ.
inline std::string IRPortableModelPath(const std::string& source) {
    auto read=[](const std::string& path){std::ifstream f(path.c_str(),std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());};
    auto hash=[](const std::string& s){uint64_t h=UINT64_C(14695981039346656037);for(unsigned char c:s){h^=c;h*=UINT64_C(1099511628211);}std::ostringstream o;o<<std::hex<<std::setfill('0')<<std::setw(16)<<h;return o.str();};
    const std::string bytes=read(source);
    std::istringstream lines(bytes);std::string line,mtl;
    while(std::getline(lines,line)){if(line.compare(0,7,"mtllib ")==0){mtl=line.substr(7);if(!mtl.empty()&&mtl.back()=='\r')mtl.pop_back();break;}}
    if(mtl.empty())return source;
    const std::string folder=source.substr(0,source.find_last_of("/\\")+1);
    const std::string derivative=source+".portable."+hash(bytes)+"-"+hash(read(folder+mtl))+".obj";
    if(std::ifstream(derivative.c_str()).good()){
        std::cout<<"[PortableModel] source="<<source<<" sourceFNV="<<hash(bytes)<<" load="<<derivative<<" sourceModified=0 geometryTransform=none\n";
        return derivative;
    }
    return source;
}
