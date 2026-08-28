#ifndef DDS_SUB_THREAD_H
#define DDS_SUB_THREAD_H

#include "simplethread.h"
#include "listbuf.h"
#include "buffer.h"

#include "log.h"

#include "WaitSet.h"
#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "Topic.h"
#include "Subscriber.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"

#include <string.h>

using namespace DDS;
using namespace std;

/*
 * DdsSubThread DDS 订阅线程
 *
 * 订阅 USEDATATYPE 主题上的 H264 码流包 (type=MSG_TYPE_H264),
 * 逐包放入 h264Buf 队列供解码线程处理。
 *
 * 注意: 只接收 type=MSG_TYPE_H264 的包, 忽略本进程发布出去的
 * 灰度包 (type=MSG_TYPE_GRAY), 防止自环接收。
 */
class DdsSubThread : public SimpleThread
{
public:
    ListBuf<H264Buf> *h264Buf_;
    SmyJson::Value &jsonRoot;

    DdsSubThread( ListBuf<H264Buf> *h264Buf, SmyJson::Value &json_root)
        : jsonRoot(json_root),h264Buf_( h264Buf )
    {
    }

    int run() override
    {


        ReturnCode_t rtn;

        //config
        /* 域号 */
        int domain_id = jsonRoot["domain_id"].toInt();
        string topic_name = jsonRoot["topic_name"].toString();


        loginfo( "sub thread start: domain_id=%d, )\n",
                domain_id,
                topic_name );



        if ( TheParticipantFactory == NULL )
        {
            //while(1)usleep(1000);
            logerr( "DdsSubThread: get instance failed\n" );
            return -1;
        }

        /* 创建域参与者 */
        DomainParticipant *dp = TheParticipantFactory->create_participant(
            DomainId_t( domain_id ), DOMAINPARTICIPANT_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
        if ( dp == NULL )
        {
            logerr( "DdsSubThread: create dp failed\n" );
            return -1;
        }

        /* 注册数据类型 */
        rtn = ShapeTypeTypeSupport::get_instance()->register_type( dp, NULL );
        if ( rtn != RETCODE_OK )
        {
            logerr( "DdsSubThread: register type failed\n" );
            return -1;
        }

        /* 创建主题 */
        Topic *tp = dp->create_topic(
            topic_name.data(),
            ShapeTypeTypeSupport::get_instance()->get_type_name(),
            TOPIC_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
        if ( tp == NULL )
        {
            logerr( "DdsSubThread: create tp failed\n" );
            return -1;
        }

        /* 创建订阅者 */
        Subscriber *sub = dp->create_subscriber( SUBSCRIBER_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
        if ( sub == NULL )
        {
            logerr( "DdsSubThread: create sub failed\n" );
            return -1;
        }
/*
            DataReaderQos dr_qos;
            sub->get_default_datareader_qos(dr_qos);

            dr_qos.reliability.kind = RELIABLE_RELIABILITY_QOS;
            dr_qos.history.kind = KEEP_ALL_HISTORY_QOS;
            dr_qos.resource_limits.max_samples = 500;
            dr_qos.resource_limits.max_samples_per_instance = 500;
            dr_qos.resource_limits.max_instances = 1;

            DataReader* _dr = sub->create_datareader(tp, dr_qos, nullptr, STATUS_MASK_NONE);

            */
        /* 创建数据读者 */
        //DataReader *_dr = sub->create_datareader( tp, dr_qos, NULL, STATUS_MASK_NONE );
        DataReader *_dr = sub->create_datareader( tp, DATAREADER_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
        ShapeTypeDataReader *dr = dynamic_cast<ShapeTypeDataReader *>( _dr );
        if ( dr == NULL )
        {
            logerr( "DdsSubThread: create dr failed\n" );
            return -1;
        }

        ReadCondition *rd_cond = dr->create_readcondition(
            ANY_SAMPLE_STATE, ANY_VIEW_STATE, ANY_INSTANCE_STATE );

        WaitSet *waitset = new WaitSet();
        waitset->attach_condition( rd_cond );

        Duration_t timeout;
        timeout.sec     = 1;    /* 1秒超时, 便于响应 stop() */
        timeout.nanosec = 0;

        ShapeTypeSeq dataSeq;
        SampleInfoSeq infoSeq;

        loginfo( "DdsSubThread: 等待 H264 数据 ...\n" );

        int total_pack = 0;

        while ( healthy() )
        {
            ConditionSeq cond_seq;
            ReturnCode_t rtn_wait = waitset->wait( cond_seq, timeout );

            if ( rtn_wait == RETCODE_TIMEOUT )
                continue;
            else if ( rtn_wait != RETCODE_OK )
            {
                logwarn( "DdsSubThread: wait error\n" );
                continue;
            }

            /* 取出接收数据 */
            rtn = dr->take( dataSeq, infoSeq, LENGTH_UNLIMITED,
                           ANY_SAMPLE_STATE, ANY_VIEW_STATE, ANY_INSTANCE_STATE );
            if ( rtn != RETCODE_OK )
            {
                logwarn( "DdsSubThread: take data failed\n" );
                continue;
            }

            for ( unsigned int i = 0; i < infoSeq.length(); i++ )
            {
                if ( !infoSeq[ i ].valid_data )
                    continue;

                //读取二进制载荷
                 const ShapeType& recv_msg = dataSeq[i];
                // printf("recv: x=%d, type=%d, sn=%d, cmd=%d, len=%d, crc=%d\n",
                //       recv_msg.x, recv_msg.type, recv_msg.sn, recv_msg.cmd, recv_msg.len, recv_msg.crc);

                // uint8_t* payload_buf = (uint8_t*)recv_msg.data.get_buffer();
                // int payload_len = recv_msg.data.length();
                // // 此处：payload_buf 就是视频分片字节流，可做帧重组解码

                H264Buf *b = ListBuf<H264Buf>::alloc();
                b->size    = recv_msg.len;

                //长度保护
                if ( b->size > H264_PACKET_MAX )  b->size = H264_PACKET_MAX;
                if ( b->size > ( int )recv_msg.data.length() )  b->size = ( int )recv_msg.data.length();

                if ( b->size > 0 )  recv_msg.data.to_array( b->data, b->size );
               // printf("total_pack=%d, recv_msg=%d size=%d\n", total_pack, recv_msg.sn, b->size);
                h264Buf_->put( b );
                total_pack++;

                //if ( ( total_pack % 1000 ) == 0 )
                //    loginfo( "DdsSubThread: 已接收 %d 个 H264 包\n", total_pack );

            }

            /* 返还数据空间 */
            rtn = dr->return_loan( dataSeq, infoSeq );
            if ( rtn != RETCODE_OK )
                logwarn( "DdsSubThread: return loan failed\n" );
        }

        /* 释放 DDS 资源 */
        rtn = dp->delete_contained_entities();
        if ( rtn != RETCODE_OK )
            logwarn( "DdsSubThread: dp delete contained entities failed\n" );
        rtn = TheParticipantFactory->delete_participant( dp );
        if ( rtn != RETCODE_OK )
            logwarn( "DdsSubThread: dpf delete dp failed\n" );
        rtn = TheParticipantFactory->finalize_instance();
        if ( rtn != RETCODE_OK )
            logwarn( "DdsSubThread: dpf finalize instance failed\n" );

        ShapeTypeTypeSupport::finalize_instance();
        loginfo( "DdsSubThread: 退出\n" );
        return 0;
    }


};

#endif  // DDS_SUB_THREAD_H
