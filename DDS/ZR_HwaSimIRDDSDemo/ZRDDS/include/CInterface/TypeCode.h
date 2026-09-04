/**
 * @file:       TypeCode.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef TypeCode_h__
#define TypeCode_h__

#include "OsResource.h"
#include "TypeCodeKind.h"
#include "ZRDDSCommon.h"
#include "ZRDDSCWrapper.h"

#ifdef _ZRDDS_INCLUDE_TYPECODE

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @fn  TCTypeKind TypeCodeGetKind(const TypeCode* self);
 *
 * @brief   获取TypeCode表示的类型
 *
 * @author  Hzy
 * @date    2016/7/5
 *
 * @param   self    指向目标TypeCode。
 *
 * @return  TC的类型。
 */

DCPSDLL extern TCTypeKind TypeCodeGetKind(const TypeCode* self);

/**
 * @fn  ZR_BOOLEAN TypeCodeCompare(const TypeCode* self TypeCode1, const TypeCode* right TypeCode2);
 *
 * @brief   比较两个TypeCode是否指向表示同一个结构.
 *
 * @author  Hzy
 * @date    2016/7/5
 *
 * @param   TypeCode1   第一个TypeCode。
 * @param   TypeCode2   第二个TypeCode。
 *
 * @return  true表示相等，false表示不相等。
 */

DCPSDLL extern ZR_BOOLEAN TypeCodeCompare(const TypeCode* typeCode1, const TypeCode* typeCode2);

/**
 * @fn  const ZR_INT8* TypeCodeGetName(const TypeCode* self);
 *
 * @brief   获取TypeCode的名称，只支持union/struct/enum类型。
 *
 * @author  Hzy
 * @date    2016/7/5
 *
 * @param   self    指向目标TypeCode
 *
 * @return  NULL表示失败，否则表示名称。
 */

DCPSDLL extern const ZR_INT8* TypeCodeGetName(const TypeCode* self);

/**
 * @fn  ZR_INT32 TypeCodeGetMemberCount(const TypeCode* self);
 *
 * @brief   获取TypeCode表示的类型成员个数，只支持union/struct/enum类型。
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self    指向目标TypeCode
 *
 * @return  小于0表示失败，否则为有效值
 */

DCPSDLL extern ZR_INT32 TypeCodeGetMemberCount(const TypeCode* self);

/**
 * @fn  const ZR_INT8* TypeCodeGetMemberName(const TypeCode* self, ZR_UINT32 index);
 *
 * @brief   获取指定下标成员的名称.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self    指向目标。
 * @param   index   目标成员下标。
 *
 * @return  NULL表示失败，否则为有效的成员名称指针。
 */

DCPSDLL extern const ZR_INT8* TypeCodeGetMemberName(const TypeCode* self, ZR_UINT32 index);

/**
 * @fn  DCPSDLL const TypeCodeHeader* TypeCodeGetBaseType(const TypeCodeHeader* self);
 *
 * @brief   获取TypeCode父结构，仅适用于VALUE_TYPE.
 *
 * @author  Hzy
 * @date    2016/12/13
 *
 * @param   self    指向VALUE_TYPE.
 *
 * @return  NULL表示获取失败，否则执行父结构的类。
 */

DCPSDLL extern const TypeCodeHeader* TypeCodeGetBaseType(const TypeCodeHeader* self);

/**
 * @fn  DCPSDLL extern TypeCode* TypeCodeGetElementType(const TypeCode* self);
 *
 * @brief   Type code get element type.
 *
 * @author  Rainnus
 * @date    2016/9/27
 *
 * @param   self    The self.
 *
 * @return  null if it fails, else a TypeCodeHeader*.
 */

DCPSDLL extern TypeCode* TypeCodeGetElementType(const TypeCode* self);

/**
 * @fn  ZR_INT32 TypeCodeGetIndexByName(const TypeCode* self, const ZR_INT8* name);
 *
 * @brief   通过成员名称获取下标从0开始.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self    指向目标。
 * @param   name    成员名称。
 *
 * @return  小于零表示失败，否则为有效的下标。
 */

DCPSDLL extern ZR_INT32 TypeCodeGetIndexByName(const TypeCode* self, const ZR_INT8* name);

/**
 * @fn  TypeCode*TypeCodeGetMemberType(const TypeCode* self, ZR_UINT32 index);
 *
 * @brief   获取指定下标成员的TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self    指向目标。
 * @param   index   表示下标。
 */

DCPSDLL extern TypeCode*TypeCodeGetMemberType(const TypeCode* self, ZR_UINT32 index);

