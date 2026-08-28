/*
 * mpp_h264_dec.h
 *
 * RK3588 MPP H.264 硬件解码器类封装 (从 mpp_h264_dec.cpp 提取整合)
 *
 * 用法:
 *   MppH264Decoder dec(0);
 *   dec.init();
 *   MppFrame frame = NULL;
 *   bool got = dec.decode(h264_data, size, false, &frame);
 *   if (got) {
 *       外部处理 frame (如取 Y 平面转灰度)
 *       dec.releaseFrame(frame);
 *   }
 *   dec.deinit();
 *
 * 说明:
 *   - decode() 内部持有全局互斥锁 g_mpp_decode_mutex,
 *     多个实例同时调用时只有一个进入 MPP 硬件接口, 防止 VPU 竞争。
 *   - g_mpp_decode_mutex 定义在使用本头文件的 .cpp 中 (如 decode_thread.cpp)。
 *   - decode() 额外提供 input_full 输出参数: 当解码器输入缓冲满
 *     (put_packet 返回 -1012) 时置 true, 调用者应先排空输出再重发同一包。
 */
#ifndef MPP_H264_DEC_H
#define MPP_H264_DEC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <mutex>

#include "rockchip/rk_mpi.h"
#include "rockchip/mpp_buffer.h"
#include "rockchip/mpp_frame.h"
#include "rockchip/mpp_packet.h"

/* 全局互斥锁: 所有 MppH264Decoder 实例共享, 定义在使用本头文件的 .cpp 中 */
extern std::mutex g_mpp_decode_mutex;

class MppH264Decoder
{
public:
    MppH264Decoder( int channel_id = 0 )
        : ctx_( NULL )
        , mpi_( NULL )
        , channel_id_( channel_id )
        , inited_( false )
        , eos_( false )
        , width_( 0 )
        , height_( 0 )
        , frame_count_( 0 )
    {
    }

    ~MppH264Decoder()
    {
        deinit();
    }

    /* ---------- 初始化 ---------- */
    bool init()
    {
        if ( inited_ )
            return true;

        MPP_RET ret = mpp_create( &ctx_, &mpi_ );
        if ( ret != MPP_OK )
        {
            fprintf( stderr, "[CH%d] mpp_create 失败: %d\n", channel_id_, ret );
            return false;
        }

        ret = mpp_init( ctx_, MPP_CTX_DEC, MPP_VIDEO_CodingAVC );
        if ( ret != MPP_OK )
        {
            fprintf( stderr, "[CH%d] mpp_init 失败: %d\n", channel_id_, ret );
            mpp_destroy( ctx_ );
            ctx_ = NULL;
            return false;
        }

        /* split 模式: 解码器内部自动处理 NAL 边界分割, 可直接送任意长度分片 */
        RK_U32 need_split = 1;
        mpi_->control( ctx_, MPP_DEC_SET_PARSER_SPLIT_MODE, &need_split );

        /* 输出格式 NV12 */
        MppFrameFormat output_fmt = MPP_FMT_YUV420SP;
        mpi_->control( ctx_, MPP_DEC_SET_OUTPUT_FORMAT, &output_fmt );

        /* fast play: 禁用 MPP 内部帧率控制, 全速输出解码帧 */
        RK_U32 enable_fast = 1;
        mpi_->control( ctx_, MPP_DEC_SET_ENABLE_FAST_PLAY, &enable_fast );

        inited_ = true;
        printf( "[CH%d] MPP 解码器初始化成功 (H.264, NV12)\n", channel_id_ );
        return true;
    }

    /* ---------- 清理 ---------- */
    void deinit()
    {
        if ( !inited_ )
            return;

        if ( mpi_ )
        {
            mpi_->reset( ctx_ );
            mpi_ = NULL;
        }
        if ( ctx_ )
        {
            mpp_destroy( ctx_ );
            ctx_ = NULL;
        }
        inited_ = false;
        eos_    = false;
    }

