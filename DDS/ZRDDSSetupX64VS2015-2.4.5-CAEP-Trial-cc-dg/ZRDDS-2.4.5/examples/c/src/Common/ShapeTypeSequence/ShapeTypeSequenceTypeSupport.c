/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include <stdlib.h>
#include "ZRDDSTypePlugin.h"
#include "ShapeTypeSequence.h"
#include "ShapeTypeSequenceTypeSupport.h"
#include "ShapeTypeSequenceDataReader.h"
#include "ShapeTypeSequenceDataWriter.h"
#include "ZRDDSTypeSupport.cpp"

#ifdef __cplusplus
extern "C"
{
#endif

const DDS_Char* ShapeTypeSequence_TYPENAME = "ShapeTypeSequence";
DDSTypeSupportImpl(ShapeTypeSequenceTypeSupport, ShapeTypeSequence, ShapeTypeSequence_TYPENAME);

DDS_TypeSupport ShapeTypeSequenceTypeSupport_instance = {
    ShapeTypeSequenceTypeSupport_register_type,
    ShapeTypeSequenceTypeSupport_unregister_type,
    ShapeTypeSequenceTypeSupport_get_type_name
};

#ifdef __cplusplus
}
#endif