/**
 * @fn  ZR_INT32 TypeCodeGetLabelCount(const TypeCode* self, ZR_UINT32 index);
 *
 * @brief   获取Union类型指定的成员的label数量，无效返回 - 1.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self    指向目标。
 * @param   index   指明下标。
 *
 * @return  A ZR_INT32.
 */

DCPSDLL extern ZR_INT32 TypeCodeGetLabelCount(const TypeCode* self, ZR_UINT32 index);

/**
 * @fn  ZR_INT32 TypeCodeGetLabel(const TypeCode* self, ZR_UINT32 memberIdx, ZR_UINT32 labelIdx, TypeCodeExceptionCode* expCode);
 *
 * @brief   获取Union成员的label.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self            指向目标。
 * @param   memberIdx       成员下标。
 * @param   labelIdx        标签下标。
 * @param [in,out]  expCode 返回是否有效，可能因为参数无效需要返回无效值。
 *
 * @return  如果expCode有效，则返回值为label，否则该返回值无效。
 */

DCPSDLL extern ZR_INT32 TypeCodeGetLabel(const TypeCode* self,
    ZR_UINT32 memberIdx,
    ZR_UINT32 labelIdx,
    TypeCodeExceptionCode* expCode);

/**
 * @fn  ZR_INT32 TypeCodeGetDefaultIndex(const TypeCode* self);
 *
 * @brief   获取Union类型的默认下标.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self    指向目标。
 *
 * @return  -1表示失败，否则为有效的下标。
 */

DCPSDLL extern ZR_INT32 TypeCodeGetDefaultIndex(const TypeCode* self);

/**
 * @fn  ZR_INT32 TypeCodeGetEnumVal(const TypeCode* self, ZR_UINT32 memberIdx, TypeCodeExceptionCode* expCode);
 *
 * @brief   获取枚举值的值。
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self            指向目标。
 * @param   memberIdx       成员下标。
 * @param [in,out]  expCode 返回是否有效，可能因为参数无效需要返回无效值。
 *
 * @return  如果expCode有效，则返回值为枚举的值，否则该返回值无效。
 */

DCPSDLL extern ZR_INT32 TypeCodeGetEnumVal(const TypeCode* self,
    ZR_UINT32 memberIdx,
    TypeCodeExceptionCode* expCode);

/**
 * @fn  DCPSDLL const ZR_INT8* TypeCodeGetEnumString(const TypeCode* self, ZR_UINT32 memberIdx, TypeCodeExceptionCode* expCode);
 *
 * @brief   获取枚举成员对应的字符串。
 *
 * @author  Hzy
 * @date    2018/3/16
 *
 * @param   self            指明TypeCode.
 * @param   enumVal         枚举值。
 * @param [in,out]  expCode 如果expCode有效，则返回值为枚举的值，否则该返回值无效。
 *
 * @return  NULL表示失败，否则表示枚举字符串值。
 */

DCPSDLL extern const ZR_INT8* TypeCodeGetEnumString(const TypeCode* self,
    ZR_UINT32 enumVal,
    TypeCodeExceptionCode* expCode);

/**
 * @fn  ZR_BOOLEAN TypeCodeIsMemberKey(const TypeCode* self, ZR_UINT32 index, TypeCodeExceptionCode* expCode);
 *
 * @brief   获取指定结构体成员是否是key域.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self            指向目标。
 * @param   index           指向成员下标。
 * @param [in,out]  expCode 返回是否有效，可能因为参数无效需要返回无效值。
 *
 * @return  如果expCode有效，则返回值为时候是key域，否则该返回值无效。
 */

DCPSDLL extern ZR_BOOLEAN TypeCodeIsMemberKey(const TypeCode* self,
    ZR_UINT32 index,
    TypeCodeExceptionCode* expCode);

/**
 * @fn  ZR_INT32 TypeCodeGetArrayDimensionCount(const TypeCode* self);
 *
 * @brief   获取数组类型的维度。
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self    指向目标。
 *
 * @return  小于0表示无效,否则表示维度。
 */

DCPSDLL extern ZR_INT32 TypeCodeGetArrayDimensionCount(const TypeCode* self);

/**
 * @fn  ZR_INT32 TypeCodeGetArrayDimension(const TypeCode* self, ZR_INT32 index);
 *
 * @brief   获取指定下标维度的维数.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self    指向目标。
 * @param   index   指向维度。
 *
 * @return  小于零表示失败，否则为有效的维数。
 */