    /* ---------- 一体化解码接口 ----------
     *
     * 合并 sendPacket + getFrame, 一次调用完成 "送数据 + 取帧"。
     *
     * 参数:
     *   data:       H.264 码流数据指针
     *               - 非 NULL: 送入解码器
     *               - NULL 且 is_eos=false: 不送数据, 只取帧 (排空缓冲)
     *               - NULL 且 is_eos=true:  送 EOS 空包, 刷新缓存
     *   size:       数据大小 (data 为 NULL 时忽略)
     *   is_eos:     是否标记 EOS (最后一包或显式刷新)
     *   out_frame:  输出参数, 接收解码帧指针, 返回 true 时有效, 用完须 releaseFrame()
     *   input_full: 可选输出, 置 true 表示本次 put_packet 失败 (输入缓冲满),
     *               调用者应先排空输出再重发同一包
     *
     * 返回:
     *   true:  取到一帧正常视频帧, *out_frame 有效
     *   false: 无帧输出 (info-change / error / EOS / 无数据可取)
     */
    bool decode( const uint8_t *data, size_t size, bool is_eos, MppFrame *out_frame,
                 bool *input_full = NULL )
    {
        /* 全局锁: 多实例调用时只有一个进入 MPP 硬件接口 */
        std::lock_guard<std::mutex> lock( g_mpp_decode_mutex );

        *out_frame = NULL;
        if ( input_full )
            *input_full = false;

        if ( !inited_ )
            return false;

        /* --- 1. 送数据 --- */
        if ( data && size > 0 )
        {
            MppPacket packet = NULL;
            MPP_RET    ret   = mpp_packet_init( &packet, ( void * )data, size );
            if ( ret != MPP_OK )
            {
                fprintf( stderr, "[CH%d] mpp_packet_init 失败: %d\n", channel_id_, ret );
                return false;
            }
            if ( is_eos )
                mpp_packet_set_eos( packet );

            ret = mpi_->decode_put_packet( ctx_, packet );
            mpp_packet_deinit( &packet );

            if ( ret != MPP_OK )
            {
                /* -1012 = MPP_ERR_BUFFER_FULL: 输入缓冲区满
                 * 标记给调用者, 让其排空输出后重发同一包 */
                if ( input_full )
                    *input_full = true;
            }
        }
        else if ( is_eos )
        {
            /* 发送 EOS 空包, 刷新解码器缓存的最后几帧 */
            MppPacket packet = NULL;
            mpp_packet_init( &packet, NULL, 0 );
            mpp_packet_set_eos( packet );
            mpi_->decode_put_packet( ctx_, packet );
            mpp_packet_deinit( &packet );
        }

        /* --- 2. 取帧 (无论 put 是否成功都尝试) --- */
        MppFrame frame = NULL;
        MPP_RET  ret   = mpi_->decode_get_frame( ctx_, &frame );
        if ( ret != MPP_OK || !frame )
            return false;

        /* EOS 帧: 解码器已全部输出完毕 */
        if ( mpp_frame_get_eos( frame ) )
        {
            eos_ = true;
            mpp_frame_deinit( &frame );
            return false;
        }

        /* info change 帧: 分辨率变更通知, 无像素数据 */
        if ( mpp_frame_get_info_change( frame ) )
        {
            width_  = mpp_frame_get_width( frame );
            height_ = mpp_frame_get_height( frame );
            printf( "[CH%d] 分辨率变更: %dx%d, 确认并继续\n",
                    channel_id_, width_, height_ );
            mpi_->control( ctx_, MPP_DEC_SET_INFO_CHANGE_READY, NULL );
            mpp_frame_deinit( &frame );
            return false;
        }

        /* 错误帧 / 丢弃帧 */
        if ( mpp_frame_get_errinfo( frame ) || mpp_frame_get_discard( frame ) )
        {
            mpp_frame_deinit( &frame );
            return false;
        }

        /* 正常视频帧 */
        if ( width_ == 0 )
        {
            width_  = mpp_frame_get_width( frame );
            height_ = mpp_frame_get_height( frame );
            printf( "[CH%d] 首帧: %dx%d\n", channel_id_, width_, height_ );
        }
        frame_count_++;
        *out_frame = frame;
        return true;
    }

    /* 释放帧 (decode 返回 true 时, 用完后必须调用) */
    void releaseFrame( MppFrame frame )
    {
        if ( frame )
            mpp_frame_deinit( &frame );
    }

    /* 是否收到 EOS */
    bool isEos() const { return eos_; }

    /* ---------- 信息查询 ---------- */
    int getWidth()      const { return width_; }
    int getHeight()     const { return height_; }
    int getFrameCount() const { return frame_count_; }
    int getChannel()    const { return channel_id_; }

private:
    /* MPP 资源 */
    MppCtx  ctx_;
    MppApi *mpi_;
    int     channel_id_;
    bool    inited_;
    bool    eos_;

    /* 帧信息 */
    int width_;
    int height_;
    int frame_count_;
};

#endif  // MPP_H264_DEC_H
