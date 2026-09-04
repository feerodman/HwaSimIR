/**
 * @file:       Entity.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef Entity_h__
#define Entity_h__

#include "InstanceHandle_t.h"
#include "Condition.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @fn  DCPSDLL DDS_InstanceHandle_t DDS_Entity_get_instance_handle(const DDS_Entity* self);
 *
 * @ingroup CInfrastruct
 *
 * @brief   获取该实体的唯一标识。
 *
 * @param [in,out]  self        指向目标。
 *
 * @return  实体唯一标识。
 */

DCPSDLL DDS_InstanceHandle_t DDS_Entity_get_instance_handle(
    const DDS_Entity* self);

/**
 * @fn  DCPSDLL DDS_StatusCondition* DDS_Entity_get_statuscondition(DDS_Entity* self);
 *
 * @ingroup CInfrastruct
 *
 * @brief   返回该实体关联的状态条件。
 *
 * @param [in,out]  self        指向目标。
 *
 * @return  底层自动创建的状态条件。
 */

DCPSDLL DDS_StatusCondition* DDS_Entity_get_statuscondition(
    DDS_Entity* self);

/**
 * @fn  DCPSDLL DDS_UShort DDS_Entity_get_status_changes(DDS_Entity* self);
 *
 * @ingroup CInfrastruct
 *
 * @brief   获取该实体从上一次获取任意状态后的状态变化。
 *
 * @param [in,out]  self        指向目标。
 *
 * @return  返回状态的改变掩码。
 */

DCPSDLL DDS_UShort DDS_Entity_get_status_changes(
    DDS_Entity* self);

#ifdef __cplusplus
}
#endif

#endif /* Entity_h__*/
