#ifndef CustomerTxVideoDataWriter_h__
#define CustomerTxVideoDataWriter_h__
/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/

#include "CustomerTxVideo.h"
#include "ZRDDSDataWriter.h"


typedef DDS::ZRDDSDataWriter<HD264Stream> HD264StreamDataWriter;


typedef DDS::ZRDDSDataWriter<LD264Stream> LD264StreamDataWriter;


typedef DDS::ZRDDSDataWriter<HDDecoder> HDDecoderDataWriter;


typedef DDS::ZRDDSDataWriter<LDDecoder> LDDecoderDataWriter;

#endif

