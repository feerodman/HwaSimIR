/**
 * @file:       BuiltinDataDataReader.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef BuiltinDataDataReader_h__
#define BuiltinDataDataReader_h__

#include "ZRDDSDataReader.h"
#include "ParticipantBuiltinTopicData.h"
#include "PublicationBuiltinTopicData.h"
#include "SubscriptionBuiltinTopicData.h"
#include "TopicBuiltinTopicData.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @struct DDS_ParticipantBuiltinTopicDataSeq
 *
 * @ingroup CDiscoveryTypes
 *
 * @brief   声明内置数据类型 DDS_ParticipantBuiltinTopicData 的序列。
 */

/**  
 * @class DDS_ParticipantBuiltinTopicDataDataReader
 *
 * @ingroup CDiscoveryTypes
 *
 * @brief 声明内置数据类型 DDS_ParticipantBuiltinTopicData 关联的数据读者
 *  
 * @details 用于从ZRDDS中获取内置域参与者主题数据及其状态，该类由 ZRDDSDataReader 宏实例化，模板参数为： 
 *          DDS_ParticipantBuiltinTopicData DDS_ParticipantBuiltinTopicDataSeq ，用法参见 
 *          #DDS_DomainParticipant_get_builtin_subscriber 。
 */

typedef struct DDS_ParticipantBuiltinTopicDataDataReader DDS_ParticipantBuiltinTopicDataDataReader;
ZRDDSDataReader(DDS_ParticipantBuiltinTopicDataDataReader, DDS_ParticipantBuiltinTopicDataSeq, DDS_ParticipantBuiltinTopicData);

/**
 * @struct DDS_PublicationBuiltinTopicDataSeq
 *
 * @ingroup CDiscoveryTypes
 *
 * @brief   声明内置数据类型 DDS_PublicationBuiltinTopicData 的序列。
 */

/**  
 * @class DDS_PublicationBuiltinTopicDataDataReader
 *
 * @ingroup CDiscoveryTypes
 *
 * @brief 声明内置数据类型 DDS_PublicationBuiltinTopicData 关联的数据读者
 *  
 * @details 用于从ZRDDS中获取内置域参与者主题数据及其状态，该类由 ZRDDSDataReader 宏实例化，模板参数为： 
 *          DDS_PublicationBuiltinTopicData DDS_PublicationBuiltinTopicDataSeq ，用法参见 
 *          #DDS_DomainParticipant_get_builtin_subscriber 。
 */

typedef struct DDS_PublicationBuiltinTopicDataDataReader DDS_PublicationBuiltinTopicDataDataReader;
ZRDDSDataReader(DDS_PublicationBuiltinTopicDataDataReader, DDS_PublicationBuiltinTopicDataSeq, DDS_PublicationBuiltinTopicData);

/**
 * @struct DDS_PublicationBuiltinTopicDataSeq
 *
 * @ingroup CDiscoveryTypes
 *
 * @brief   声明内置数据类型 DDS_PublicationBuiltinTopicData 的序列。
 */

/**  
 * @class DDS_SubscriptionBuiltinTopicDataDataReader
 *
 * @ingroup CDiscoveryTypes
 *
 * @brief 声明内置数据类型 DDS_SubscriptionBuiltinTopicData 关联的数据读者
 *  
 * @details 用于从ZRDDS中获取内置域参与者主题数据及其状态，该类由 ZRDDSDataReader 宏实例化，模板参数为： 
 *          DDS_SubscriptionBuiltinTopicData DDS_SubscriptionBuiltinTopicDataSeq ，用法参见 
 *          #DDS_DomainParticipant_get_builtin_subscriber 。
 */

typedef struct DDS_SubscriptionBuiltinTopicDataDataReader DDS_SubscriptionBuiltinTopicDataDataReader;
ZRDDSDataReader(DDS_SubscriptionBuiltinTopicDataDataReader, DDS_SubscriptionBuiltinTopicDataSeq, DDS_SubscriptionBuiltinTopicData);

#ifdef _ZRDDS_INCLUDE_TOPIC_BUILTIN_TOPIC_DATA


/**  
 * @class DDS_TopicBuiltinTopicDataDataReader
 *
 * @ingroup CDiscoveryTypes
 *
 * @brief 声明内置数据类型 DDS_TopicBuiltinTopicData 关联的数据读者
 *  
 * @details 用于从ZRDDS中获取内置订阅端主题数据，该类由 ZRDDSDataReader 宏实例化，模板参数为：
 *          DDS_TopicBuiltinTopicData DDS_TopicBuiltinTopicDataSeq ，用法参见
 *          #DDS_DomainParticipant_get_builtin_subscriber 。
 */
typedef struct DDS_TopicBuiltinTopicDataDataReader DDS_TopicBuiltinTopicDataDataReader;
ZRDDSDataReader(DDS_TopicBuiltinTopicDataDataReader, DDS_TopicBuiltinTopicDataSeq, DDS_TopicBuiltinTopicData);

#endif /* _ZRDDS_INCLUDE_TOPIC_BUILTIN_TOPIC_DATA */

#ifdef __cplusplus
}
#endif

#endif /* BuiltinDataDataReader_h__*/
