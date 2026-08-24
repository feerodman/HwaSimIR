/**
 * @file:       DataReaderListener.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef DataReaderListener_h__
#define DataReaderListener_h__

#include "Listener.h"
#include "SampleInfo.h"
#include "RequestedDeadlineMissedStatus.h"
#include "LivelinessChangedStatus.h"
#include "RequestedIncompatibleQosStatus.h"
#include "SampleRejectedStatus.h"
#include "SubscriptionMatchedStatus.h"
#include "SampleLostStatus.h"
#include "ZRDDSCWrapper.h"
#include <stdio.h>

#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS

/**
 * @typedef void(*DataReaderListenerRequestedDeadlineMissedCallback)( DDS_DataReader* reader, const DDS_RequestedDeadlineMissedStatus* status)
 *
 * @ingroup CListener
 *
 * @brief   底层 #DDS_REQUESTED_DEADLINE_MISSED_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out]  reader  该监听器关联的数据读者。
 * @param   status          当前的状态值。
 */

typedef void(*DataReaderListenerRequestedDeadlineMissedCallback)(
    DDS_DataReader* reader,
    const DDS_RequestedDeadlineMissedStatus* status);
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */

#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS

/**
 * @typedef void(*DataReaderListenerLivelinessChangedCallback)( DDS_DataReader* reader, const DDS_LivelinessChangedStatus* status)
 *
 * @ingroup CListener
 *
 * @brief   底层 #DDS_LIVELINESS_LOST_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out]  reader  该监听器关联的数据读者。
 * @param   status          当前的状态值。
 */

typedef void(*DataReaderListenerLivelinessChangedCallback)(
    DDS_DataReader* reader,
    const DDS_LivelinessChangedStatus* status);
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */

/**
 * @typedef void(*DataReaderListenerRequestedIncompatibleQosCallback)( DDS_DataReader* reader, const DDS_RequestedIncompatibleQosStatus* status)
 *
 * @ingroup CListener
 *
 * @brief   底层 #DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out]  reader  该监听器关联的数据读者。
 * @param   status          当前的状态值。
 */

typedef void(*DataReaderListenerRequestedIncompatibleQosCallback)(
    DDS_DataReader* reader,
    const DDS_RequestedIncompatibleQosStatus* status);

/**
 * @typedef void(*DataReaderListenerSampleRejectedCallback)( DDS_DataReader* reader, const DDS_SampleRejectedStatus* status)
 *
 * @ingroup CListener
 *
 * @brief   底层 #DDS_SAMPLE_REJECTED_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out]  reader  该监听器关联的数据读者。
 * @param   status          当前的状态值。
 */

typedef void(*DataReaderListenerSampleRejectedCallback)(
    DDS_DataReader* reader,
    const DDS_SampleRejectedStatus* status);

/**
 * @typedef void(*DataReaderListenerDataAvailableCallback)( DDS_DataReader* reader)
 *
 * @ingroup CListener
 *
 * @brief   有新的样本存储到数据读者底层时回调。
 *
 * @details 该回调不包含新的数据样本的内容，用户需要通过 @ref read-take 系列按照要求获取数据样本的内容。
 *
 * @param [in,out]  reader  该监听器关联的数据读者。
 */

typedef void(*DataReaderListenerDataAvailableCallback)(
    DDS_DataReader* reader);

/**
 * @typedef void(*DataReaderListenerSubscriptionMatchedCallback)( DDS_DataReader* reader, const DDS_SubscriptionMatchedStatus* status)
 *
 * @ingroup CListener
 *
 * @brief   底层 #DDS_SUBSCRIPTION_MATCHED_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out]  reader  该监听器关联的数据读者。
 * @param   status          当前的状态值。
 */

typedef void(*DataReaderListenerSubscriptionMatchedCallback)(
    DDS_DataReader* reader,
    const DDS_SubscriptionMatchedStatus* status);

/**
 * @typedef void(*DataReaderListenerSampleLostCallback)( DDS_DataReader* reader, const DDS_SampleLostStatus* status)
 *
 * @ingroup CListener
 *
 * @brief   底层 #DDS_SAMPLE_LOST_STATUS 状态的回调函数函数指针类型。
 *
 * @param [in,out]  reader  该监听器关联的数据读者。
 * @param   status          当前的状态值。
 */

typedef void(*DataReaderListenerSampleLostCallback)(
    DDS_DataReader* reader,
    const DDS_SampleLostStatus* status);

typedef void(*DataReaderListenerSampleArrivedCallback)(
    DDS_DataReader* reader,
    void* sample,
    const DDS_SampleInfo* sampleInfo);

/**
 * @struct DDS_DataReaderListener
 *
 * @ingroup  CListener
 *
 * @brief   数据读者监听器类型。
 */

