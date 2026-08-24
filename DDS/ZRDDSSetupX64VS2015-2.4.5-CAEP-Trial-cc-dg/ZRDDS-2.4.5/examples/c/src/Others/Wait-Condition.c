#include <stdio.h>

#include "WaitSet.h"
#include "Condition.h"
#include "FooDataReader.h"
#include "StatusKind.h"

int WaitCondition(FooDataReader* reader)
{
    FooDataReader* _reader = (FooDataReader*)reader;
    DDS_Entity* entity = FooDataReader_as_entity(_reader);
    DDS_SubscriptionMatchedStatus matchStatus;
    DDS_LivelinessChangedStatus liveStatus;
    /*创建WaitSet或者使用已有的WaitSet*/
    DDS_WaitSet* waitset = DDS_WaitSet_new();
    /*获取DataReader相关的StatusCondition并设置关心DDS_SUBSCRIPTION_MATCHED_STATUS状态，也可以同时设置其他状态*/
    DDS_StatusCondition* statusCondition = DDS_Entity_get_statuscondition(entity);
    DDS_StatusCondition_set_enabled_statuses(statusCondition, DDS_SUBSCRIPTION_MATCHED_STATUS | DDS_LIVELINESS_CHANGED_STATUS);
    /*创建读取数据条件*/
    DDS_ReadCondition* rdCondition = FooDataReader_create_readcondition(_reader,
        DDS_NOT_READ_SAMPLE_STATE, /*底册存在处于未读取状态的数据样本*/
        DDS_ANY_VIEW_STATE, /*任意的视图状态*/
        DDS_ANY_INSTANCE_STATE); /*任意的数据实例状态*/
    if (rdCondition == NULL)
    {
        /*错误处理代码*/
        return -1;
    }
    /*创建监视条件*/
    DDS_GuardCondition* guardCondition = DDS_GuardCondition_new();
    /*将状态条件添加到等待集合*/
    if (DDS_RETCODE_OK != DDS_WaitSet_attach_condition(waitset, (DDS_Condition*)statusCondition))
    {
        /*错误处理代码*/
        return -1;
    }
    /*将读取条件添加到等待集合*/
    if (DDS_RETCODE_OK != DDS_WaitSet_attach_condition(waitset, (DDS_Condition*)rdCondition))
    {
        /*错误处理代码*/
        return -1;
    }
    /*将监视条件添加到等待集合*/
    if (DDS_RETCODE_OK != DDS_WaitSet_attach_condition(waitset, (DDS_Condition*)guardCondition))
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
        DDS_Condition* _condition = (DDS_Condition*)DDS_ConditionSeq_get_reference(&activeSeq, i);
        if ((DDS_StatusCondition*)_condition == statusCondition)
        {
            /*当设置了多个关心的状态时，应检测该状态是否发生变化，再调用相应的方法获取*/
            DDS_StatusKindMask statusMask = DDS_Entity_get_status_changes(entity);
            if ((statusMask & DDS_SUBSCRIPTION_MATCHED_STATUS) != 0)
            {
                FooDataReader_get_subscription_matched_status(_reader, &matchStatus);
                break;
            }
            if ((statusMask & DDS_LIVELINESS_CHANGED_STATUS) != 0)
            {
                FooDataReader_get_liveliness_changed_status(_reader, &liveStatus);
                break;
            }
        }
        else if ((DDS_ReadCondition*)_condition == rdCondition)
        {
            FooSeq sampleSeq;
            DDS_SampleInfoSeq infoSeq;
            if (DDS_RETCODE_OK != FooDataReader_read_w_condition(_reader, &sampleSeq, &infoSeq, MAX_INT32_VALUE, rdCondition))
            {
                /*错误处理*/
                return -1;
            }
            /*处理获取成功的样本*/
            FooDataReader_return_loan(_reader, &sampleSeq, &infoSeq);
        }
        else if ((DDS_GuardCondition*)_condition == guardCondition)
        {
            /*处理*/
        }
    }
    return 0;
}