DCPSDLL extern ZR_INT32 TypeCodeGetArrayDimension(const TypeCode* self, ZR_UINT32 index);

/**
 * @fn  DCPSDLL ZR_INT32 TypeCodeGetArrayElementCount(const TypeCodeHeader* self);
 *
 * @brief   获取类型的最大长度，支持string/sequence。
 *
 * @author  Hzy 
 * @date    2018/2/6
 *
 * @param   self    指向对象。
 *
 * @return  最大长度。
 */

DCPSDLL extern ZR_INT32 TypeCodeGetMaxLength(const TypeCodeHeader* self);

/**
 * @fn  DCPSDLL extern ZR_INT32 TypeCodeGetArrayElementCount(const TypeCode* self);
 *
 * @brief   获取数组元素个数。
 *
 * @author  Rainnus
 * @date    2016/10/25
 *
 * @param   self    指向对象。
 *
 * @return  数组元素个数。
 */

DCPSDLL extern ZR_INT32 TypeCodeGetArrayElementCount(const TypeCode* self);

/**
 * @fn  ZR_INT32 TypeCodeAddMemberToEnum(TypeCode* self, ZR_UINT32 index, const ZR_INT8* name, ZR_UINT32 value);
 *
 * @brief   向枚举类型添加成员
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self    指明目标。
 * @param   index           指明添加的位置，大于当前成员个数则添加在尾部。
 * @param   name            成员名称。
 * @param   value           添加的成员值。
 *
 * @return  大于0表示添加后枚举成员个数，小于0表示失败.
 */

DCPSDLL extern ZR_INT32 TypeCodeAddMemberToEnum(TypeCode* self,
    ZR_UINT32 index,
    const ZR_INT8* name,
    ZR_UINT32 value);

/**
 * @fn  ZR_INT32 TypeCodeAddMemberToUnion(TypeCode* self, ZR_UINT32 index, const ZR_INT8* name, ZR_UINT32 labelCount, ZR_UINT32* labels, const TypeCode* tc);
 *
 * @brief   向联合类型添加成员
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self    指向目标。
 * @param   index           指明添加的位置，大于当前成员个数则添加在尾部。
 * @param   name            成员的名称。
 * @param   labelCount      该成员标签数量。
 * @param [in,out]  labels  该成员的标签数组。
 * @param   tc              该成员的类型。
 *
 * @return  返回添加后联合成员个数，小于0表示失败。
 */

DCPSDLL extern ZR_INT32 TypeCodeAddMemberToUnion(TypeCode* self, ZR_UINT32 index, const ZR_INT8* name, ZR_UINT32 labelCount, ZR_UINT32* labels, const TypeCode* tc);

/**
 * @fn  DCPSDLL ZR_INT32 TypeCodeAddMemberToStruct(TypeCodeHeader* self, ZR_UINT32 index, ZR_UINT32 memberId, const ZR_INT8* name, const TypeCodeHeader* tc, ZR_BOOLEAN isKey, ZR_BOOLEAN isOption);
 *
 * @brief   向结构体类型添加成员.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self    指向目标。
 * @param   index           指明添加的位置，大于当前成员个数则添加在尾部。
 * @param   memberId        指定该成员的Id, -1表示与下标一致。
 * @param   name            成员的名称。
 * @param   tc              成员类型。
 * @param   isKey           该成员是否是key。
 * @param   isOption        该成员是否是可选的。
 *
 * @return  返回添加后结构体成员个数，小于0表示失败。
 */

DCPSDLL extern ZR_INT32 TypeCodeAddMemberToStruct(
    TypeCode* self, 
    ZR_UINT32 index, 
    ZR_UINT32 memberId, 
    const ZR_INT8* name, 
    const TypeCode* tc, 
    ZR_BOOLEAN isKey, 
    ZR_BOOLEAN isOption);

/**
 * @fn  DCPSDLL ZR_INT32 TypeCodeAddMemberToValueType(TypeCodeHeader* self, ZR_UINT32 index, ZR_UINT32 memberId, const ZR_INT8* name, const TypeCodeHeader* tc, ZR_BOOLEAN isKey, ZR_BOOLEAN isOption);
 *
 * @brief   向ValueType类型添加成员.
 *
 * @author  Rainnus
 * @date    2016/10/21
 *
 * @param [in,out]  self    指向目标。
 * @param   index           指明添加的位置，大于当前成员个数则添加在尾部。
 * @param   memberId        指定该成员的Id, -1表示与下标一致。
 * @param   name            成员的名称。
 * @param   tc              成员类型。
 * @param   isKey           该成员是否是key。
 * @param   isOption        该成员是否是可选的。
 *
 * @return  返回添加后ValueType成员个数，小于0表示失败。
 */

