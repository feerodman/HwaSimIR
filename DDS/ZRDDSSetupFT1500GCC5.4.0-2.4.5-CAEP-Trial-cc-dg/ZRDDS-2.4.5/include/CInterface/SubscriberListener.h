/**
 * @file:       SubscriberListener.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef SubscriberListener_h__
#define SubscriberListener_h__

#include "DataReaderListener.h"
#include "ZRDDSCWrapper.h"

/**
 * @typedef void(*SubscriberListenerDataOnReadersCallback)(DDS_Subscriber* sub)
 *
 * @ingroup  CListener
 *
 * @brief   回调 #DDS_DATA_ON_READERS_STATUS 状态的回调函数函数指针类型。
 *
 * @details 当订阅者的子实体数据读者有新数据达到时，订阅者将处于此状态，以下情况将清楚该状态：
 *          - 订阅者监听器设置的关心的状态包含 #DDS_DATA_ON_READERS_STATUS ；
 *          - 用户调用 #DDS_Subscriber_notify_datareaders 函数；
 *          - 用户成功调用 DDS_SubscriberListener::on_data_available 方法；
 *          - 用户调用该域参与者中任意数据读者的 @ref read-take 系列方法。
 *
 * @param   sub 该监听器所属的订阅者。
 */

typedef void(*SubscriberListenerDataOnReadersCallback)(DDS_Subscriber* sub);

/**
 * @struct DDS_SubscriberListener
 *
 * @ingroup CListener
 *
 * @brief   订阅者监听器类型。
 */

typedef struct DDS_SubscriberListener
{
    /** @brief   该成员用于监听订阅者的数据读者子实体的状态。 */
    DDS_DataReaderListener datareader_listener;
    /** @brief  为 #DDS_DATA_ON_READERS_STATUS 状态设置的回调函数。 */
    SubscriberListenerDataOnReadersCallback on_data_on_readers;
}DDS_SubscriberListener;

/**
 * @def DDS_SubscriberListener_initial(listener)
 *
 * @ingroup CListener
 *
 * @brief   初始化订阅者监听器对象为所有成员为空。
 *
 * @param   listener    需要初始化的订阅者监听器对象。
 */

#define DDS_SubscriberListener_initial(listener) memset(listener, 0, sizeof(DDS_SubscriberListener))

#endif /* SubscriberListener_h__*/
