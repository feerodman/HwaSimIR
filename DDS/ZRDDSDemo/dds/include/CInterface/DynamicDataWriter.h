/**
 * @file:       DynamicDataWriter.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef DynamicDataWriter_h__
#define DynamicDataWriter_h__

#include "ZRDynamicData.h"
#include "ZRDDSDataWriter.h"

#ifdef _ZRDDS_INCLUDE_DYNAMIC_DATA
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ZRDynamicDataDataWriter ZRDynamicDataDataWriter;
ZRDDSDataWriter(ZRDynamicDataDataWriter, ZRDynamicData);

#ifdef __cplusplus
}
#endif
#endif /* _ZRDDS_INCLUDE_DYNAMIC_DATA */

#endif /* DynamicDataWriter_h__*/
