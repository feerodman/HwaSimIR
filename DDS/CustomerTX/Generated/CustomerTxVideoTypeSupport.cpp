/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include <stdlib.h>
#include "ZRDDSTypePlugin.h"
#include "CustomerTxVideo.h"
#include "CustomerTxVideoTypeSupport.h"
#include "CustomerTxVideoDataReader.h"
#include "CustomerTxVideoDataWriter.h"
#include "ZRDDSTypeSupport.cpp"


const DDS_Char* HD264Stream_TYPENAME = "HD264Stream";
DDSTypeSupportImpl(HD264StreamTypeSupport, HD264Stream, HD264Stream_TYPENAME);


const DDS_Char* LD264Stream_TYPENAME = "LD264Stream";
DDSTypeSupportImpl(LD264StreamTypeSupport, LD264Stream, LD264Stream_TYPENAME);


const DDS_Char* HDDecoder_TYPENAME = "HDDecoder";
DDSTypeSupportImpl(HDDecoderTypeSupport, HDDecoder, HDDecoder_TYPENAME);


const DDS_Char* LDDecoder_TYPENAME = "LDDecoder";
DDSTypeSupportImpl(LDDecoderTypeSupport, LDDecoder, LDDecoder_TYPENAME);

