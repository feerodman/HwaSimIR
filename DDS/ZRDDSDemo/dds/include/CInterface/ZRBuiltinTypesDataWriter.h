/**
 * @file:       ZRBuiltinTypesDataWriter.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef ZRBuiltinTypesDataWriter_h__
#define ZRBuiltinTypesDataWriter_h__

#include "ZRBuiltinTypes.h"
#include "ZRDDSDataWriter.h"

#ifdef _ZRDDS_INCLUDE_BUILTIN_TYPES

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct DDS_BooleanDataWriter DDS_BooleanDataWriter;
ZRDDSDataWriter(DDS_BooleanDataWriter, DDS_Boolean);

typedef struct DDS_OctetDataWriter DDS_OctetDataWriter;
ZRDDSDataWriter(DDS_OctetDataWriter, DDS_Octet);

typedef struct DDS_CharDataWriter DDS_CharDataWriter;
ZRDDSDataWriter(DDS_CharDataWriter, DDS_Char);

typedef struct DDS_ShortDataWriter DDS_ShortDataWriter;
ZRDDSDataWriter(DDS_ShortDataWriter, DDS_Short);

typedef struct DDS_UShortDataWriter DDS_UShortDataWriter;
ZRDDSDataWriter(DDS_UShortDataWriter, DDS_UShort);

typedef struct DDS_LongDataWriter DDS_LongDataWriter;
ZRDDSDataWriter(DDS_LongDataWriter, DDS_Long);

typedef struct DDS_ULongDataWriter DDS_ULongDataWriter;
ZRDDSDataWriter(DDS_ULongDataWriter, DDS_ULong);

typedef struct DDS_LongLongDataWriter DDS_LongLongDataWriter;
ZRDDSDataWriter(DDS_LongLongDataWriter, DDS_LongLong);

typedef struct DDS_ULongLongDataWriter DDS_ULongLongDataWriter;
ZRDDSDataWriter(DDS_ULongLongDataWriter, DDS_ULongLong);

typedef struct DDS_FloatDataWriter DDS_FloatDataWriter;
ZRDDSDataWriter(DDS_FloatDataWriter, DDS_Float);

typedef struct DDS_DoubleDataWriter DDS_DoubleDataWriter;
ZRDDSDataWriter(DDS_DoubleDataWriter, DDS_Double);

typedef struct DDS_StringDataWriter DDS_StringDataWriter;
ZRDDSDataWriter(DDS_StringDataWriter, DDS_String);

typedef struct DDS_KeyedStringDataWriter DDS_KeyedStringDataWriter;
ZRDDSDataWriter(DDS_KeyedStringDataWriter, DDS_KeyedString);

typedef struct DDS_BytesDataWriter DDS_BytesDataWriter;
ZRDDSDataWriter(DDS_BytesDataWriter, DDS_Bytes);

typedef struct DDS_KeyedBytesDataWriter DDS_KeyedBytesDataWriter;
ZRDDSDataWriter(DDS_KeyedBytesDataWriter, DDS_KeyedBytes);

typedef struct DDS_ZeroCopyBytesDataWriter DDS_ZeroCopyBytesDataWriter;
ZRDDSDataWriter(DDS_ZeroCopyBytesDataWriter, DDS_ZeroCopyBytes);

#ifdef __cplusplus
}
#endif

#endif /*_ZRDDS_INCLUDE_BUILTIN_TYPES*/

#endif

