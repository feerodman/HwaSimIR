#ifndef DECODE_THREAD_H
#define DECODE_THREAD_H

#include "simplethread.h"
#include "listbuf.h"
#include "buffer.h"
#include "mpp_h264_dec.hpp"


#include "log.h"

#include <math.h>
#include <unistd.h>

/* MPP 全局互斥锁定义 (mpp_h264_dec.h 中 extern 声明) */
//std::mutex g_mpp_decode_mutex;


/*
 * DecodeThread 解码线程
 *
 * 从 h264Buf 队列取 H264 码流包, 用 MPP 硬件解码成 NV12 帧,
 * 提取 Y 平面作为灰度图像 (超出 DDS 上限时等比缩放),
 * 放入 grayBuf 队列供发布线程发送。
 *
 * 收到 H264 流结束标记后, 发送 EOS 刷新解码器缓存帧,
 * 发完所有帧后在 grayBuf 中放入流结束标记, 并重置解码器等待下一路。
 */
class DecodeThread : public SimpleThread
{
public:
    ListBuf<H264Buf> *h264Buf_;
    ListBuf<GrayBuf> *grayBuf_;
    MppH264Decoder    decoder_;   /* MPP H.264 解码器 */
    int               gray_seq_;  /* 灰度帧序号 */

     SmyJson::Value &jsonRoot;

    DecodeThread( ListBuf<H264Buf> *h264Buf, ListBuf<GrayBuf> *grayBuf , SmyJson::Value &json_root)
        : h264Buf_( h264Buf )
        , grayBuf_( grayBuf )
        , decoder_( 0 )
        , gray_seq_( 0 )
        , jsonRoot(json_root)
    {
    }
    /* ==================== 灰度缩放 ==================== */

    /*
 * 灰度图等比缩放 (最近邻采样), 保证输出面积 <= maxBytes
 * src:  Y 平面数据, 每行 stride 字节
 * dst:  输出灰度数据, 大小 maxBytes
 * 返回输出字节数, 通过 dw/dh 返回输出宽高
 */
    static int scale_gray_to_fit( const uint8_t *src, int sw, int sh, int sstride,
                                 uint8_t *dst, int maxBytes, int *dw, int *dh )
    {
        double k = sqrt( ( double )sw * sh / maxBytes );
        int    w = ( int )( sw / k );
        int    h = ( int )( sh / k );

        if ( w < 1 ) w = 1;
        if ( h < 1 ) h = 1;

        while ( w * h > maxBytes )
            w--;

        for ( int y = 0; y < h; y++ )
        {
            int          sy      = y * sh / h;
            const uint8_t *srcrow = src + ( size_t )sy * sstride;
            uint8_t     *dstrow  = dst + ( size_t )y * w;
            for ( int x = 0; x < w; x++ )
            {
                int sx       = x * sw / w;
                dstrow[ x ] = srcrow[ sx ];
            }
        }

        *dw = w;
        *dh = h;
        return w * h;
    }

    /* ==================== 灰度帧发送 ==================== */

    /*
 * 将 MPP 解码帧的 Y 平面 (NV12 的灰度分量) 提取为灰度帧放入 grayBuf。
 * 若 width*height 超过 DDS data 上限 (32768), 自动等比缩放。
 */

    //FILE *fp =NULL;
    void graySend( MppFrame frame )
    {
        // if(fp==NULL)
        // {

        //     fp = fopen( "pub_out.gray", "wb" );
        //     if ( !fp )
        //     {
        //         printf( "创建文件失败: %s\n", "pub_out.gray" );
        //     }
        // }

        MppBuffer buffer = mpp_frame_get_buffer( frame );
        if ( !buffer )
        {
            logwarn( "graySend: frame buffer 为空\n" );
            return;
        }

        int           width      = mpp_frame_get_width( frame );
        int           height     = mpp_frame_get_height( frame );
        int           hor_stride = mpp_frame_get_hor_stride( frame );
        const uint8_t *y          = ( const uint8_t * )mpp_buffer_get_ptr( buffer );

        GrayBuf *g = ListBuf<GrayBuf>::alloc();
        if ( !g )  return;



            //if ( ( long long )width * height <= GRAY_MAX_BYTES )
            {
                /* 原尺寸拷贝 Y 平面 (去除 stride 填充) */
                uint8_t *dst = ( uint8_t * )g->data;
                for ( int i = 0; i < height; i++ )
                {
                    memcpy( dst + ( size_t )i * width, y + ( size_t )i * hor_stride, width );

                    //fwrite( y + ( size_t )i * hor_stride, 1, width, fp );


                }
                g->size = width * height;
            }
            // else
            // {
            //     /* 超过 DDS 上限, 等比缩放 */
            //     int dw = 0, dh = 0;
            //     scale_gray_to_fit( y, width, height, hor_stride,
            //                       ( uint8_t * )g->data, GRAY_MAX_BYTES, &dw, &dh );
            //     g->size   = dw * dh;
            //     g->width  = dw;
            //     g->heigth = dh;
            //     logwarn( "灰度图 %dx%d 超 DDS 上限, 缩放为 %dx%d\n", width, height, dw, dh );
            // }

            grayBuf_->put( g );

    }

