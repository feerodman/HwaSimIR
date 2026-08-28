#ifndef MutableTypeExampleDataReader_h__
#define MutableTypeExampleDataReader_h__
/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/

#include "MutableTypeExample.h"
#include "ZRDDSDataReader.h"

typedef struct OrignalTypeSeq OrignalTypeSeq;

typedef DDS::ZRDDSDataReader<OrignalType, OrignalTypeSeq> OrignalTypeDataReader;

typedef struct NewTypeSeq NewTypeSeq;

typedef DDS::ZRDDSDataReader<NewType, NewTypeSeq> NewTypeDataReader;

#endif

