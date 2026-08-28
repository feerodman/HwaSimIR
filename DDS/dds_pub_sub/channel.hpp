#ifndef CHANNEL_THREAD_H
#define CHANNEL_THREAD_H

#include "simplethread.h"
#include "listbuf.h"
#include "buffer.h"

#include "config.h"
#include "listbuf.h"
#include "buffer.h"
#include "smyjson.h"


#include "dds_sub_thread.hpp"
#include "dds_pub_thread.hpp"
#include "decode_thread.hpp"


class ChannelThread : public SimpleThread
{
public:
    int channelNo;
    SmyJson::Value &jsonRoot;
    ChannelThread(int ch_no, SmyJson::Value &json_root):
        jsonRoot(json_root),channelNo(ch_no)
    {
    }

protected:
    int run() override
    {
        /* 数据缓冲队列 */
        ListBuf<H264Buf> h264Buf;   /* H264 码流包队列 (订阅 -> 解码) */
        ListBuf<GrayBuf> grayBuf;   /* 灰度帧队列 (解码 -> 发布) */

        /* 三个线程 */
        DdsSubThread *sub = new DdsSubThread( &h264Buf , jsonRoot["sub"]);
        DecodeThread *dec = new DecodeThread( &h264Buf, &grayBuf, jsonRoot["dec"]);
        DdsPubThread *pub = new DdsPubThread( &grayBuf, jsonRoot["pub"]);


        dec->start();
        pub->start();
        usleep(50*1000);
        sub->start();


        /* 监控循环 */
        while ( 1 )
        {
            loginfo( "channel-%d, buf_count[%2d,%2d], in_count[%5lld,%5lld]\n",
                   channelNo,
                   h264Buf.count(), grayBuf.count(),
                   ( long long )h264Buf.in_count, ( long long )grayBuf.in_count );
            fflush( stdout );
            usleep( 1000 * 1000 );
        }
    }

};

#endif  // CHANNEL_THREAD_H
