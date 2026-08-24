/**
 * @file:       Listener.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef Listener_h__
#define Listener_h__

/**
 * @struct DDS_Listener
 *
 * @ingroup CListener
 *
 * @brief   所有实体监听器类型的“父类”。
 */

typedef struct DDS_Listener
{
    /** @brief   无用数据，用于填充空的Listener。 */
    void* user_data;
}DDS_Listener;

#endif /* Listener_h__*/