    /* ==================== EOS 处理 ==================== */

    /*
 * H264 流结束: 发送 EOS 刷新解码器缓存帧, 全部取完后
 * 在 grayBuf 放入流结束标记, 并重置解码器准备接收下一路。
 */
    void handleEos()
    {
        loginfo( "DecodeThread: 收到流结束标记, 刷新解码器缓存帧...\n" );

        MppFrame frame = NULL;
        /* 发送 EOS 空包 */
        decoder_.decode( NULL, 0, true, &frame, NULL );

        /* 排空剩余帧 */
        while ( healthy() )
        {
            bool got = decoder_.decode( NULL, 0, false, &frame, NULL );
            if ( !got )
            {
                if ( decoder_.isEos() )
                    break;
                usleep( 1000 );
                continue;
            }
            graySend( frame );
            decoder_.releaseFrame( frame );
        }

        /* 灰度流结束标记 */
        GrayBuf *g = ListBuf<GrayBuf>::alloc();
        g->size    = 0;
        grayBuf_->put( g );

        loginfo( "DecodeThread: 本路解码完成, 共 %d 帧\n", decoder_.getFrameCount() );

        /* 重置解码器, 准备接收下一路文件 */
        decoder_.deinit();
        decoder_.init();
    }

    /* ==================== 主循环 ==================== */

    int run() override
    {
        //config





        if ( !decoder_.init() )
        {
            logerr( "DecodeThread: MPP 初始化失败\n" );
            return -1;
        }
        loginfo( "DecodeThread: 启动, 等待 H264 数据...\n" );

        while ( healthy() )
        {
            /* 100ms 超时, 便于响应 stop() */
            H264Buf *buf = h264Buf_->get( 100000 );
            if ( !buf )
                continue;


            /* 送数据 + 取帧 */
            MppFrame frame     = NULL;
            bool     input_full = false;
            bool     got        = decoder_.decode( ( const uint8_t * )buf->data, buf->size,
                                       false, &frame, &input_full );
            if ( got && frame )
            {
                graySend( frame );
                decoder_.releaseFrame( frame );
                frame = NULL;
            }

            if ( input_full )
            {
                /* 输入缓冲满: 排空输出后重发同一包, 直到被接收 */
                int retry = 0;
                while ( input_full && healthy() && retry++ < 1000 )
                {
                    usleep( 1000 );
                    got = decoder_.decode( NULL, 0, false, &frame, NULL );
                    if ( got && frame )
                    {
                        graySend( frame );
                        decoder_.releaseFrame( frame );
                        frame = NULL;
                    }
                    got = decoder_.decode( ( const uint8_t * )buf->data, buf->size,
                                          false, &frame, &input_full );
                    if ( got && frame )
                    {
                        graySend( frame );
                        decoder_.releaseFrame( frame );
                        frame = NULL;
                    }
                }
                if ( input_full )
                    logwarn( "DecodeThread: H264 包重发超时, 丢弃 1 包\n" );
            }
            else if ( !got )
            {
                /* 本包未取到帧, 尝试排空一次缓冲 */
                got = decoder_.decode( NULL, 0, false, &frame, NULL );
                if ( got && frame )
                {
                    graySend( frame );
                    decoder_.releaseFrame( frame );
                }
            }

            ListBuf<H264Buf>::free( buf );
        }

        decoder_.deinit();
        loginfo( "DecodeThread: 退出\n" );
        return 0;
    }
};

#endif  // DECODE_THREAD_H
