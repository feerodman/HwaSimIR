/*
 * h264_pub.cpp — H264 文件 DDS 发送工具
 *
 * 读取 H264 文件, 按 <=32768 字节分包通过 DDS 发送 (type=MSG_TYPE_H264),
 * 末尾发送流结束标记 (cmd=DDS_CMD_H264_EOS), 供 dds_dec 订阅解码。
 *
 * 用法: ./h264_pub <in.h264>
 *
 * 协议:
 *   data.type = MSG_TYPE_H264
 *   data.cmd  = DDS_CMD_H264_DATA (0) 码流数据
 *   data.cmd  = DDS_CMD_H264_EOS  (1) 流结束 (len=0)
 *   data.sn   = 包序号
 *   data.len  = 有效数据长度
 *   data.data = H264 码流
 */

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "Topic.h"
#include "Publisher.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"

#include "buffer.h"

#include <stdio.h>
#include <string.h>

using namespace DDS;
using namespace std;

int main( int argc, char *argv[] )
{
    if ( argc < 2 )
    {
        printf( "用法: %s <in.h264>\n", argv[ 0 ] );
        return -1;
    }

    FILE *fp = fopen( argv[ 1 ], "rb" );
    if ( !fp )
    {
        printf( "打开文件失败: %s\n", argv[ 1 ] );
        return -1;
    }
    fseek( fp, 0, SEEK_END );
    long file_size = ftell( fp );
    fseek( fp, 0, SEEK_SET );

    /* 域号 */
    const int domain_id = DDS_DOMAIN_ID;
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
        DDS_TOPIC_NAME,
        ShapeTypeTypeSupport::get_instance()->get_type_name(),
        TOPIC_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
    if ( tp == NULL )
    {
        printf( "create tp failed\n" );
        fclose( fp );
        return -1;
    }

    /* 创建发布者 */
    Publisher *pub = dp->create_publisher( PUBLISHER_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
    if ( pub == NULL )
    {
        printf( "create pub failed\n" );
        fclose( fp );
        return -1;
    }

    /* 创建数据写者 */
    DataWriter *_dw = pub->create_datawriter( tp, DATAWRITER_QOS_DEFAULT, NULL, STATUS_MASK_NONE );
    ShapeTypeDataWriter *dw = dynamic_cast<ShapeTypeDataWriter *>( _dw );
    if ( dw == NULL )
    {
        printf( "create dw failed\n" );
        fclose( fp );
        return -1;
    }

    /* 初始化发送数据 */
    ShapeType data;
    ShapeTypeInitialize( &data );
    data.x    = 0;

    static char buf[ H264_PACKET_MAX ];
    int sn = 0;

    /* 等待订阅者匹配 */
    ZRSleep( 3000 );
    printf( "h264_pub: 开始发送 %s (%.2f MB)\n", argv[ 1 ],
            ( double )file_size / ( 1024.0 * 1024.0 ) );

    /* 分包发送 */
    while(1)
    {
        size_t n;

        fseek(fp, 0, SEEK_SET);

        while ( ( n = fread( buf, 1, H264_PACKET_MAX, fp ) ) > 0 )
        {
            data.sn  = sn++;
            data.len = ( long )n;
            data.data.from_array( buf, n );

            rtn = dw->write( data, HANDLE_NIL_NATIVE );
            if ( rtn != RETCODE_OK )
            {
                printf( "write failed: %d\n", rtn );
                break;
            }
            ZRSleep( 1 );   /* 小间隔节流 */
        }
    }

    fclose( fp );

    printf( "h264_pub: 发送完成, 共 %d 包\n", sn );

     ZRSleep( 3000 );

    /* 释放 DDS 资源 */
    ShapeTypeFinalize( &data );
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
