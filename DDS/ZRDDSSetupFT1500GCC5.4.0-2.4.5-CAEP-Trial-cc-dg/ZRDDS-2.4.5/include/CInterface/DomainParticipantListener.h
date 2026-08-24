/**
 * @file:       DomainParticipantListener.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef DomainParticipantListener_h__
#define DomainParticipantListener_h__

#include "TopicListener.h"
#include "PublisherListener.h"
#include "SubscriberListener.h"

/**
 * @struct DDS_DomainParticipantListener
 *
 * @ingroup  CListener
 *
 * @brief   域参与者监听器类型。
 */

typedef struct DDS_DomainParticipantListener
{
    /** @brief   该成员用于监听域参与者主题子实体的状态。 */
    DDS_TopicListener topiclistener;
    /** @brief   该成员用于监听域参与者发布者子实体的状态。 */
    DDS_PublisherListener publisherlistener;
    /** @brief   该成员用于监听域参与者订阅者子实体的状态。 */
    DDS_SubscriberListener subscriberlistener;
}DDS_DomainParticipantListener ;

/**
 * @def DDS_DomainParticipantListener_initial(listener)
 *
 * @ingroup CListener
 *
 * @brief   初始化域参与者监听器对象为所有成员为空。
 *
 * @param   listener    需要初始化的域参与者监听器对象。
 */

#define DDS_DomainParticipantListener_initial(listener) memset(listener, 0, sizeof(DDS_DomainParticipantListener))

#endif /* DomainParticipantListener_h__*/
