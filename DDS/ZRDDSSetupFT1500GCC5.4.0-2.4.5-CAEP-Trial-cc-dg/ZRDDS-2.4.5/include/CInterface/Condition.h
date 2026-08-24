/**
 * @file:       Condition.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef Condition_h__
#define Condition_h__

#include "ZRDDSCommon.h"
#include "OsResource.h"
#include "ReturnCode_t.h"
#include "ViewStateMask.h"
#include "SampleStateMask.h"
#include "InstanceStateMask.h"
#include "ZRSequence.h"
#include "StatusKindMask.h"
#include "ZRDDSCWrapper.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @fn  DCPSDLL DDS_Boolean DDS_StatusCondition_get_trigger_value(const DDS_StatusCondition* conditon);
 *
 * @ingroup CInfrastruct
 *
 * @brief   获取状态条件当前的触发状态。
 *
 * @param   conditon    statusCondition。
 *
 * @return  true表示被触发，false表示该条件还未被触发。
 */

DCPSDLL DDS_Boolean DDS_StatusCondition_get_trigger_value(
    const DDS_StatusCondition* conditon);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_StatusCondition_set_enabled_statuses( DDS_StatusCondition* conditon, DDS_StatusKindMask enabledStatuses);
 *
 * @ingroup CInfrastruct
 *
 * @brief   以掩码的方式设置状态条件中用户关心的状态。
 *
 * @param [in,out]  conditon    指明目标。
 * @param   enabledStatuses     掩码，用户关心的状态的位或结果。
 *
 * @return  当前总是返回 #DDS_RETCODE_OK 。
 */

DCPSDLL DDS_ReturnCode_t DDS_StatusCondition_set_enabled_statuses(
    DDS_StatusCondition* conditon, 
    DDS_StatusKindMask enabledStatuses);

/**
 * @fn  DCPSDLL DDS_StatusKindMask DDS_StatusCondition_get_enabled_statuses( const DDS_StatusCondition* conditon);
 *
 * @ingroup CInfrastruct
 *
 * @brief   获取前一次用户通过 ::DDS_StatusCondition_set_enabled_statuses 方法设置的关心的状态。
 *
 * @param   conditon    指明目标。
 *
 * @return  掩码方式返回当前状态条件所关联的状态集合。
 */

DCPSDLL DDS_StatusKindMask DDS_StatusCondition_get_enabled_statuses(
    const DDS_StatusCondition* conditon);

/**
 * @fn  DDS_Entity* DDS_StatusCondition_get_entity(DDS_StatusCondition* condition);
 *
 * @ingroup CInfrastruct
 *
 * @brief   获取状态条件所属的实体。
 *
 * @param [in,out]  condition   指明目标。
 *
 * @return  NULL表示失败，否则指向关联的实体的父指针，用户通过类型转换为具体的实体类型。
 */

DCPSDLL DDS_Entity* DDS_StatusCondition_get_entity(
    DDS_StatusCondition* condition);

#ifdef _ZRDDS_INCLUDE_GUARD_CONDITION

/**
 * @fn  DCPSDLL DDS_GuardCondition* DDS_GuardCondition_new();
 *
 * @ingroup CInfrastruct
 *
 * @brief   默认构造处于非触发状态的监视条件。
 *
 * @return  非NULL表示新的监视条件，NULL表示失败，失败的原因可能为分配内存失败。
 */

DCPSDLL DDS_GuardCondition* DDS_GuardCondition_new();

/**
 * @fn  void DDS_GuardCondition_delete(DDS_GuardCondition* condition);
 *
 * @ingroup CInfrastruct
 *
 * @brief   删除指定监视条件。
 *
 * @param [in,out]  condition   指明目标。
 */

DCPSDLL void DDS_GuardCondition_delete(
    DDS_GuardCondition* condition);

/**
 * @fn  DDS_Boolean DDS_GuardCondition_get_trigger_value(const DDS_GuardCondition* conditon);
 *
 * @ingroup CInfrastruct
 *
 * @brief   获取监视条件当前的触发状态。
 *
 * @param   conditon    指明目标。
 *
 * @return  true表示被触发，false表示该条件还未被触发。
 */