typedef struct DDS_DataReaderListener
{
    /** @brief   “基类对象” */
    DDS_Listener listener;
#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
    /** @brief  为 #DDS_REQUESTED_DEADLINE_MISSED_STATUS 状态设置的回调函数。 */
    DataReaderListenerRequestedDeadlineMissedCallback
        on_requested_deadline_missed;
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */
    /** @brief  为 #DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS 状态设置的回调函数。 */
    DataReaderListenerRequestedIncompatibleQosCallback
        on_requested_incompatible_qos;
    /** @brief  为 #DDS_SAMPLE_REJECTED_STATUS 状态设置的回调函数。 */
    DataReaderListenerSampleRejectedCallback 
        on_sample_rejected;
    /** @brief  为 #DDS_SAMPLE_LOST_STATUS 状态设置的回调函数。 */
    DataReaderListenerSampleLostCallback
        on_sample_lost;
#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
    /** @brief  为 #DDS_LIVELINESS_LOST_STATUS 状态设置的回调函数。 */
    DataReaderListenerLivelinessChangedCallback
        on_liveliness_changed;
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */
    /** @brief  有新的样本存储到数据读者底层时设置的回调函数。 */
    DataReaderListenerDataAvailableCallback 
        on_data_available;
    DataReaderListenerSampleArrivedCallback
        on_data_arrived;
    /** @brief  为 #DDS_SUBSCRIPTION_MATCHED_STATUS 状态设置的回调函数。 */
    DataReaderListenerSubscriptionMatchedCallback 
        on_subscription_matched;
} DDS_DataReaderListener;

/**
 * @def DDS_SimpleDataReaderListener(NAME, TYPE)
 *
 * @ingroup  CListener
 *
 * @brief   用于简化数据读者监听器中数据到达函数的代码复杂度，该宏将完成以下工作：
 *          - 定义 NAME_on_data_available 方法，该方法在DDS底层在数据读者有数据到达时调用，并执行从底层获取所有新的数据并依次调用 NAME_on_process_sample 方法。
 *          - 声明了函数 NAME_on_process 的返回值以及函数名。
 *
 *          关联相同类型（可能是不同的主题）可以使用同一个回调函数，
 *          使用 `const DDS_Char* topicName = DDS_TopicDescription_get_name(DataReaderImplGetTopicDescription(reader));` 获取该数据所属主题。
 *
 * @param   NAME    为回调函数名称前缀。
 * @param   TYPE    用于指明处理的样本类型。
 *
 * @details 当用户需要定义数据回调接口时，假设取名前缀为Track，主题关联的类型为 DDS_ZeroCopyBytes 类型，通过以下代码完成：
 *
 *          @code{.c}
 *          // 声明实现数据处理函数，其中 Track 和 DDS_ZeroCopyBytes 为 DDS_SimpleDataReaderListener 宏参数
 *          // (DDS_DataReader* reader, DDS_ZeroCopyBytes* sample, DDS_SampleInfo* info)为 Track_on_process_sample 的函数参数
 *          // reader 表示数据到达的数据读者
 *          // sample 表示新到达的数据样本
 *          // info 表示与数据样本关联的样本信息
 *          SimpleDataReaderListener(Track, DDS_ZeroCopyBytes)(DDS_DataReader* reader, DDS_ZeroCopyBytes* sample, DDS_SampleInfo* info)
 *          {
 *              
 *          }
 *          // 其他代码
 *          // ...
 *          // 在创建数据读者时，为数据读者监听器成员赋值。
 *          DDS_DataReaderListener drListener;
 *          DDS_DataReaderListener_initial(&drListener);
 *          drListener.on_data_available = Track_on_data_available;
 *          @endcode
 */

#define DDS_SimpleDataReaderListener(NAME, TYPE)                                                \
    void NAME##_on_process_sample(DDS_DataReader* reader, TYPE* sample, DDS_SampleInfo* info);  \
    void NAME##_on_data_available(DDS_DataReader* the_reader)                                   \
    {                                                                                           \
        TYPE##Seq data_values;                                                                  \
        TYPE##Seq_initialize(&data_values);                                                     \
        DDS_SampleInfoSeq sample_infos;                                                         \
        DDS_SampleInfoSeq_initialize(&sample_infos);                                            \
        TYPE##DataReader* reader = (TYPE##DataReader*)the_reader;                               \
        DDS_ReturnCode_t retCode = TYPE##DataReader_take(reader, &data_values,                  \
            &sample_infos, LENGTH_UNLIMITED,                                                    \
            DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);                  \
        if (DDS_RETCODE_OK != retCode)                                                          \
        {                                                                                       \
            printf("take failed(%d.\n", retCode);                                               \
            return;                                                                             \
        }                                                                                       \
        ZR_UINT32 i;                                                                            \
        for (i = 0; i < sample_infos._length; i++)                                              \
        {                                                                                       \
            DDS_SampleInfo* info = DDS_SampleInfoSeq_get_reference(&sample_infos, i);           \
            if (!info->valid_data)                                                              \
            {                                                                                   \
                continue;                                                                       \
            }                                                                                   \
            TYPE* sample = TYPE##Seq_get_reference(&data_values, i);                 \
            NAME##_on_process_sample(the_reader, sample, info);                                 \
        }                                                                                       \
        TYPE##DataReader_return_loan(reader, &data_values, &sample_infos);                      \
        return;                                                                                 \
    }                                                                                           \
    void NAME##_on_process_sample

/**
 * @def DDS_DataReaderListener_initial(listener)
 *
 * @ingroup CListener
 *
 * @brief   初始化数据读者监听器对象为所有成员为空。
 *
 * @param   listener    需要初始化的数据读者监听器对象。
 */

#define DDS_DataReaderListener_initial(listener) memset(listener, 0, sizeof(DDS_DataReaderListener))

#endif /* DataReaderListener_h__*/
