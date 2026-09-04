/**
 * @file:       TopicListener.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef TopicListener_h__
#define TopicListener_h__

#include "Listener.h"
#include "InconsistentTopicStatus.h"
#include "ZRDDSCWrapper.h"

/**
 * @typedef void(*TopicListenerInconsistentTopicCallback)( DDS_Topic* topic, const DDS_InconsistentTopicStatus* status)
 *
 * @ingroup CListener
 *
 * @brief   底层 #DDS_INCONSISTENT_TOPIC_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out]  topic   该监听器关联的主题。
 * @param   status          当前的状态值。
 */

typedef void(*TopicListenerInconsistentTopicCallback)(
    DDS_Topic* topic,
    const DDS_InconsistentTopicStatus* status);

/**
 * @struct DDS_TopicListener
 *
 * @ingroup CListener
 *
 * @brief   主题监听器类型。
 */

typedef struct DDS_TopicListener
{
    /** @brief   “父类”成员。 */
    DDS_Listener listener;
    /** @brief  为 #DDS_INCONSISTENT_TOPIC_STATUS 状态设置的回调函数。 */
    TopicListenerInconsistentTopicCallback on_inconsistent_topic;
}DDS_TopicListener;

/**
 * @def DDS_TopicListener_initial(listener)
 *
 * @ingroup CListener
 *
 * @brief   初始化主题监听器对象为所有成员为空。
 *
 * @param   listener    需要初始化的主题监听器对象。
 */

#define DDS_TopicListener_initial(listener) memset(listener, 0, sizeof(DDS_TopicListener))

#endif /* TopicListener_h__*/
