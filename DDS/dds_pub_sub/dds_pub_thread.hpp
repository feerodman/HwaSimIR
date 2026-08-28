#ifndef DDS_PUB_THREAD_H
#define DDS_PUB_THREAD_H

#include "simplethread.h"
#include "listbuf.h"
#include "buffer.h"

#include "log.h"
#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "Topic.h"
#include "Publisher.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"

#include <string.h>

using namespace DDS;
using namespace std;

/*
 * DdsPubThread DDS 发布线程
 *
 * 从 grayBuf 队列取灰度帧, 打包成 ShapeType 数据
 * (type=MSG_TYPE_GRAY, x=宽, sn=高, cmd=2 图像 / cmd=3 流结束),
 * 灰度数据放在 data 序列中, 通过 DDS 发布出去。
 */
class DdsPubThread : public SimpleThread
{
public:
    ListBuf<GrayBuf> *grayBuf_;

    SmyJson::Value &jsonRoot;
    DdsPubThread( ListBuf<GrayBuf> *grayBuf, SmyJson::Value &json_root)
        : jsonRoot(json_root), grayBuf_( grayBuf )
    {
    }


    int run() override
    {
        ReturnCode_t rtn;

        //config
        /* 域号 */
        int domain_id = jsonRoot["domain_id"].toInt();
        string topic_name = jsonRoot["topic_name"].toString();
        int key = jsonRoot["key"].toInt();
        int data_size = jsonRoot["data_size"].toInt();

        loginfo( "pub thread start: domain_id=%d,topic_name=%s, key=%d, data_size=%d)\n",
                domain_id,
                topic_name.data(),
                key,
                data_size);

        if(data_size<1) data_size = 32768;


        if ( TheParticipantFactory == NULL )
        {
            logerr( "DdsPubThread: get instance failed\n" );
            return -1;
        }

        /* 创建域参与者 */
        DomainParticipant *dp = TheParticipantFactory->create_participant(
            DomainId_t( domain_id ), DOMAINPARTICIPANT_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
        if ( dp == NULL )
        {
            logerr( "DdsPubThread: create dp failed\n" );
            return -1;
        }

        /* 注册数据类型 */
        rtn = ShapeTypeTypeSupport::get_instance()->register_type( dp, NULL );
        if ( rtn != RETCODE_OK )
        {
            logerr( "DdsPubThread: register type failed\n" );
            return -1;
        }

        /* 创建主题 */
        Topic *tp = dp->create_topic(
            topic_name.data(),
            ShapeTypeTypeSupport::get_instance()->get_type_name(),
            TOPIC_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
        if ( tp == NULL )
        {
            logerr( "DdsPubThread: create tp failed\n" );
            return -1;
        }

        /* 创建发布者 */
        Publisher *pub = dp->create_publisher( PUBLISHER_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
        if ( pub == NULL )
        {
            logerr( "DdsPubThread: create pub failed\n" );
            return -1;
        }

        DataWriterQos dw_qos;
        pub->get_default_datawriter_qos(dw_qos);
        //1. 可靠传输
        dw_qos.reliability.kind = RELIABLE_RELIABILITY_QOS;
        dw_qos.reliability.max_blocking_time.sec = 1;
        dw_qos.reliability.max_blocking_time.nanosec = 0;

        //2. KEEP_ALL：禁止覆盖旧分片
        dw_qos.history.kind = KEEP_ALL_HISTORY_QOS;

        //3. 设置队列最大样本数量
        dw_qos.resource_limits.max_samples = 500;
        dw_qos.resource_limits.max_samples_per_instance = 500;
        dw_qos.resource_limits.max_instances = 1;

        // 创建dw时传入自定义qos，不再使用DATAWRITER_QOS_DEFAULT
        //DataWriter* _dw = pub->create_datawriter(tp, dw_qos, nullptr, STATUS_MASK_NONE);


        /* 创建数据写者 */
        //DataWriter *_dw = pub->create_datawriter( tp, DATAWRITER_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
        DataWriter *_dw = pub->create_datawriter( tp, dw_qos, NULL, STATUS_MASK_NONE );
        ShapeTypeDataWriter *dw = dynamic_cast<ShapeTypeDataWriter *>( _dw );
        if ( dw == NULL )
        {
            logerr( "DdsPubThread: create dw failed\n" );
            return -1;
        }

        /* 初始化发送数据 */
        ShapeType data;
        ShapeTypeInitialize( &data );


        data.x   = key;
        data.sn  = 0;
        data.len = 0;

        /* 等待订阅者匹配 */
        ZRSleep( 3000 );
        loginfo( "DdsPubThread: 开始发布灰度图 ...\n" );

        // FILE *fp = fopen( "pub_out.gray", "wb" );
        // if ( !fp )
        // {
        //     printf( "创建文件失败: %s\n", "pub_out.gray" );
        //     return -1;
        // }


        int total = 0;
        while ( healthy() )
        {
            /* 100ms 超时, 便于响应 stop() */
            GrayBuf *g = grayBuf_->get( -1 );
            if ( !g )  continue;


            for(int offset = 0;offset<g->size;offset+=data_size)
            {
                data.len = g->size-offset;
                if(data.len>data_size) data.len=data_size;
                data.data.from_array( &g->data[offset], data.len );

                // if ( fwrite( &g->data[offset], 1, data.len, fp ) != ( size_t )data.len )
                // {
                //     printf( "write file failed\n" );

                // }

                rtn = dw->write( data, HANDLE_NIL_NATIVE );
                if ( rtn != RETCODE_OK )
                {
                    logerr( "DdsPubThread: write err (%d)\n", rtn );
                }




                data.sn++;
            }
            //printf("total=%d, size=%d, sn=%d\n", total, g->size,data.sn);


            total++;
            ListBuf<GrayBuf>::free( g );
        }

        /* 释放 DDS 资源 */
        ShapeTypeFinalize( &data );
        rtn = dp->delete_contained_entities();
        if ( rtn != RETCODE_OK )
            logwarn( "DdsPubThread: dp delete contained entities failed\n" );
        rtn = TheParticipantFactory->delete_participant( dp );
        if ( rtn != RETCODE_OK )
            logwarn( "DdsPubThread: dpf delete dp failed\n" );
        rtn = TheParticipantFactory->finalize_instance();
        if ( rtn != RETCODE_OK )
            logwarn( "DdsPubThread: dpf finalize instance failed\n" );

        ShapeTypeTypeSupport::finalize_instance();
        loginfo( "DdsPubThread: 退出\n" );
        return 0;
    }

};

#endif  // DDS_PUB_THREAD_H