DCPSDLL extern ZR_INT32 TypeCodeAddMemberToValueType(
    TypeCode* self,
    ZR_UINT32 index,
    ZR_UINT32 memberId,
    const ZR_INT8* name,
    const TypeCode* tc,
    ZR_BOOLEAN isKey,
    ZR_BOOLEAN isOption);
/**
 * @fn  ZR_INT8* TypeCodeGetTypePrintableString(const TypeCodeHeader* self);
 *
 * @brief   获取等价的IDL文件内容的缓冲区，此方法若返回成功，则必须调用TypeCodeReleasePrintableString方法释放空间。
 *
 * @author  Hzy
 * @date    2016/12/13
 *
 * @param   self    指明的TypeCode。
 *
 * @return  NULL获取失败，否则指向内容。
 */

DCPSDLL extern ZR_INT8* TypeCodeGetTypePrintableString(const TypeCodeHeader* self);

/**
 * @fn  DCPSDLL void TypeCodeReleasePrintableString(ZR_INT8* buffer);
 *
 * @brief   释放由TypeCodeGetTypePrintableString底层创建的空间。
 *
 * @author  Hzy
 * @date    2016/12/13
 *
 * @param [in,out]  buffer  指向由TypeCodeGetTypePrintableString返回的空间。
 */

DCPSDLL extern void TypeCodeReleasePrintableString(ZR_INT8* buffer);
/**
 * @fn  ZR_INT32 TypeCodePrintIDL(const TypeCodeHeader* self);
 *
 * @brief   打印指定的TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param   self    指明目标。
 *
 * @return  小于零表示打印出错，0表示打印成功。
 */

DCPSDLL extern ZR_INT32 TypeCodePrintIDL(const TypeCode* self);

/**
 * @fn  TypeCodeFactory* TypeCodeFactoryGetInstance();
 *
 * @brief   获取TypeCodeFactory的单例.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @return  NULL表示失败，否则成功。
 */

DCPSDLL extern TypeCodeFactory* TypeCodeFactoryGetInstance();

/**
 * @fn  ZR_BOOLEAN TypeCodeFactoryFinalize();
 *
 * @brief   析构全局TypeCode单例。
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @return  true表示成功，false表示失败。
 */

DCPSDLL extern ZR_BOOLEAN TypeCodeFactoryFinalize();

/**
 * @fn  TypeCode* TypeCodeFactoryGetPrimitiveTC(TypeCodeFactory* self, TCTypeKind kind);
 *
 * @brief   获取基本数据类型的TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self    指明目标。
 * @param   kind            指明基本数据类型。
 *
 * @return  NULL表示失败，否则为有效的TypeCode。
 */

DCPSDLL extern TypeCode* TypeCodeFactoryGetPrimitiveTC(TypeCodeFactory* self, TCTypeKind kind);

/**
 * @fn  DCPSDLL TypeCodeHeader* TypeCodeFactoryCreateStructTC(TypeCodeFactoryImpl* self, const ZR_INT8* name, ExtensibilityKind kind);
 *
 * @brief   创建结构体TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self    指明目标。
 * @param   name            结构体的名称。
 * @param   kind            该结构体的extensibility属性。
 *
 * @return  NULL表示失败，否则为有效的TypeCode。
 */

DCPSDLL extern TypeCode* TypeCodeFactoryCreateStructTC(
    TypeCodeFactory* self, 
    const ZR_INT8* name, 
    ExtensibilityKind kind);

/**
 * @fn  DCPSDLL TypeCodeHeader* TypeCodeFactoryCreateValueTypeTC(TypeCodeFactoryImpl* self, const ZR_INT8* name, TypeCodeModifierKind modifierKind, ExtensibilityKind kind, const TypeCodeHeader *baseTC);
 *
 * @brief   Type code factory create value type tc.
 *
 * @author  Rainnus
 * @date    2016/10/21
 *
 * @param [in,out]  self    指明目标。
 * @param   name            valueType名称。
 * @param   modifierKind    modifier的类型。
 * @param   kind            extensibility类型。
 * @param   baseTC          继承的类型。
 *
 * @return  NULL表示失败，否则为有效的TypeCode。
 */

DCPSDLL extern TypeCode* TypeCodeFactoryCreateValueTypeTC(
    TypeCodeFactory* self, 
    const ZR_INT8* name, 
    TypeCodeModifierKind modifierKind, 
    ExtensibilityKind kind, 
    const TypeCode*baseTC);

