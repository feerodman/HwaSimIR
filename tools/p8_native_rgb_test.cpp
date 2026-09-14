#include "IRNativeRgbCopy.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>
int main(int argc,char** argv){
    if(argc!=2)return 2;std::ifstream f(argv[1],std::ios::binary);int cases=0;
    std::uint32_t count=0;f.read(reinterpret_cast<char*>(&count),4);
    for(std::uint32_t c=0;c<count;++c){
        std::uint32_t components=0,pixels=0;f.read(reinterpret_cast<char*>(&components),4);f.read(reinterpret_cast<char*>(&pixels),4);
        if((components!=3&&components!=4)||pixels>1000000)return 3;
        std::vector<unsigned char> input(pixels*components+2,0x7d),reference(pixels*3),output(pixels*3+2,0xa5);
        f.read(reinterpret_cast<char*>(input.data()+1),pixels*components);f.read(reinterpret_cast<char*>(reference.data()),pixels*3);
        if(!f)return 4;IRNativeRgbCopy(input.data()+1,output.data()+1,pixels,components);
        if(!std::equal(reference.begin(),reference.end(),output.begin()+1)||output.front()!=0xa5||output.back()!=0xa5)return 5;
        ++cases;
    }
    std::cout<<"NativeRgbCopy PandaGolden cases="<<cases<<" result=PASS exactBytes=1 unaligned=1 tail=1 guardBytes=1\n";return cases==count?0:6;
}
