/*
 * gray_sub.cpp — 灰度图 DDS 接收存文件工具
 *
 * 订阅 USEDATATYPE 主题 (域 80), 接收 dds_dec 发布的灰度图像包
 * (type=MSG_TYPE_GRAY), 将各帧灰度数据按顺序写入原始灰度文件,
 * 收到流结束标记 (cmd=DDS_CMD_GRAY_EOS) 后退出。
 *
 * 用法: ./gray_sub <out.gray>
 *
 * 协议 (见 buffer.h):
 *   data.type = MSG_TYPE_GRAY
 *   data.cmd  = DDS_CMD_GRAY_IMG (2) 灰度图像, x=宽, sn=高, data 为灰度数据
 *   data.cmd  = DDS_CMD_GRAY_EOS (3) 流结束 (len=0)
 *
 * 查看结果 (宽 640 高 480, 每帧 640*480 字节):
 *   ffplay -f rawvideo -pixel_format gray -video_size 640x480 out.gray
 *   或者用 python/opencv 逐帧读取 np.fromfile
 */

#include "WaitSet.h"
#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "Topic.h"
#include "Subscriber.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"

#include "buffer.h"

#include <stdio.h>
#include <string.h>

using namespace DDS;
using namespace std;

int main( int argc, char *argv[] )
{
    if ( argc < 2 )
    {
        printf( "用法: %s <out.gray>\n", argv[ 0 ] );
        return -1;
    }

    FILE *fp = fopen( argv[ 1 ], "wb" );
    if ( !fp )
    {
        printf( "创建文件失败: %s\n", argv[ 1 ] );
        return -1;
    }

    /* 域号 */
    const int domain_id = 81;
    ReturnCode_t rtn;

    if ( TheParticipantFactory == NULL )
    {
        printf( "get instance failed\n" );
        fclose( fp );
        return -1;
    }

    /* 创建域参与者 */
    DomainParticipant *dp = TheParticipantFactory->create_participant(
        DomainId_t( domain_id ), DOMAINPARTICIPANT_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
    if ( dp == NULL )
    {
        printf( "create dp failed\n" );
        fclose( fp );
        return -1;
    }

    /* 注册数据类型 */
    rtn = ShapeTypeTypeSupport::get_instance()->register_type( dp, NULL );
    if ( rtn != RETCODE_OK )
    {
        printf( "register type failed\n" );
        fclose( fp );
        return -1;
    }

    /* 创建主题 */
    Topic *tp = dp->create_topic(
        "USEDATATYPE_1",
        ShapeTypeTypeSupport::get_instance()->get_type_name(),
        TOPIC_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
    if ( tp == NULL )
    {
        printf( "create tp failed\n" );
        fclose( fp );
        return -1;
    }

    /* 创建订阅者 */
    Subscriber *sub = dp->create_subscriber( SUBSCRIBER_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
    if ( sub == NULL )
    {
        printf( "create sub failed\n" );
        fclose( fp );
        return -1;
    }


    DataReaderQos dr_qos;
    sub->get_default_datareader_qos(dr_qos);

    dr_qos.reliability.kind = RELIABLE_RELIABILITY_QOS;
    dr_qos.history.kind = KEEP_ALL_HISTORY_QOS;
    dr_qos.resource_limits.max_samples = 500;
    dr_qos.resource_limits.max_samples_per_instance = 500;
    dr_qos.resource_limits.max_instances = 1;


    /* 创建数据读者 */
    DataReader *_dr = sub->create_datareader( tp, dr_qos, NULL, STATUS_MASK_NONE );
    //DataReader *_dr = sub->create_datareader( tp, DATAREADER_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
    ShapeTypeDataReader *dr = dynamic_cast<ShapeTypeDataReader *>( _dr );
    if ( dr == NULL )
    {
        printf( "create dr failed\n" );
        fclose( fp );
        return -1;
    }

    ReadCondition *rd_cond = dr->create_readcondition(
        ANY_SAMPLE_STATE, ANY_VIEW_STATE, ANY_INSTANCE_STATE );

    WaitSet *waitset = new WaitSet();
    waitset->attach_condition( rd_cond );

    Duration_t timeout;
    timeout.sec     = 1;    /* 1秒超时 */
    timeout.nanosec = 0;

    ShapeTypeSeq dataSeq;
    SampleInfoSeq infoSeq;

    printf( "gray_sub: 等待灰度数据 ...\n" );

    static char buf[ GRAY_MAX_BYTES ];
    long long  total_frames = 0;    /* 已写入帧数 */
    long long  total_bytes  = 0;    /* 已写入字节数 */
    int        last_w = 960, last_h = 408;
    int        done = 0;

    printf( "gray_sub: 查看命令: ffplay -f rawvideo -pixel_format gray "
           "-video_size %dx%d %s\n", last_w, last_h, argv[ 1 ] );
    while ( !done )
    {
        ConditionSeq cond_seq;
        ReturnCode_t rtn_wait = waitset->wait( cond_seq, timeout );

        if ( rtn_wait == RETCODE_TIMEOUT )
            continue;
        else if ( rtn_wait != RETCODE_OK )
        {
            printf( "wait error\n" );
            continue;
        }

        rtn = dr->take( dataSeq, infoSeq, LENGTH_UNLIMITED,
                        ANY_SAMPLE_STATE, ANY_VIEW_STATE, ANY_INSTANCE_STATE );
        if ( rtn != RETCODE_OK )
        {
            printf( "take data failed\n" );
            continue;
        }

        for ( unsigned int i = 0; i < infoSeq.length(); i++ )
        {
            if ( !infoSeq[ i ].valid_data )
                continue;

            int size = ( int )dataSeq[ i ].len;
            //printf("total=%d, size=%d\n", total_frames, size);

            /* 长度保护 */
            if ( size > GRAY_MAX_BYTES )
                size = GRAY_MAX_BYTES;
            if ( size > ( int )dataSeq[ i ].data.length() )
                size = ( int )dataSeq[ i ].data.length();
            if ( size <= 0 )
                continue;

            dataSeq[ i ].data.to_array( buf, size );

            if ( fwrite( buf, 1, size, fp ) != ( size_t )size )
            {
                printf( "write file failed\n" );
                done = 1;
                break;
            }

            total_frames++;
            total_bytes += size;

           // if ( ( total_frames % 50 ) == 0 )
           //     printf( "gray_sub: total_frames= %lld (%lld Byte)\n", total_frames, total_bytes);
        }

        rtn = dr->return_loan( dataSeq, infoSeq );
        if ( rtn != RETCODE_OK )
            printf( "return loan failed\n" );
    }

    fclose( fp );

    printf( "gray_sub: 完成, 共 %lld 帧, %lld 字节, 尺寸 %dx%d\n",
            total_frames, total_bytes, last_w, last_h );


    /* 释放 DDS 资源 */
    delete waitset;

    rtn = dp->delete_contained_entities();
    if ( rtn != RETCODE_OK )
        printf( "dp delete contained entities failed\n" );
    rtn = TheParticipantFactory->delete_participant( dp );
    if ( rtn != RETCODE_OK )
        printf( "dpf delete dp failed\n" );
    rtn = TheParticipantFactory->finalize_instance();
    if ( rtn != RETCODE_OK )
        printf( "dpf finalize instance failed\n" );

    ShapeTypeTypeSupport::finalize_instance();
    return 0;
}
