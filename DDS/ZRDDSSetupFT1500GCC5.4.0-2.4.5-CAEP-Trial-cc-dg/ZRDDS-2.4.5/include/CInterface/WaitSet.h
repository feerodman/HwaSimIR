/**
 * @file:       WaitSet.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef WaitSet_h__
#define WaitSet_h__

#include "ZRDDSCommon.h"
#include "ReturnCode_t.h"
#include "Condition.h"
#include "Duration_t.h"

#ifdef _ZRDDS_INCLUDE_WAITSET

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @fn  DCPSDLL DDS_WaitSet* DDS_WaitSet_new();
 *
 * @ingroup CInfrastruct
 *          
 * @brief   创建一个新的空的等待集合。
 *
 * @return  NULL表示失败，非NULL的表示创建的等待集合。
 */

DCPSDLL DDS_WaitSet* DDS_WaitSet_new();

/**
 * @fn  DCPSDLL void DDS_WaitSet_delete(DDS_WaitSet* waitset);
 *
 * @ingroup CInfrastruct
 *
 * @brief   删除指定的等待集合，将自动解开阻塞。
 *
 * @param [in,out]  waitset        指向目标。
 */

DCPSDLL void DDS_WaitSet_delete(DDS_WaitSet* waitset);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_WaitSet_attach_condition(DDS_WaitSet* self, DDS_Condition* condition);
 *
 * @ingroup CInfrastruct
 *
 * @brief   将指定的条件加入到该等待集合中。
 *
 * @details 如果添加的条件的触发状态为true，将会解开阻塞。
 *
 * @param [in,out]  self        指向目标。
 * @param [in,out]  condition 指明需要添加的条件。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :添加成功；
 *          - #DDS_RETCODE_OUT_OF_RESOURCES :分配内存失败；
 *          - #DDS_RETCODE_BAD_PARAMETER :参数不是有效的条件。
 */

DCPSDLL DDS_ReturnCode_t DDS_WaitSet_attach_condition(
    DDS_WaitSet* self, 
    DDS_Condition* condition);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_WaitSet_detach_condition(DDS_WaitSet* self, DDS_Condition* condition);
 *
 * @ingroup CInfrastruct
 *
 * @brief   从等待集合中移除指定的条件。
 *
 * @param [in,out]  self        指向目标。
 * @param [in,out]  condition 指明需要删除的条件。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :添加成功；
 *          - #DDS_RETCODE_PRECONDITION_NOT_MET :指定的条件不在该等待集合中；
 *          - #DDS_RETCODE_BAD_PARAMETER :参数不是有效的条件。
 */

DCPSDLL DDS_ReturnCode_t DDS_WaitSet_detach_condition(
    DDS_WaitSet* self, 
    DDS_Condition* condition);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_WaitSet_wait(DDS_WaitSet* self, DDS_ConditionSeq* activeConditions, const DDS_Duration_t* timeout);
 *
 * @ingroup CInfrastruct
 *
 * @brief   阻塞等待等待集合中的条件处于触发状态。
 *
 * @details 应用线程等待某些条件的发生，堵塞调用线程， 当调用成功时，@e activeConditions 中存放触发的条件，
 *          不允许多个线程等待同一个等待条件。
 *
 * @param [in,out]  self        指向目标。
 * @param   activeConditions    出口参数，用于存储处于触发状态的条件。
 * @param   timeout             指明最长等待时间。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :出口参数有效，表明处于触发状态的条件；
 *          - #DDS_RETCODE_PRECONDITION_NOT_MET :在多个线程上等待条件；
 *          - #DDS_RETCODE_BAD_PARAMETER / #DDS_RETCODE_ERROR ：用户提供的空间不足，且底层扩容失败；
 *          - #DDS_RETCODE_TIMEOUT : 在指定的时间内没有条件处于触发状态。
 */

DCPSDLL DDS_ReturnCode_t DDS_WaitSet_wait(
    DDS_WaitSet* self, 
    DDS_ConditionSeq* activeConditions, 
    const DDS_Duration_t* timeout);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_WaitSet_get_conditions(DDS_WaitSet* self, DDS_ConditionSeq* attachedConditions);
 *
 * @ingroup CInfrastruct
 *
 * @brief   获取当前该等待集合中已有的条件。
 *
 * @param [in,out]  self        指向目标。
 * @param [in,out]  attachedConditions 出口参数，用于存储当前集合中的条件。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :出口参数有效，表明处于触发状态的条件；
 *          - #DDS_RETCODE_BAD_PARAMETER / #DDS_RETCODE_ERROR ：用户提供的空间不足，且底层扩容失败；
 */

DCPSDLL DDS_ReturnCode_t DDS_WaitSet_get_conditions(
    DDS_WaitSet* self, 
    DDS_ConditionSeq* attachedConditions);

#ifdef __cplusplus
}
#endif

#endif /* _ZRDDS_INCLUDE_WAITSET */

#endif /* WaitSet_h__*/
