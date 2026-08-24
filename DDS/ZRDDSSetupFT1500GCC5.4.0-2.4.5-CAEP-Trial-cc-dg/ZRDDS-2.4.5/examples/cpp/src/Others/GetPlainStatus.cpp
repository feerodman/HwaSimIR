#include <stdio.h>

#include "DataReader.h"
#include "WaitSet.h"
#include "SubscriptionMatchedStatus.h"
#include "DataReaderListener.h"
using namespace DDS;
/**
* 以获取 DDS_SubscriptionMatchedStatus 为例，分别通过三种方式获取实体状态。
*/

/**   
 * 方法1.
 */
int SyncGetSubscriptionMatchedStatusStatus(
    DataReader* reader, 
    SubscriptionMatchedStatus& status)
{
    // 方法1：直接调用方法
    reader->get_subscription_matched_status(status);
    return 0;
}

/**   
 * 方法2.
 */
int WaitGetSubscriptionMatchedStatusStatus(
    DataReader* reader,
    SubscriptionMatchedStatus& status)
{
    // 创建WaitSet或者使用已有的WaitSet
    WaitSet* waitset = new WaitSet();
    // 获取DataReader相关的StatusCondition并设置关心DDS_SUBSCRIPTION_MATCHED_STATUS状态，也可以同时设置其他状态
    StatusCondition* statusCondition = reader->get_statuscondition();
    statusCondition->set_enabled_statuses(SUBSCRIPTION_MATCHED_STATUS);
    // 将该条件添加到等待集合
    if (DDS_RETCODE_OK != waitset->attach_condition(statusCondition))
    {
        // 错误处理代码
        return -1;
    }
    // 调用WaitSet的方法阻塞等待状态发生改变，同时设置最长等待时间
    ConditionSeq activeSeq;
    Duration_t timeout;
    timeout.sec = 5;
    timeout.nanosec = 0;
    ReturnCode_t retcode = waitset->wait(activeSeq, timeout);
    // 判断等待结果
    if (RETCODE_TIMEOUT == retcode)
    {
        printf("wait time out.\n");
        return 1;
    }
    if (RETCODE_OK != retcode)
    {
        printf("other error.\n");
        return -1;
    }
    for (DDS_ULong i = 0; i < activeSeq.length(); ++i)
    {
        statusCondition = dynamic_cast<StatusCondition*>(activeSeq[i]);
        // 当设置了多个关心的状态时，应检测该状态是否发生变化，再调用相应的方法获取
        StatusKindMask statusMask = reader->get_status_changes();
        if ((statusMask & SUBSCRIPTION_MATCHED_STATUS) != 0)
        {
            reader->get_subscription_matched_status(status);
            break;
        }
    }
    return 0;
}

/**   
 * 方法3.
 */
class ExampleDataReaderListener: public DataReaderListener
{
public:
    virtual void on_subscription_matched(
        DataReader *the_reader, 
        const SubscriptionMatchedStatus &status) 
    {
        // 底层回调该方法
    }
};

int ListenerGetSubscriptionMatchedStatusStatus(
    DataReader* reader,
    SubscriptionMatchedStatus& status)
{
    // 设置监听器，也可以在创建时指定监听器及其关心的状态，只需要掩码表示的集合中包含该状态即可
    ExampleDataReaderListener* listener = new ExampleDataReaderListener;
    reader->set_listener(listener, SUBSCRIPTION_MATCHED_STATUS);
    // 当底层检测到状态变化时，在 ExampleDataReaderListener::on_subscription_matched 中回调
    return 0;
}