/**
 * @file:       PublisherListener.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef PublisherListener_h__
#define PublisherListener_h__

#include "DataWriterListener.h"

/**
 * @struct DDS_PublisherListener
 *
 * @ingroup CListener
 *
 * @brief   发布者监听器类型。
 */

typedef struct DDS_PublisherListener
{
    /** @brief   该成员用于监听发布者的数据写者子实体的状态。 */
    DDS_DataWriterListener datawriter_listener;
}DDS_PublisherListener;

/**
 * @def DDS_PublisherListener_initial(listener)
 *
 * @ingroup CListener
 *
 * @brief   初始化发布者监听器对象为所有成员为空。
 *
 * @param   listener    需要初始化的发布者监听器对象。
 */

#define DDS_PublisherListener_initial(listener) memset(listener, 0, sizeof(DDS_PublisherListener))

#endif /* PublisherListener_h__*/
