/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include <stdlib.h>
#include "ZRDDSTypePlugin.h"
#include "DDS_Struct.h"
#include "DDS_StructTypeSupport.h"
#include "DDS_StructDataReader.h"
#include "DDS_StructDataWriter.h"
#include "ZRDDSTypeSupport.cpp"


const DDS_Char* ServToProxy_TYPENAME = "ServToProxy";
DDSTypeSupportImpl(ServToProxyTypeSupport, ServToProxy, ServToProxy_TYPENAME);


const DDS_Char* ProxyToServ_TYPENAME = "ProxyToServ";
DDSTypeSupportImpl(ProxyToServTypeSupport, ProxyToServ, ProxyToServ_TYPENAME);

