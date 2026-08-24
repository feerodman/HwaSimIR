/**
 * @file:       DynamicDataTypeSupport.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef DynamicDataTypeSupport_h__
#define DynamicDataTypeSupport_h__

#include "ZRDDSTypePlugin.h"
#include "ZRDynamicData.h"
#include "ZRDDSCWrapper.h"

#ifdef _ZRDDS_INCLUDE_DYNAMIC_DATA
#ifdef __cplusplus
extern "C"
{
#endif

extern DCPSDLL DDS_Boolean ZRDynamicDataTypeSupportInitialize(
    ZRDynamicDataTypeSupport *self, 
    TypeCode *type, 
    const ZRDynamicDataProperty_t *props);

extern DCPSDLL ZRDynamicDataTypeSupport* ZRDynamicDataTypeSupportNew(
    TypeCode *type, 
    const ZRDynamicDataProperty_t *props);

extern DCPSDLL void ZRDynamicDataTypeSupportFinalize(
    ZRDynamicDataTypeSupport *self);

extern DCPSDLL void ZRDynamicDataTypeSupportDelete(
    ZRDynamicDataTypeSupport *self);

extern DCPSDLL DDS_ReturnCode_t ZRDynamicDataTypeSupportRegisterType(
    ZRDynamicDataTypeSupport *self, 
    DDS_DomainParticipant *participant, 
    const char *typeName,
    TypePluginCreateDataWriterFunction createDwFunc,
    TypePluginDestroyDataWriterFunction destroyDwFunc,
    TypePluginCreateDataReaderFunction createDrFunc,
    TypePluginDestroyDataReaderFunction destroyDrFunc);

extern DCPSDLL DDS_ReturnCode_t ZRDynamicDataTypeSupportUnregisterType(
    ZRDynamicDataTypeSupport *self, 
    DDS_DomainParticipant *participant, 
    const char *typeName);

extern DCPSDLL const char* ZRDynamicDataTypeSupportGetTypeName(
    const ZRDynamicDataTypeSupport *self);

extern DCPSDLL const TypeCode* ZRDynamicDataTypeSupportGetDataType(
    const ZRDynamicDataTypeSupport *self);

extern DCPSDLL ZRDynamicData* ZRDynamicDataTypeSupportCreateData(
    const ZRDynamicDataTypeSupport *self);

extern DCPSDLL DDS_ReturnCode_t ZRDynamicDataTypeSupportDeleteData(
    ZRDynamicDataTypeSupport *self, 
    ZRDynamicData *data);

extern DCPSDLL void ZRDynamicDataTypeSupportPrintData(
    const ZRDynamicDataTypeSupport *self, 
    const ZRDynamicData *data);

extern DCPSDLL DDS_ReturnCode_t ZRDynamicDataTypeSupportCopyData(
    const ZRDynamicDataTypeSupport *self, 
    ZRDynamicData *dest, 
    const ZRDynamicData *source);

extern DCPSDLL DDS_ReturnCode_t ZRDynamicDataTypeSupportInitializeData(
    const ZRDynamicDataTypeSupport *self, 
    ZRDynamicData *data);

extern DCPSDLL DDS_ReturnCode_t ZRDynamicDataTypeSupportFinalizeData(
    const ZRDynamicDataTypeSupport *self, 
    ZRDynamicData *data);

extern DCPSDLL TypeCode* ZRDynamicDataTypeSupportGetTypeCode(
    ZRDynamicDataTypeSupport* self);

#ifdef __cplusplus
}
#endif

#endif /* _ZRDDS_INCLUDE_DYNAMIC_DATA */
#endif 
