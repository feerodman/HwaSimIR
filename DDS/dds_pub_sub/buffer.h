#ifndef DDS_DEC_BUFFER_H
#define DDS_DEC_BUFFER_H

/* ==================== 消息协议 ====================
 * 通过 ShapeType 的 type/cmd/sn/len/data 字段区分消息:
 *   type = MSG_TYPE_H264 : H264 码流包
 *           cmd = DDS_CMD_H264_DATA (0) 码流数据包, data 为 H264 码流
 *           cmd = DDS_CMD_H264_EOS  (1) 流结束标记, len=0
 *   type = MSG_TYPE_GRAY : 灰度图像包
 *           cmd = DDS_CMD_GRAY_IMG (2) 灰度图像, x=宽, sn=高, data 为灰度数据
 *           cmd = DDS_CMD_GRAY_EOS (3) 灰度流结束标记, len=0
 * -------------------------------------------------- */

#define MSG_TYPE_H264   0   /* H264 码流包 */
#define MSG_TYPE_GRAY   1   /* 灰度图像包 */

#define DDS_CMD_H264_DATA   0
#define DDS_CMD_H264_EOS    1
#define DDS_CMD_GRAY_IMG    2
#define DDS_CMD_GRAY_EOS    3

/* ==================== DDS 参数 ==================== */
#define DDS_DOMAIN_ID   80
#define DDS_TOPIC_NAME  "USEDATATYPE"

/* ==================== 缓冲大小 ==================== */
#define H264_PACKET_MAX 1024   /* H264 单包最大长度 (与 DDS data 序列上限一致) */
#define GRAY_MAX_BYTES  (32768)   /* 灰度图最大字节数 (DDS data 序列上限) */

/* ==================== 缓冲结构 ==================== */

/* H264 码流包 (订阅线程 -> 解码线程) */
typedef struct
{
    char data[ H264_PACKET_MAX ];
    int  size;          /* 有效数据长度 */
} H264Buf;

/* 灰度图像帧 (解码线程 -> 发布线程) */
typedef struct
{
    char data[ GRAY_MAX_BYTES *100 ];
    int  size;
} GrayBuf;

#endif  // DDS_DEC_BUFFER_H
