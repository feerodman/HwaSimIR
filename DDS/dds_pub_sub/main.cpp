/*
 * dds_dec 主程序
 *
 * 数据链路:
 *   [DdsSubThread]  DDS 订阅 H264 码流包  -> h264Buf 队列
 *   [DecodeThread]  MPP 解码 -> NV12 -> 灰度 -> grayBuf 队列
 *   [DdsPubThread]  取灰度帧 -> DDS 发布 (data 字段携带灰度数据)
 *
 * 框架: SimpleThread 线程类 + ListBuf 缓冲队列 + log 日志 (参考 dmtp08Server)
 *
 * 运行:
 *   ./dds_dec
 *   另开终端用 ./h264_pub xx.h264 发送码流文件
 */

/* config.h 内含 common.h + log.h + STR() 宏, 且必须先于 listbuf.h */
#include "config.h"
#include "listbuf.h"
#include "buffer.h"
#include "smyjson.h"

#include "channel.hpp"
#include <mutex>



#define VerHi  1
#define VerLow 0
SmyJson::Value jsonRoot;

std::mutex g_mpp_decode_mutex;

int main( int argc, char *argv[] )
{
    ( void )argc;
    ( void )argv;

    loginfo( "version : v" STR( VerHi ) "." STR( VerLow ) "\n" );
    loginfo( "build time     : " __DATE__ "-" __TIME__ "\n" );

    SimpleThread::setRt();  /* 设置为实时进程 */

    //解析配置文件，到全局变量jsonRoot， 其他地方直接使用。
    bool ok = SmyJson::passerFile(jsonRoot, ConfigJsonFile);
    if(!ok)
    {
        logerr("err, passer config file failed, name=%s\n", ConfigJsonFile);
        return -1;
    }


    ChannelThread *c0=nullptr;
    ChannelThread *c1=nullptr;

    if(jsonRoot["channels"][0]["enable"].toInt())
    {
        c0 = new ChannelThread(0, jsonRoot["channels"][0]);
        c0->start();

    }

    if(jsonRoot["channels"][1]["enable"].toInt())
    {
        usleep(50*1000);
        usleep(50*1000);

        c1 = new ChannelThread(1, jsonRoot["channels"][1]);
        c1->start();
    }

    while(1)
    {
        if(c0) if(!c0->healthy()) break;
        if(c1) if(!c1->healthy()) break;
        usleep(1000*1000);
    }

    return 0;
}
