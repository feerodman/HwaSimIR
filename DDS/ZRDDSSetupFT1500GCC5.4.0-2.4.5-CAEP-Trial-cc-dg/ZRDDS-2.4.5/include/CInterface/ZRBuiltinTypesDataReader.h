/**
 * @file:       ZRBuiltinTypesDataReader.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef ZRBuiltinTypesDataReader_h__
#define ZRBuiltinTypesDataReader_h__

#include "ZRBuiltinTypes.h"
#include "ZRDDSDataReader.h"

#ifdef _ZRDDS_INCLUDE_BUILTIN_TYPES

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct DDS_BooleanDataReader DDS_BooleanDataReader;
ZRDDSDataReader(DDS_BooleanDataReader, DDS_BooleanSeq, DDS_Boolean);

typedef struct DDS_OctetDataReader DDS_OctetDataReader;
ZRDDSDataReader(DDS_OctetDataReader, DDS_OctetSeq, DDS_Octet);

typedef struct DDS_CharDataReader DDS_CharDataReader;
ZRDDSDataReader(DDS_CharDataReader, DDS_CharSeq, DDS_Char);

typedef struct DDS_ShortDataReader DDS_ShortDataReader;
ZRDDSDataReader(DDS_ShortDataReader, DDS_ShortSeq, DDS_Short);

typedef struct DDS_UShortDataReader DDS_UShortDataReader;
ZRDDSDataReader(DDS_UShortDataReader, DDS_UShortSeq, DDS_UShort);

typedef struct DDS_LongDataReader DDS_LongDataReader;
ZRDDSDataReader(DDS_LongDataReader, DDS_LongSeq, DDS_Long);

typedef struct DDS_ULongDataReader DDS_ULongDataReader;
ZRDDSDataReader(DDS_ULongDataReader, DDS_ULongSeq, DDS_ULong);

typedef struct DDS_LongLongDataReader DDS_LongLongDataReader;
ZRDDSDataReader(DDS_LongLongDataReader, DDS_LongLongSeq, DDS_LongLong);

typedef struct DDS_ULongLongDataReader DDS_ULongLongDataReader;
ZRDDSDataReader(DDS_ULongLongDataReader, DDS_ULongLongSeq, DDS_ULongLong);

typedef struct DDS_FloatDataReader DDS_FloatDataReader;
ZRDDSDataReader(DDS_FloatDataReader, DDS_FloatSeq, DDS_Float);

typedef struct DDS_DoubleDataReader DDS_DoubleDataReader;
ZRDDSDataReader(DDS_DoubleDataReader, DDS_DoubleSeq, DDS_Double);

typedef struct DDS_StringDataReader DDS_StringDataReader;
ZRDDSDataReader(DDS_StringDataReader, DDS_StringSeq, DDS_String);

typedef struct DDS_KeyedStringDataReader DDS_KeyedStringDataReader;
ZRDDSDataReader(DDS_KeyedStringDataReader, DDS_KeyedStringSeq, DDS_KeyedString);

typedef struct DDS_BytesDataReader DDS_BytesDataReader;
ZRDDSDataReader(DDS_BytesDataReader, DDS_BytesSeq, DDS_Bytes);

typedef struct DDS_KeyedBytesDataReader DDS_KeyedBytesDataReader;
ZRDDSDataReader(DDS_KeyedBytesDataReader, DDS_KeyedBytesSeq, DDS_KeyedBytes);

typedef struct DDS_ZeroCopyBytesDataReader DDS_ZeroCopyBytesDataReader;
ZRDDSDataReader(DDS_ZeroCopyBytesDataReader, DDS_ZeroCopyBytesSeq, DDS_ZeroCopyBytes);

#ifdef __cplusplus
}
#endif

#endif /*_ZRDDS_INCLUDE_BUILTIN_TYPES*/

#endif

