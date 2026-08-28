/**
 * @file:       ZRDDSTypeSupport.cpp
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#include <stdio.h>
#include "ZRDDSTypeSupport.h"
#include "ZRDDSTypePlugin.h"

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
#define ZRDDSOnSiteDeserilizeFunc(TType)                                                                            \
    result.m_noSerializingSupportedFunc = (TypePluginNoSerializingSupportedFunction)TType##NoSerializingSupported;  \
    result.m_fixedHeaderLengthFunc = (TypePluginFixedHeaderLengthFunction)TType##FixedHeaderLength;                 \
    result.m_onSiteDeserializeFunc = (TypePluginOnSiteDeserializeFunction)TType##OnSiteDeserialize;
#else
#define ZRDDSOnSiteDeserilizeFunc(TType)
#endif

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
#define ZRDDSNoDeserilizeFunc(TType)                                                                            \
    result.m_loanSampleBufFunc = (TypePluginLoanSampleBufFunction)TType##LoanSampleBuf;                         \
    result.m_returnSampleBufFunc = (TypePluginReturnSampleBufFunction)TType##ReturnSampleBuf;                   \
    result.m_loanDeserializeFunc = (TypePluginLoanDeserializeFunction)TType##LoanDeserialize;
#else
#define ZRDDSNoDeserilizeFunc(TType)
#endif

/**
 * @def DDSTypeSupportImpl(TTypeSupport, TType, TPluginNew, TPluginDelete, TTypeNameVar)
 *
 * @brief   TTypeSupport的定义宏。
 *
 * @author  Hzy
 * @date    2016/6/28
 *
 * @param   TTypeSupport    对应的TTypeSupport类型。
 * @param   TType           对应的数据类型。
 * @param   TPluginNew      实现依赖的创建TypePlugin的接口。
 * @param   TPluginDelete   实现依赖的删除TypePlugin的接口。
 * @param   TTypeNameVar    数据类型名称变量。
 */

#define DDSTypeSupportImpl(TTypeSupport, TType, TTypeNameVar)   \
    ZRDDSTypePlugin* TType##PluginNew() \
    {                                   \
        static ZRDDSTypePlugin result;  \
        ZR_BOOLEAN initialized = false; \
        if (initialized)                \
        {                               \
            return &result;             \
        }                               \
        result.m_createSampleFunc = (TypePluginCreateSampleFunction)TType##CreateSample;        \
        result.m_destroySampleFunc = (TypePluginDestroySampleFunction)TType##DestroySample;     \
        result.m_copySampleFunc = (TypePluginCopySampleFunction)TType##CopyEx;                  \
        result.m_getMaxSizeFunc = (TypePluginGetMaxSizeFunction)TType##GetSerializedSampleMaxSize;   \
        result.m_getSizeFunc = (TypePluginGetSizeFunction)TType##GetSerializedSampleSize;       \
        result.m_serializeFunc = (TypePluginSerializeFunction)TType##Serialize;                 \
        result.m_deserializeFunc = (TypePluginDeserializeFunction)TType##Deserialize;           \
        result.m_getMaxKeySizeFunc = (TypePluginGetMaxKeySizeFunction)TType##GetSerializedKeyMaxSize;\
        result.m_getKeySizeFunc = (TypePluginGetKeySizeFunction)TType##GetSerializedKeySize;    \
        result.m_serializeKeyFunc = (TypePluginSerializeKeyFunction)TType##SerializeKey;        \
        result.m_deserializeKeyFunc = (TypePluginDeserializeKeyFunction)TType##DeserializeKey;  \
        result.m_getKeyHashFunc = (TypePluginGetKeyHashFunction)TType##GetKeyHash;              \
        result.m_hasKeyFunc = (TypePluginHasKeyFunction)TType##HasKey;                          \
        result.m_typecodeFunc = (TypePluginGetTypeCodeFunction)TType##GetInnerTypeCode;         \
        ZRDDSOnSiteDeserilizeFunc(TType)                                                        \
        ZRDDSNoDeserilizeFunc(TType)                                                            \
        initialized = true;                                                                     \
        return &result;                                     \
    }                                                       \
    const DDS_Char* TTypeSupport##_get_type_name()          \
    {                                                       \
        return TTypeNameVar;                                \
    }                                                       \
    DDS_ReturnCode_t TTypeSupport##_register_type(          \
        DDS_DomainParticipant* participant,                 \
        const DDS_Char* typeName)                           \
    {                                                       \
        ZRDDSTypePlugin* plugin = TType##PluginNew();       \
        if (NULL == plugin)                                 \
        {                                                   \
            printf("%s create TypePlugin failed.", #TTypeSupport);           \
            return DDS_RETCODE_ERROR;                       \
        }                                                   \
        if (participant == NULL)                            \
        {                                                   \
            return DDS_RETCODE_BAD_PARAMETER;               \
        }                                                   \
        return DomainParticipantRegisterType(participant,   \
            typeName == NULL ? TTypeSupport##_get_type_name() : typeName,   \
            plugin);                                        \
    }                                                       \
    DDS_ReturnCode_t TTypeSupport##_unregister_type(        \
        DDS_DomainParticipant* participant,                 \
        const DDS_Char* typeName)                           \
    {                                                       \
        if (participant == NULL)                            \
        {                                                   \
            return DDS_RETCODE_BAD_PARAMETER;               \
        }                                                   \
        return DomainParticipantUnRegisterType(participant, \
            typeName == NULL ? TTypeSupport##_get_type_name() : typeName);  \
    }
