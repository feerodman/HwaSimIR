#pragma once
#include <cstddef>
#include <cstdint>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
// Panda unsigned-byte RAM order is BGR(A). Preserve each byte and row;
// only reorder components and drop alpha, as get_ram_image_as("RGB").
inline void IRNativeRgbCopy(const std::uint8_t* input,std::uint8_t* output,std::size_t pixels,int components) {
    std::size_t i=0;
#if defined(__aarch64__)
    for(;i+16<=pixels;i+=16){
        uint8x16x3_t rgb;
        if(components==4){const auto b=vld4q_u8(input+i*4);rgb.val[0]=b.val[2];rgb.val[1]=b.val[1];rgb.val[2]=b.val[0];}
        else {const auto b=vld3q_u8(input+i*3);rgb.val[0]=b.val[2];rgb.val[1]=b.val[1];rgb.val[2]=b.val[0];}
        vst3q_u8(output+i*3,rgb);
    }
#endif
    for(;i<pixels;++i){output[i*3]=input[i*components+2];output[i*3+1]=input[i*components+1];output[i*3+2]=input[i*components];}
}
