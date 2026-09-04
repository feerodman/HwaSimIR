/**
 * @file:       ZRBuiltinTypesTypeSupport.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef ZRBuiltinTypesTypeSupport_h__
#define ZRBuiltinTypesTypeSupport_h__

#include "ZRDDSTypeSupport.h"

#ifdef _ZRDDS_INCLUDE_BUILTIN_TYPES

#ifdef __cplusplus
extern "C"
{
#endif
    
DDSInnerTypeSupport(DDS_BooleanTypeSupport, DDS_Boolean);

DDSInnerTypeSupport(DDS_OctetTypeSupport, DDS_Octet);

DDSInnerTypeSupport(DDS_CharTypeSupport, DDS_Char);

DDSInnerTypeSupport(DDS_ShortTypeSupport, DDS_Short);

DDSInnerTypeSupport(DDS_UShortTypeSupport, DDS_UShort);

DDSInnerTypeSupport(DDS_LongTypeSupport, DDS_Long);

DDSInnerTypeSupport(DDS_ULongTypeSupport, DDS_ULong);

DDSInnerTypeSupport(DDS_LongLongTypeSupport, DDS_LongLong);

DDSInnerTypeSupport(DDS_ULongLongTypeSupport, DDS_ULongLong);

DDSInnerTypeSupport(DDS_FloatTypeSupport, DDS_Float);

DDSInnerTypeSupport(DDS_DoubleTypeSupport, DDS_Double);

DDSInnerTypeSupport(DDS_StringTypeSupport, DDS_String);

DDSInnerTypeSupport(DDS_KeyedStringTypeSupport, DDS_KeyedString);

DDSInnerTypeSupport(DDS_BytesTypeSupport, DDS_Bytes);

DDSInnerTypeSupport(DDS_KeyedBytesTypeSupport, DDS_KeyedBytes);

DDSInnerTypeSupport(DDS_ZeroCopyBytesTypeSupport, DDS_ZeroCopyBytes);

#ifdef __cplusplus
}
#endif

#endif /*_ZRDDS_INCLUDE_BUILTIN_TYPES*/

#endif

