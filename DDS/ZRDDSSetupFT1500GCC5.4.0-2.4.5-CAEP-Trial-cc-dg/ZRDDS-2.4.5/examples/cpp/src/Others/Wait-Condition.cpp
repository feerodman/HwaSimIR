#include <stdio.h>

#include "WaitSet.h"
#include "Condition.h"
#include "ReadCondition.h"
#include "FooDataReader.h"

int WaitCondition(FooDataReader* reader)
{
    SubscriptionMatchedStatus matchStatus;
    LivelinessChangedStatus liveStatus;
    // 创建WaitSet或者使用已有的WaitSet
    WaitSet* waitset = new WaitSet();
    // 获取DataReader相关的StatusCondition并设置关心SUBSCRIPTION_MATCHED_STATUS状态，也可以同时设置其他状态
    StatusCondition* statusCondition = reader->get_statuscondition();
    statusCondition->set_enabled_statuses(SUBSCRIPTION_MATCHED_STATUS | LIVELINESS_CHANGED_STATUS);
    // 创建读取数据条件
    ReadCondition* rdCondition = reader->create_readcondition(
        NOT_READ_SAMPLE_STATE, // 底册存在处于未读取状态的数据样本 
        ANY_VIEW_STATE, // 任意的视图状态
        ANY_INSTANCE_STATE); // 任意的数据实例状态
    if (rdCondition == NULL)
    {
        // 错误处理代码
        return -1;
    }
    // 创建监视条件
    GuardCondition* guardCondition = new GuardCondition();
    // 将状态条件添加到等待集合
    if (RETCODE_OK != waitset->attach_condition(statusCondition))
    {
        // 错误处理代码
        return -1;
    }
    // 将读取条件添加到等待集合
    if (RETCODE_OK != waitset->attach_condition(rdCondition))
    {
        // 错误处理代码
        return -1;
    }
    // 将监视条件添加到等待集合
    if (RETCODE_OK != waitset->attach_condition(guardCondition))
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
        if (activeSeq[i] == statusCondition)
        {
            // 当设置了多个关心的状态时，应检测该状态是否发生变化，再调用相应的方法获取
            StatusKindMask statusMask = reader->get_status_changes();
            if ((statusMask & SUBSCRIPTION_MATCHED_STATUS) != 0)
            {
                reader->get_subscription_matched_status(matchStatus);
                break;
            }
            if ((statusMask & LIVELINESS_CHANGED_STATUS) != 0)
            {
                reader->get_liveliness_changed_status(liveStatus);
                break;
            }
        }
        else if (activeSeq[i] == rdCondition)
        {
            FooSeq sampleSeq;
            SampleInfoSeq infoSeq;
            if (RETCODE_OK != reader->read_w_condition(sampleSeq, infoSeq, MAX_INT32_VALUE, rdCondition))
            {
                // 错误处理
                return -1;
            }
            // 处理获取成功的样本
            reader->return_loan(sampleSeq, infoSeq);
        }
        else if (activeSeq[i] == guardCondition)
        {
            // 处理
        }
    }
    return 0;
}
