#ifndef CustomerTxVideoDataReader_h__
#define CustomerTxVideoDataReader_h__
/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/

#include "CustomerTxVideo.h"
#include "ZRDDSDataReader.h"

typedef struct HD264StreamSeq HD264StreamSeq;

typedef DDS::ZRDDSDataReader<HD264Stream, HD264StreamSeq> HD264StreamDataReader;

typedef struct LD264StreamSeq LD264StreamSeq;

typedef DDS::ZRDDSDataReader<LD264Stream, LD264StreamSeq> LD264StreamDataReader;

typedef struct HDDecoderSeq HDDecoderSeq;

typedef DDS::ZRDDSDataReader<HDDecoder, HDDecoderSeq> HDDecoderDataReader;

typedef struct LDDecoderSeq LDDecoderSeq;

typedef DDS::ZRDDSDataReader<LDDecoder, LDDecoderSeq> LDDecoderDataReader;

#endif