DCPSDLL DDS_Boolean DDS_GuardCondition_get_trigger_value(
    const DDS_GuardCondition* conditon);

/**
 * @fn  DDS_ReturnCode_t DDS_GuardCondition_set_trigger_value(DDS_GuardCondition* conditon, DDS_Boolean value);
 *
 * @ingroup CInfrastruct
 *
 * @brief   手动设置监控类的触发状态。
 *
 * @param [in,out]  conditon    指明目标。
 * @param   value               true表示设置为触发状态，false表示设置为非触发状态。
 *
 * @return  总是返回 #DDS_RETCODE_OK 。
 */

DCPSDLL DDS_ReturnCode_t DDS_GuardCondition_set_trigger_value(
    DDS_GuardCondition* conditon, 
    DDS_Boolean value);

#endif /* _ZRDDS_INCLUDE_GUARD_CONDITION */

#ifdef _ZRDDS_INCLUDE_READ_CONDITION

/**
 * @fn  DCPSDLL DDS_Boolean DDS_ReadCondition_get_trigger_value(const DDS_ReadCondition* conditon);
 *
 * @ingroup CSubscription
 *
 * @brief   获取读取条件当前的触发状态。
 *
 * @param   conditon    指明目标。
 *
 * @return  true表示被触发，false表示该条件还未被触发。
 */

DCPSDLL DDS_Boolean DDS_ReadCondition_get_trigger_value(
    const DDS_ReadCondition* conditon);

/**
 * @fn  DCPSDLL DDS_DataReader* DDS_ReadCondition_get_datareader(const DDS_ReadCondition* condition);
 *
 * @ingroup CSubscription
 *
 * @brief   获取指定读取条件所属的数据读者。
 *
 * @param   condition   指明目标。
 *
 * @return  指向创建该读取条件的数据读者对象。
 */

DCPSDLL DDS_DataReader* DDS_ReadCondition_get_datareader(
    DDS_ReadCondition* condition);

/**
 * @fn  DCPSDLL DDS_SampleStateMask DDS_ReadCondition_get_sample_state_mask(const DDS_ReadCondition* condition);
 *
 * @ingroup CSubscription
 *
 * @brief   获取创建时传入的样本状态掩码。
 *
 * @param   condition   指明目标。
 *
 * @return  该读取条件样本状态掩码。
 */

DCPSDLL DDS_SampleStateMask DDS_ReadCondition_get_sample_state_mask(
    const DDS_ReadCondition* condition);

/**
 * @fn  DCPSDLL DDS_ViewStateMask DDS_ReadCondition_get_view_state_mask(const DDS_ReadCondition* condition);
 *
 * @ingroup CSubscription
 *
 * @brief   获取创建时传入的视图状态掩码。
 *
 * @param   condition   指明目标。
 *
 * @return  该读取条件视图状态掩码。
 */

DCPSDLL DDS_ViewStateMask DDS_ReadCondition_get_view_state_mask(
    const DDS_ReadCondition* condition);

/**
 * @fn  DCPSDLL DDS_InstanceStateMask DDS_ReadCondition_get_instance_state_mask(const DDS_ReadCondition* condition);
 *
 * @ingroup CSubscription
 *
 * @brief   获取创建时传入的实例状态掩码。
 *
 * @param   condition   指明目标。
 *
 * @return  该读取条件实例状态掩码。
 */

DCPSDLL DDS_InstanceStateMask DDS_ReadCondition_get_instance_state_mask(
    const DDS_ReadCondition* condition);

#endif /* _ZRDDS_INCLUDE_READ_CONDITION */

/**
 * @typedef Condition* ConditionPtr
 *
 * @ingroup CInfrastruct
 *
 * @brief   定义 DDS_Condition 指针的别名。
 */

typedef DDS_Condition* ConditionPtr;

/**
 * @struct DDS_ConditionSeq 
 *
 * @ingroup CInfrastruct
 *
 * @brief   声明 ConditionPtr 的序列类型，参见 #DDS_USER_SEQUENCE_C 。
 */

DDS_SEQUENCE_C(DDS_ConditionSeq, ConditionPtr);

#ifdef __cplusplus
}
#endif

#endif /* Condition_h__*/
