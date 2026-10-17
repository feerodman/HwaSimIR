// Exercises the actual production FFmpeg encoder, not a replacement CLI encoder.
#include "VideoEncoder.h"
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    VideoEncoderConfig config;config.width=config.height=800;config.fps=60;
    H264FfmpegEncoder encoder;std::string error;
    config.h264Level="3.1";
    if(encoder.configure(config,error)){std::cerr<<"invalid level accepted";return 2;}
    std::cout<<"[NegativeLevel31] rejected="<<error<<std::endl;
    config.h264Level="3.2";config.h264RateControl="cbr";config.h264Aud=true;
    if(!encoder.configure(config,error)){std::cerr<<error;return 3;}
    std::ofstream output(argv[1],std::ios::binary);std::vector<std::uint8_t> pixels(800*800*3);
    for(int index=0;index<120;++index){
        for(int y=0;y<800;++y)for(int x=0;x<800;++x){const auto i=(y*800+x)*3;pixels[i]=(x+index)%256;pixels[i+1]=(y+index)%256;pixels[i+2]=(x+y)%256;}
        RawVideoFrame frame;frame.data=pixels.data();frame.width=frame.height=800;frame.stride=2400;frame.pixelFormat=RawVideoPixelFormat::Rgb24;frame.ptsMs=index*1000/60;
        EncodedVideoFrame encoded;
        if(!encoder.encode(frame,encoded,error)||encoded.payload.empty()){std::cerr<<error;return 4;}
        output.write(reinterpret_cast<const char*>(encoded.payload.data()),encoded.payload.size());
    }
    const char ending[]={0,0,0,1,10};output.write(ending,5);output.close();
    return output?0:5;
}
