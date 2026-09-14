#pragma once
#include "VideoEncoder.h"
#if defined(__aarch64__)
#include <arm_neon.h>

namespace HwaNv12 {
struct Eight { uint8x8_t y,u,v; };
inline Eight rgbEight(const std::uint8_t* source,bool rgb){
    const auto packed=vld3_u8(source);
    const auto r=packed.val[rgb?0:2],g=packed.val[1],b=packed.val[rgb?2:0];
    auto y=vmull_u8(r,vdup_n_u8(66));
    y=vmlal_u8(y,g,vdup_n_u8(129));y=vmlal_u8(y,b,vdup_n_u8(25));
    y=vaddq_u16(y,vdupq_n_u16(128));
    const auto rs=vreinterpretq_s16_u16(vmovl_u8(r));
    const auto gs=vreinterpretq_s16_u16(vmovl_u8(g));
    const auto bs=vreinterpretq_s16_u16(vmovl_u8(b));
    auto u=vmulq_n_s16(rs,-38);u=vmlaq_n_s16(u,gs,-74);u=vmlaq_n_s16(u,bs,112);
    auto v=vmulq_n_s16(rs,112);v=vmlaq_n_s16(v,gs,-94);v=vmlaq_n_s16(v,bs,-18);
    u=vaddq_s16(vshrq_n_s16(vaddq_s16(u,vdupq_n_s16(128)),8),vdupq_n_s16(128));
    v=vaddq_s16(vshrq_n_s16(vaddq_s16(v,vdupq_n_s16(128)),8),vdupq_n_s16(128));
    return {vadd_u8(vshrn_n_u16(y,8),vdup_n_u8(16)),vqmovun_s16(u),vqmovun_s16(v)};
}
// Same limited-range BT.601 integer operations and per-pixel chroma rounding as
// the existing scalar converter. Input orientation, stride, and RGB/BGR order
// are preserved. Other geometry/formats continue through the scalar fallback.
inline bool convert(const RawVideoFrame& raw,std::uint8_t* yPlane,std::uint8_t* uvPlane,int stride){
    if(raw.pixelFormat==RawVideoPixelFormat::Gray8||(raw.width%8)!=0)return false;
    const bool rgb=raw.pixelFormat==RawVideoPixelFormat::Rgb24;
    for(int row=0;row<raw.height;row+=2){
        const auto* a=raw.data+std::size_t(raw.flipVertical?raw.height-1-row:row)*raw.stride;
        const auto* b=raw.data+std::size_t(raw.flipVertical?raw.height-2-row:row+1)*raw.stride;
        auto* ya=yPlane+std::size_t(row)*stride;auto* yb=ya+stride;
        auto* uv=uvPlane+std::size_t(row/2)*stride;
        for(int x=0;x<raw.width;x+=8){
            const auto p=rgbEight(a+x*3,rgb),q=rgbEight(b+x*3,rgb);
            vst1_u8(ya+x,p.y);vst1_u8(yb+x,q.y);
            auto u=vadd_u16(vpaddl_u8(p.u),vpaddl_u8(q.u));
            auto v=vadd_u16(vpaddl_u8(p.v),vpaddl_u8(q.v));
            u=vshr_n_u16(vadd_u16(u,vdup_n_u16(2)),2);
            v=vshr_n_u16(vadd_u16(v,vdup_n_u16(2)),2);
            const auto joined=vmovn_u16(vcombine_u16(u,v));
            vst1_u8(uv+x,vzip_u8(joined,vext_u8(joined,joined,4)).val[0]);
        }
    }
    return true;
}
}
#endif
