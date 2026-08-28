#ifndef PROTROL_h
#define PROTROL_h

#include "config.h"



#pragma pack( push, 1 )


//输入流类型
#define data_io_type_local 0
#define data_io_type_network 1
#define data_io_type_camera 2
#define data_io_type_OpticalFibre 3

///////////////////////////////////////////////////////////////////////////////////////////////
////报文定义

typedef struct
{
    char data[ BufSizeMax];
    int  size;//原始图像占用空间
    int width;  //原始图像宽度
    int heigth; //原始图像高度
    int channel; //原始图像通道

    float *m_input_infer_device;//在obj模型中申请，在kp模型中释放。并且只在obj和kp模型中使用。
    float bbox_roi[4];//obj传给kp的识别框，基于src_h和src_w;


    int res_bbox_roi[4];//obj传给kp的识别框，基于原始图像
    int res_keypoints[32];
    int res_keypoints_count;


    //test time
    long long kp_t0;
    long long kp_t1;
    long long kp_t2;
    long long kp_t3;


    long long decode_t1;
    long long decode_t2;


} VideoBuffer;

typedef struct
{



    int width;  //原始图像宽度
    int heigth; //原始图像高度
    int channel; //原始图像通道

    float bbox_roi[4];//obj传给kp的识别框，基于src_h和src_w;
    int res_bbox_roi[4];//obj传给kp的识别框，基于原始图像
    int res_keypoints_count;
    int res_keypoints[32];
}ResultPack;



#pragma pack( pop )

#endif  // PROTROL_h
