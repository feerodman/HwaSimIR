#include "Nv12Neon.h"
#include <cassert>
#include <iostream>
#include <random>
#include <vector>
#include <chrono>
static int code(const unsigned char* p,int mode){
    const int r=p[0],g=p[1],b=p[2];
    if(mode==0)return ((66*r+129*g+25*b+128)>>8)+16;
    if(mode==1)return ((-38*r-74*g+112*b+128)>>8)+128;
    return ((112*r-94*g-18*b+128)>>8)+128;
}
int main(){
    const unsigned char red[]={255,0,0},white[]={255,255,255};
    assert(code(red,0)==82&&code(red,1)==90&&code(red,2)==240);
    assert(code(white,0)==235&&code(white,1)==128&&code(white,2)==128);
    std::mt19937 rng(7310914);std::uint64_t pixels=0;
    for(int width:{8,16,800})for(bool flip:{false,true})for(bool rgb:{false,true}){
        const int height=800,inputStride=width*3+19,stride=width+32;
        std::vector<unsigned char> input(inputStride*height);
        for(auto& byte:input)byte=static_cast<unsigned char>(rng());
        std::vector<unsigned char> actual(stride*height*3/2,128),expected=actual;
        RawVideoFrame frame;frame.data=input.data();frame.width=width;frame.height=height;
        frame.stride=inputStride;frame.flipVertical=flip;frame.pixelFormat=rgb?RawVideoPixelFormat::Rgb24:RawVideoPixelFormat::Bgr24;
        auto pixel=[&](int x,int y,int component){
            auto* p=input.data()+(flip?height-1-y:y)*inputStride+x*3;
            unsigned char ordered[]={p[rgb?0:2],p[1],p[rgb?2:0]};return code(ordered,component);
        };
        for(int y=0;y<height;++y)for(int x=0;x<width;++x)expected[y*stride+x]=pixel(x,y,0);
        for(int y=0;y<height;y+=2)for(int x=0;x<width;x+=2)for(int c=1;c<=2;++c){
            int total=0;for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)total+=pixel(x+dx,y+dy,c);
            expected[stride*height+(y/2)*stride+x+c-1]=(total+2)/4;
        }
        assert(HwaNv12::convert(frame,actual.data(),actual.data()+stride*height,stride));
        assert(actual==expected);pixels+=std::uint64_t(width)*height;
        if(width==800){
            const auto begin=std::chrono::steady_clock::now();
            for(int i=0;i<100;++i)HwaNv12::convert(frame,actual.data(),actual.data()+stride*height,stride);
            std::cout<<"800x800 conversion meanMs="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/100<<" flip="<<flip<<" rgb="<<rgb<<"\n";
        }
    }
    std::cout<<"PASS exact scalar equivalence pixels="<<pixels<<" padded input/output, RGB/BGR, both orientations\n";
}
