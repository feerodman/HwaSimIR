/**
 * @file:       DataWriterListener.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef DataWriterListener_h__
#define DataWriterListener_h__

#include "Listener.h"
#include "OfferedDeadlineMissedStatus.h"
#include "LivelinessLostStatus.h"
#include "OfferedIncompatibleQosStatus.h"
#include "PublicationMatchedStatus.h"
#include "ZRDDSCWrapper.h"

#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS

/**
 * @typedef void(*DataWriterListenerOfferedDeadlineMissedCallback)( DDS_DataWriter* writer, const DDS_OfferedDeadlineMissedStatus* status)
 *
 * @ingroup CListener
 *
 * @brief 底层 #DDS_OFFERED_DEADLINE_MISSED_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out] writer 该监听器关联的数据写者。
 * @param status              当前的状态值。
 */

typedef void(*DataWriterListenerOfferedDeadlineMissedCallback)(
    DDS_DataWriter* writer,
    const DDS_OfferedDeadlineMissedStatus* status);
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */

#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS

/**
 * @typedef void(*DataWriterListenerLivelinessLostCallback)( DDS_DataWriter* writer, const DDS_LivelinessLostStatus* status)
 *
 * @ingroup CListener
 *
 * @brief 底层 #DDS_LIVELINESS_LOST_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out] writer 该监听器关联的数据写者。
 * @param status              当前的状态值。
 */

typedef void(*DataWriterListenerLivelinessLostCallback)(
    DDS_DataWriter* writer,
    const DDS_LivelinessLostStatus* status);
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */

/**
 * @typedef void(*DataWriterListenerOfferedIncompatibleQosCallback)( DDS_DataWriter* writer, const DDS_OfferedIncompatibleQosStatus* status)
 *
 * @ingroup CListener
 *
 * @brief 底层 #DDS_OFFERED_INCOMPATIBLE_QOS_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out] writer 该监听器关联的数据写者。
 * @param status              当前的状态值。
 */

typedef void(*DataWriterListenerOfferedIncompatibleQosCallback)(
    DDS_DataWriter* writer,
    const DDS_OfferedIncompatibleQosStatus* status);

/**
 * @typedef void(*DataWriterListenerPublicationMatchedCallback)( DDS_DataWriter* writer, const DDS_PublicationMatchedStatus* status)
 *
 * @ingroup CListener
 *
 * @brief 底层 #DDS_PUBLICATION_MATCHED_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out] writer 该监听器关联的数据写者。
 * @param status              当前的状态值。
 */

typedef void(*DataWriterListenerPublicationMatchedCallback)(
    DDS_DataWriter* writer,
    const DDS_PublicationMatchedStatus* status);

/**
 * @struct DDS_DataWriterListener
 *
 * @ingroup CListener
 *
 * @brief   数据写者监听器类型。
 */

typedef struct DDS_DataWriterListener
{
    /** @brief   "基类"对象。 */
    DDS_Listener listener;
#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
    /** @brief  为 #DDS_OFFERED_DEADLINE_MISSED_STATUS 状态设置的回调函数。 */
    DataWriterListenerOfferedDeadlineMissedCallback on_offered_deadline_missed;
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */
    /** @brief  为 #DDS_OFFERED_INCOMPATIBLE_QOS_STATUS 状态设置的回调函数。 */
    DataWriterListenerOfferedIncompatibleQosCallback on_offered_incompatible_qos;
#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
    /** @brief  为 #DDS_LIVELINESS_LOST_STATUS 状态设置的回调函数。 */
    DataWriterListenerLivelinessLostCallback on_liveliness_lost;
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */
    /** @brief  为 #DDS_PUBLICATION_MATCHED_STATUS 状态设置的回调函数。 */
    DataWriterListenerPublicationMatchedCallback on_publication_matched;
}DDS_DataWriterListener;

/**
 * @def DDS_DataWriterListener_initial(listener)
 *
 * @ingroup CListener
 *
 * @brief   初始化数据写者监听器对象为所有成员为空。
 *
 * @param   listener    需要初始化的数据写者监听器对象。
 */
#define DDS_DataWriterListener_initial(listener) memset(listener, 0, sizeof(DDS_DataWriterListener))

#endif /* DataWriterListener_h__*/
