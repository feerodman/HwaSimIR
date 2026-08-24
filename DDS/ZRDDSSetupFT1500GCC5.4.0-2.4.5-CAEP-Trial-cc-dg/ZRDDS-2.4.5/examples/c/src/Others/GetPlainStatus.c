#include <stdio.h>
#include <string.h>
#include "DataReader.h"
#include "WaitSet.h"
#include "SubscriptionMatchedStatus.h"
#include "DataReaderListener.h"
#include "FooDataReader.h"
#include "StatusKind.h"
/**
* 以获取 DDS_SubscriptionMatchedStatus 为例，分别通过三种方式获取实体状态。
*/

/**
 * 方法1.
 */
int SyncGetSubscriptionMatchedStatusStatus(
    DDS_DataReader* reader,
    DDS_SubscriptionMatchedStatus* status)
{
    FooDataReader* _reader = (FooDataReader*)reader;
    /*方法1：直接调用方法*/
    FooDataReader_get_subscription_matched_status(_reader, status);
    return 0;
}

/**
* 方法2.
*/
int WaitGetSubscriptionMatchedStatusStatus(
    DDS_DataReader* reader,
    DDS_SubscriptionMatchedStatus* status)
{
    FooDataReader* _reader = (FooDataReader*)reader;
    DDS_Entity* entity = FooDataReader_as_entity(_reader);
    /*创建WaitSet或者使用已有的WaitSet*/
    DDS_WaitSet *waitset = DDS_WaitSet_new();
    /*获取DataReader相关的StatusCondition并设置关心DDS_SUBSCRIPTION_MATCHED_STATUS状态，也可以同时设置其他状态*/
    DDS_StatusCondition* statusCondition = DDS_Entity_get_statuscondition(entity);
    DDS_StatusCondition_set_enabled_statuses(statusCondition, DDS_SUBSCRIPTION_MATCHED_STATUS);
    /*将该条件添加到等待集合*/
    if (DDS_RETCODE_OK != DDS_WaitSet_attach_condition(waitset, (DDS_Condition*)statusCondition))
    {
        /*错误处理代码*/
        return -1;
    }
    /*调用WaitSet的方法阻塞等待状态发生改变，同时设置最长等待时间*/
    DDS_ConditionSeq activeSeq;
    DDS_Duration_t timeout;
    timeout.sec = 5;
    timeout.nanosec = 0;
    DDS_ReturnCode_t retcode = DDS_WaitSet_wait(waitset, &activeSeq, &timeout);
    /*判断等待结果*/
    if (DDS_RETCODE_TIMEOUT == retcode)
    {
        printf("wait time out.\n");
        return 1;
    }
    if (DDS_RETCODE_OK != retcode)
    {
        printf("other error.\n");
        return -1;
    }
    for (ZR_UINT32 i = 0; i < DDS_ConditionSeq_get_length(&activeSeq); ++i)
    {
        statusCondition = (DDS_StatusCondition*)DDS_ConditionSeq_get_reference(&activeSeq, i);
        /*当设置了多个关心的状态时，应检测该状态是否发生变化，再调用相应的方法获取*/
        DDS_StatusKindMask statusMask = DDS_Entity_get_status_changes(entity);
        if ((statusMask & DDS_SUBSCRIPTION_MATCHED_STATUS) != 0)
        {
            FooDataReader_get_subscription_matched_status(_reader, status);
            break;
        }
    }
    return 0;
}

/**
* 方法3.
*/
class ExampleDataReaderListener : public DDS_DataReaderListener
{
public:
    virtual void on_subscription_matched(
        DDS_DataReader *the_reader,
        const DDS_SubscriptionMatchedStatus &status)
    {
        /*底层回调该方法*/
    }
};

void Example_on_subscription_matched(DDS_DataReader *the_reader,
    const DDS_SubscriptionMatchedStatus *status)
{
    /*底层回调该方法*/
}

int ListenerGetSubscriptionMatchedStatusStatus(
    DDS_DataReader* reader,
    DDS_SubscriptionMatchedStatus* status)
{
    FooDataReader* _reader = (FooDataReader*)reader;
    /*设置监听器，也可以在创建时指定监听器及其关心的状态，只需要掩码表示的集合中包含该状态即可*/
    DDS_DataReaderListener readerListener;
    memset(&readerListener, 0, sizeof(readerListener));
    readerListener.on_subscription_matched = Example_on_subscription_matched;
    FooDataReader_set_listener(_reader, &readerListener, DDS_SUBSCRIPTION_MATCHED_STATUS);
    /*当底层检测到状态变化时，在 ExampleDataReaderListener::on_subscription_matched 中回调*/
    return 0;
}