/**
 * @fn  DCPSDLL TypeCodeHeader* TypeCodeFactoryCreateEnumTC( TypeCodeFactoryImpl* self, const ZR_INT8* name, ZR_UINT32 bitBound, ExtensibilityKind kind);
 *
 * @brief   创建枚举类型TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self    指明目标。
 * @param   name            枚举类型的名称。
 * @param   bitBound        位宽大小。
 * @param   kind            Extenbility属性值。
 *
 * @return  NULL表示失败，否则为有效的TypeCode。
 */

DCPSDLL extern TypeCode* TypeCodeFactoryCreateEnumTC(
    TypeCodeFactory* self, 
    const ZR_INT8* name,
    ZR_UINT32 bitBound,
    ExtensibilityKind kind);

/**
 * @fn  TypeCode* TypeCodeFactoryCreateUnionTC(TypeCodeFactory* self, const ZR_INT8* name, TypeCode* switchTC, ZR_UINT32 defaultIdx);
 *
 * @brief   创建联合类型TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self        指明目标。
 * @param   name                联合类型的名称。
 * @param [in,out]  switchTC    辨别符的TC。
 * @param   defaultIdx          默认的成员下标。
 *
 * @return  NULL表示失败，否则为有效的TypeCode。
 */

DCPSDLL extern TypeCode* TypeCodeFactoryCreateUnionTC(TypeCodeFactory* self, const ZR_INT8* name, const TypeCode* switchTC, ZR_UINT32 defaultIdx);

/**
 * @fn  TypeCode* TypeCodeFactoryCreateStringTC(TypeCodeFactory* self, const ZR_UINT32 length);
 *
 * @brief   创建String类型TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self    指明目标。
 * @param   length          string的长度。
 *
 * @return  NULL表示失败，否则为有效的TypeCode。
 */

DCPSDLL extern TypeCode* TypeCodeFactoryCreateStringTC(TypeCodeFactory* self, const ZR_UINT32 length);

/**
 * @fn  TypeCode* TypeCodeFactoryCreateSequenceTC(TypeCodeFactory* self, const ZR_UINT32 maxLength, const TypeCode* tc);
 *
 * @brief   创建Sequence类型TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self    指明目标。
 * @param   maxLength          Sequence的最大长度。
 * @param   tc              元素的TypeCode。
 *
 * @return  NULL表示失败，否则为有效的TypeCode。
 */

DCPSDLL extern TypeCode* TypeCodeFactoryCreateSequenceTC(TypeCodeFactory* self, const ZR_UINT32 maxLength, const TypeCode* tc);

/**
 * @fn  TypeCode* TypeCodeFactoryCreateArrayTC(TypeCodeFactory* self, const ZR_UINT32 dimensionCount, ZR_UINT* dimensions, const TypeCode* tc);
 *
 * @brief   创建数组类型TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self        指明目标。
 * @param   dimensionCount      数组维度。
 * @param [in,out]  dimensions  各个维度的维数。
 * @param   tc                  元素的TypeCode。
 *
 * @return  NULL表示失败，否则为有效的TypeCode。
 */

DCPSDLL extern TypeCode* TypeCodeFactoryCreateArrayTC(TypeCodeFactory* self, const ZR_UINT32 dimensionCount, const ZR_UINT32* dimensions, const TypeCode* tc);

/**
 * @fn  TypeCode* TypeCodeFactoryCloneTC (TypeCodeFactory* self, const TypeCode* tc);
 *
 * @brief   拷贝一个TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self    指明目标。
 * @param   tc              源TypeCode。
 *
 * @return  NULL表示失败，否则为有效的TypeCode。
 */

DCPSDLL extern TypeCode* TypeCodeFactoryCloneTC(TypeCodeFactory* self, const TypeCode* tc);

/**
 * @fn  ZR_BOOLEAN TypeCodeFactoryDeleteTC(TypeCodeFactory* self, TypeCode* tc);
 *
 * @brief   删除一个TypeCode.
 *
 * @author  Hzy
 * @date    2016/7/6
 *
 * @param [in,out]  self    指明目标。
 * @param [in,out]  tc      删除的目标。
 *
 * @return  true表示删除成功，false表示删除失败。
 */

DCPSDLL extern ZR_BOOLEAN TypeCodeFactoryDeleteTC(TypeCodeFactory* self, TypeCode* tc);

#ifdef __cplusplus
}
#endif

#endif /* _ZRDDS_INCLUDE_TYPECODE */

#endif /* TypeCode_h__*/
