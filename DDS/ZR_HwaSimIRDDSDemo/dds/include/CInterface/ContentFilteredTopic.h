/**
 * @file:       ContentFilteredTopic.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef ContentFilteredTopic_h__
#define ContentFilteredTopic_h__

#include "OsResource.h"
#include "Topic.h"

#ifdef _ZRDDS_INCLUDE_CONTENTFILTER_TOPIC

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @fn  DDS_Topic* DDS_ContentFilterTopic_get_related_topic(DDS_ContentFilteredTopic* self);
 *
 * @ingroup  CTopic
 *
 * @brief   获取该基于内容过滤主题的基础主题，在使用 DDS_DomainParticipant_create_contentfilertopic 创建时指定。
 *
 * @param [in,out]  self    指明目标。
 *
 * @return  指向该基于内容过滤的主题关联的基础主题。
 */

DCPSDLL DDS_Topic* DDS_ContentFilterTopic_get_related_topic(
    DDS_ContentFilteredTopic* self);

/**
 * @fn  DDS_ReturnCode_t DDS_ContentFilteredTopic_get_expression_paramters(DDS_ContentFilteredTopic* self, DDS_StringSeq* para);
 *
 * @ingroup  CTopic
 *
 * @brief   该方法获取关联的过滤参数，在创建基于内容过滤的主题时传入或者通过 #DDS_ContentFilteredTopic_set_expression_paramters 方法设置。
 *
 * @param [in,out]  self    指明目标。
 * @param [in,out]  para    出口参数，用于保存通信中间件维护的过滤参数。
 *
 * @return  如下的可能返回值：
 *          - #DDS_RETCODE_OK :表示成功；
 *          - #DDS_RETCODE_ERROR :未知的错误表示失败，例如拷贝参数失败。
 */

DCPSDLL DDS_ReturnCode_t DDS_ContentFilteredTopic_get_expression_paramters(
    DDS_ContentFilteredTopic* self, 
    DDS_StringSeq* para);

/**
 * @fn  DDS_ReturnCode_t DDS_ContentFilteredTopic_set_expression_paramters(DDS_ContentFilteredTopic* self, const DDS_StringSeq* para);
 *
 * @ingroup  CTopic
 *
 * @brief   该方法用于重新设置过滤参数。
 *
 * @param [in,out]  self    指明目标。
 * @param   para            指明新的过滤参数。
 *
 * @return  如下的可能返回值：
 *          - #DDS_RETCODE_OK :表示成功；
 *          - #DDS_RETCODE_BAD_PARAMETER :表示过滤参数个数或者类型与过滤表达式不匹配；
 *          - #DDS_RETCODE_ERROR :未知的错误表示失败，例如拷贝参数失败。
 */

DCPSDLL DDS_ReturnCode_t DDS_ContentFilteredTopic_set_expression_paramters(
    DDS_ContentFilteredTopic* self, 
    const DDS_StringSeq* para);

/**
 * @fn  const DDS_Char* DDS_ContentFilteredTopic_get_filter_expression(DDS_ContentFilteredTopic* self);
 *
 * @ingroup  CTopic
 *
 * @brief   获取当前基于内容过滤的主题关联的过滤表达式。
 *
 * @param [in,out]  self    指明目标。
 *
 * @return  过滤表达式字符串。
 */

DCPSDLL const DDS_Char* DDS_ContentFilteredTopic_get_filter_expression(
    DDS_ContentFilteredTopic* self);

#ifdef __cplusplus
}
#endif

#endif /* _ZRDDS_INCLUDE_CONTENTFILTER_TOPIC */

#endif /* ContentFilteredTopic_h__*/
