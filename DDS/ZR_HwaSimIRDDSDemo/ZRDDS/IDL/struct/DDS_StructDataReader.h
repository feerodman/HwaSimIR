#ifndef DDS_StructDataReader_h__
#define DDS_StructDataReader_h__
/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/

#include "DDS_Struct.h"
#include "ZRDDSDataReader.h"

typedef struct ServToProxySeq ServToProxySeq;

typedef DDS::ZRDDSDataReader<ServToProxy, ServToProxySeq> ServToProxyDataReader;

typedef struct ProxyToServSeq ProxyToServSeq;

typedef DDS::ZRDDSDataReader<ProxyToServ, ProxyToServSeq> ProxyToServDataReader;

#endif

