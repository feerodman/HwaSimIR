/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include <stdlib.h>
#include "ZRDDSTypePlugin.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"
#include "ShapeTypeDataWriter.h"
#include "ZRDDSTypeSupport.cpp"

#ifdef __cplusplus
extern "C"
{
#endif

const DDS_Char* ShapeType_TYPENAME = "ShapeType";
DDSTypeSupportImpl(ShapeTypeTypeSupport, ShapeType, ShapeType_TYPENAME);

DDS_TypeSupport ShapeTypeTypeSupport_instance = {
    ShapeTypeTypeSupport_register_type,
    ShapeTypeTypeSupport_unregister_type,
    ShapeTypeTypeSupport_get_type_name
};

#ifdef __cplusplus
}
#endif
