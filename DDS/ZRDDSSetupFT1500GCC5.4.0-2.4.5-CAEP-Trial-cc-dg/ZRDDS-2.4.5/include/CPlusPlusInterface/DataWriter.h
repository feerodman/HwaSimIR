/**
 * @file:       DataWriter.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef DataWriter_h__
#define DataWriter_h__

#include "DomainEntity.h"
#include "Topic.h"
#include "DataWriterListener.h"
#include "ZRDDSCppWrapper.h"

struct DataWriterImpl;
struct DataWriterListenerImpl;
struct ZRDynamicData;

namespace DDS {

class Publisher;

/**
 * @class DataWriter
 *
 * @ingroup CppPublication
 *
 * @brief DDS标准中的数据读者接口。
 *
 * @details 数据读写主要负责存储为用户提供发布数据的功能，数据读写接口包含两类接口：
 *          - 用户数据类型无关接口，该类即定义了这些与用户类型无关的接口，作为用户类型的数据写者的父类。
 *          - 用户数据类型相关接口，通过类模板实现， DDS::ZRDDSDataWriter
 */
class DCPSDLL DataWriter : public DomainEntity
{
public:
#ifdef _ZRXMLINTERFACE

#ifdef _ZRXMLENTITYINTERFACE

    /**
     * @fn virtual ReturnCode_t DataWriter::parse_write_sample_info_xml( const Char* xml_content, SampleInfo* sample_info, SampleInfoValidMember* valid_sample_info_member);
     *
     * @brief 解析以XML表示的SampleInfo，XML格式为write_sample_info
     *
     * @param xml_content                       以XML表示的SampleInfo，XML格式为write_sample_info
     * @param [in,out] sample_info              传入的SampleInfo结构，解析的结果会被保存于该结构中
     * @param [in,out] valid_sample_info_member 传入的SampleInfoValidMember结构，指明解析出的sample_info参数中哪些成员有效
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示解析成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的解析错误。
     */
    virtual ReturnCode_t parse_write_sample_info_xml(
        const Char* xml_content,
        SampleInfo* sample_info,
        SampleInfoValidMember* valid_sample_info_member);

    /**
     * @fn virtual ReturnCode_t DataWriter::parse_write_sample_xml( const Char* xml_content, ZRDynamicData*& data);
     *
     * @brief 解析以XML表示的数据样本并生成DynamicData，XML格式为data
     *
     * @param xml_content   以XML表示的数据样本，数据样本与创建该Writer的Topic关联的类型必须匹配
     * @param [in,out] data [in,out] 从XML创建的DynamicData
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示解析成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的解析错误。
     */
    virtual ReturnCode_t parse_write_sample_xml(
        const Char* xml_content,
        ZRDynamicData*& data);

    /**
     * @fn virtual ReturnCode_t DataWriter::return_xml_sample( ZRDynamicData* data);
     *
     * @brief 销毁从 #DataWriter::return_xml_sample 获取到的数据样本。
     *
     * @param [in,out] data 待销毁的数据样本。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示销毁成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的销毁错误。
     */
    virtual ReturnCode_t return_xml_sample(
        ZRDynamicData* data);

    /**
     * @fn virtual ReturnCode_t DataWriter::to_xml(const Char*& result, Boolean containedQos);
     *
     * @brief 将Writer转换到XML，XML根节点为data_writer
     *
     * @param [in,out] result 生成的XML
     * @param containedQos    生成是否包含QoS
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示转换成功；
     *         - #DDS_RETCODE_OUT_OF_RESOURCES :获取资源失败，一般为内存不足导致；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的转换错误。
     */
    virtual ReturnCode_t to_xml(const Char*& result, Boolean containedQos = true);

    /**
     * @fn virtual const Char* DataWriter::get_entity_name();
     *
     * @brief 获取Entity的名称
     *
     * @return 数据写者名称。
     */
    virtual const Char* get_entity_name();

    /**
     * @fn virtual Entity* DataWriter::get_factory();
     *
     * @brief 获取工厂指针，即发布者指针
     *
     * @return 获取到的工厂指针。
     */
    virtual Entity* get_factory();

#endif // _ZRXMLENTITYINTERFACE

#ifdef _ZRXMLQOSINTERFACE

    /**
     * @fn virtual ReturnCode_t DataWriter::set_qos_with_profile( const Char* library_name, const Char* profile_name, const Char* qos_name);
     *
     * @brief 从QoS仓库获取QoS配置并设置到数据写者。
     *
     * @param library_name QoS库的名字，不允许为NULL。
     * @param profile_name QoS配置的名字，不允许为NULL。
     * @param qos_name     QoS的名字，允许为NULL，将转换为default字符串。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示设置成功；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的设置错误。
     */
    virtual ReturnCode_t set_qos_with_profile(
        const Char* library_name,
        const Char* profile_name,
        const Char* qos_name);

#endif // _ZRXMLQOSINTERFACE

#endif // _ZRXMLINTERFACE

    DataWriter(DataWriter *dw);

#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
    /**
     * @fn  virtual ReturnCode_t DataWriter::get_liveliness_lost_status( LivelinessLostStatus &status);
     *
     * @brief   获取数据写者关联的 #DDS_LIVELINESS_LOST_STATUS 状态。
     *
     * @param [in,out]  status  获取到的LIVELINESS_LOST的通信状态。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示获取成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的读取错误。
     */

    virtual ReturnCode_t get_liveliness_lost_status(
        LivelinessLostStatus &status);

    /**
     * @fn  virtual ReturnCode_t DataWriter::assert_liveliness();
     *
     * @brief   声明自身的存活性
     *
     * @return  当前可能的返回值如下：
     *          - #DDS_RETCODE_OK :表示声明成功；
     *          - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *          - #DDS_RETCODE_NOT_ENABLED :数据写者未使能；
     *          - #DDS_RETCODE_ERROR :表示未知错误导致的声明错误。
     */

    virtual ReturnCode_t assert_liveliness();
#endif //_ZRDDS_INCLUDE_LIVELINESS_QOS

#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
    /**
     * @fn  virtual ReturnCode_t DataWriter::get_offered_deadline_missed_status( OfferedDeadlineMissedStatus &status);
     *
     * @brief   获取数据写者关联的 #DDS_OFFERED_DEADLINE_MISSED_STATUS 状态。
     *
     * @param [in,out]  status  获取到的OFFERED_DEADLINE_MISSED状态。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示获取成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的读取错误。
     */

    virtual ReturnCode_t get_offered_deadline_missed_status(
        OfferedDeadlineMissedStatus &status);

#endif //_ZRDDS_INCLUDE_DEADLINE_QOS

    /**
     * @fn  virtual ReturnCode_t DataWriter::get_offered_incompatible_qos_status( OfferedIncompatibleQosStatus &status);
     *
     * @brief   获取数据写者关联的 #DDS_OFFERED_INCOMPATIBLE_QOS_STATUS 状态。
     *
     * @param [in,out]  status  获取到的INCOMPATIBLE_QOS状态。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示获取成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的读取错误。
     */

    virtual ReturnCode_t get_offered_incompatible_qos_status(
        OfferedIncompatibleQosStatus &status);

    /**
     * @fn  virtual ReturnCode_t DataWriter::get_publication_matched_status( PublicationMatchedStatus &status);
     *
     * @brief   获取数据写者关联的 #DDS_PUBLICATION_MATCHED_STATUS 状态。
     *
     * @param [in,out]  status  获取到的数据写者匹配状态。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示获取成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的读取错误。
     */

    virtual ReturnCode_t get_publication_matched_status(
        PublicationMatchedStatus &status);

    /**
     * @fn  virtual ReturnCode_t DataWriter::get_matched_subscriptions( InstanceHandleSeq &handles);
     *
     * @brief   获取已匹配且没有被忽略的订阅者句柄列表。
     *
     * @param [in,out]  handles 获取到的已匹配且没有被忽略的订阅者句柄列表。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示获取成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_NOT_ENABLED :数据写者未使能；
     *         - #DDS_RETCODE_OUT_OF_RESOURCES :获取资源失败，一般为内存不足导致；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的读取错误。
     */

    virtual ReturnCode_t get_matched_subscriptions(
        InstanceHandleSeq &handles);

    /**
     * @fn  virtual ReturnCode_t DataWriter::get_matched_subscription_data( SubscriptionBuiltinTopicData &subscription_data, const InstanceHandle_t& a_handle);
     *
     * @brief   获取指定的已匹配订阅者的详细信息。
     *
     * @param [in,out]  subscription_data   保存订阅者详细信息的结构.
     * @param   a_handle                    从 #get_matched_subscriptions 获取到的订阅者句柄。
     *
     * @return  当前可能的返回值如下：
     *          - #DDS_RETCODE_OK :表示获取成功；
     *          - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *          - #DDS_RETCODE_NOT_ENABLED :数据写者未使能；
     *          - #DDS_RETCODE_ERROR :表示未知错误导致的读取错误。
     */

    virtual ReturnCode_t get_matched_subscription_data(
        SubscriptionBuiltinTopicData &subscription_data,
        const InstanceHandle_t& a_handle);

    /**
     * @fn  virtual ReturnCode_t DataWriter::wait_for_acknowledgments(const Duration_t &max_wait);
     *
     * @brief   对可靠的数据写者调用此函数将导致调用者的阻塞，直到所有已发出的数据被应答或者等待超时。
     *
     * @param   max_wait    最长等待时间。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示所有数据样本都已经被反馈成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_NOT_ENABLED :数据写者未使能；
     *         - #DDS_RETCODE_TIMEOUT :表示等待超时；
     *         - #DDS_RETCODE_ERROR :表示未知错误导致的等待错误。
     */

    virtual ReturnCode_t wait_for_acknowledgments(const Duration_t &max_wait);

    /**
     * @fn  virtual Topic* DataWriter::get_topic();
     *
     * @brief   获取该数据写者所关联的主题。
     *
     * @return  返回数据写者关联的主题。
     */

    virtual Topic* get_topic();

    /**
     * @fn  virtual Publisher* DataWriter::get_publisher();
     *
     * @brief   获取创建该DataWriter的Publisher。
     *
     * @return  创建该DataWriter的Publisher。
     */

    virtual Publisher* get_publisher();

    /**
     * @fn  virtual ReturnCode_t DataWriter::set_qos(const DataWriterQos &qoslist);
     *
     * @brief   设置DataWriterQoS。
     *
     * @details QoS影响DataWriter的行为。部分QoS只能在DataWriter使能之前修改。
     *          用户必须保证QoS的正确性，如果存在冲突，设置将会失败。
     *          具体的兼容列表见各QoS文档。
     *
     * @param   qoslist 配置完成需要置入的QoS。
     *
     * @return  可能的返回值如下：
     *          - #DDS_RETCODE_OK 设置成功。
     *          - #DDS_RETCODE_IMMUTABLE_POLICY 不可在使能后修改的QoS。
     *          - #DDS_RETCODE_INCONSISTENT QoS存在冲突。
     *          - #DDS_RETCODE_BAD_PARAMETER 参数存在问题。
     *          - #DDS_RETCODE_ERROR 未归类错误。
     */

    virtual ReturnCode_t set_qos(const DataWriterQos &qoslist);

    /**
     * @fn  virtual ReturnCode_t DataWriter::get_qos(DataWriterQos &qoslist);
     *
     * @brief   获取当前数据写者的QoS。
     *
     * @param [in,out]  qoslist 保存获取到的QoS。
     *
     * @return  可能的返回值如下：
     *          - #DDS_RETCODE_OK 获取成功。
     *          - #DDS_RETCODE_BAD_PARAMETER 参数存在问题。
     *          - #DDS_RETCODE_ERROR 未归类错误。
     */

    virtual ReturnCode_t get_qos(DataWriterQos &qoslist);

    /**
     * @fn  virtual ReturnCode_t DataWriter::set_listener( DataWriterListener* a_listener, const StatusKindMask &mask);
     *
     * @brief   设置数据写者监听器。
     *
     * @param   a_listener  用户提供的监听器对象，可以传入NULL解除监听。
     * @param   mask        状态掩码，指明需要被监听器捕获的状态。
     *
     * @return  可能的返回值如下：
     *          - #DDS_RETCODE_OK 设置成功。
     */

    virtual ReturnCode_t set_listener(
        DataWriterListener* a_listener,
        const StatusKindMask &mask);

    /**
     * @fn  virtual DataWriterListener* DataWriter::get_listener();
     *
     * @brief   获取当前数据写者的监听器。
     *
     * @return  可能的返回值如下：
     *          - NULL表示未设置监听器；
     *          - 非空表示应用通过 #set_listener 或者在创建时设置的监听器对象。
     */

    virtual DataWriterListener* get_listener();

    /**
     * @fn  StatusCondition* DataWriter::get_statuscondition();
     *
     * @brief   实现父类 DDS::Entity 的方法，返回该数据写者关联的状态条件。
     *
     * @return  底层自动创建的状态条件。
     */

    StatusCondition* get_statuscondition();

    /**
     * @fn   virtual StatusKindMask DataWriter::get_status_changes();
     *
     * @brief    实现父类 DDS::Entity 的方法，获取该实体从上一次获取任意状态后的状态变化。
     *
     * @return   返回状态的改变掩码。
     */

    virtual StatusKindMask get_status_changes();

    /**
     * @fn  virtual ReturnCode_t DataWriter::enable();
     *
     * @brief   手动使能该数据写者。
     *
     * @return  可能的返回值如下：
     *          - #DDS_RETCODE_OK 使能成功。
     *          - #DDS_RETCODE_PRECONDITION_NOT_MET 数据写者所属的发布者还未使能。
     *          - #DDS_RETCODE_ERROR 未归类错误。
     */

    virtual ReturnCode_t enable();

    /**
     * @fn virtual InstanceHandle_t DataWriter::get_instance_handle();
     *
     * @brief 实现父类 DDS::Entity 的方法，获取该数据写者的唯一标识。
     *
     * @return 数据写者的唯一标识。
     */
    virtual InstanceHandle_t get_instance_handle();

    /**
     * @fn  virtual ReturnCode_t DataWriter::get_send_status(PublicationSendStatusSeq &status);
     *
     * @brief   获取与已匹配且没有被忽略的数据读者之间通信状态列表。
     *
     * @param [in,out]  status 获取到的与已匹配且没有被忽略的数据读者之间通信状态列表。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示获取成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_NOT_ENABLED :数据写者未使能；
     *         - #DDS_RETCODE_OUT_OF_RESOURCES :获取资源失败，一般为内存不足导致；
     */

    virtual ReturnCode_t get_send_status(PublicationSendStatusSeq &status);

    /**
     * @fn  virtual ReturnCode_t DataWriter::print_send_status(PublicationSendStatusSeq &status);
     *
     * @brief   打印与数据读者之间通信状态列表。
     *
     * @param   status 通过 #get_send_status 获取到的与已匹配且没有被忽略的数据读者之间通信状态列表。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示获取成功；
     */

    virtual ReturnCode_t print_send_status(PublicationSendStatusSeq &status);

    /**
     * @fn  virtual ReturnCode_t DataWriter::get_send_status_w_handle(PublicationSendStatus &status, const InstanceHandle_t* dstGuid);
     *
     * @brief   获取与指定的数据读者之间通信状态。
     *
     * @param [in,out]  status 获取到的与指定的数据读者之间通信状态。
     * @param   dstGuid 指定的数据读者的唯一标识。
     *
     * @return 当前可能的返回值如下：
     *         - #DDS_RETCODE_OK :表示获取成功；
     *         - #DDS_RETCODE_BAD_PARAMETER :参数中存在错误；
     *         - #DDS_RETCODE_NOT_ENABLED :数据写者未使能；
     *         - #DDS_RETCODE_OUT_OF_RESOURCES :获取资源失败，一般为内存不足导致；
     */

    virtual ReturnCode_t get_send_status_w_handle(PublicationSendStatus &status, const InstanceHandle_t* dstGuid);

#ifdef _ZRDDS_INCLUDE_BATCH
    /**
     * @fn virtual ReturnCode_t DataWriter::flush();
     *
     * @brief 手动刷新批量发送，会立即将当前保存的所有待批量发送的样本打包发布。
     *
     * @return  可能的返回值如下：
     *          - #DDS_RETCODE_OK 使能成功。
     *          - #DDS_RETCODE_BAD_PARAMETER 参数存在错误。
     *          - #DDS_RETCODE_NOT_ENABLED 数据写者未使能。
     *          - #DDS_RETCODE_ERROR 未归类错误，具体内容记录在日志文件中。
     */
    virtual ReturnCode_t flush();
#endif // _ZRDDS_INCLUDE_BATCH

    DataWriter* get_impl()
    {
        return m_impl;
    }
    virtual InstanceHandle_t register_instance_untype(const void *instance, const Time_t* timestamp);
    virtual ReturnCode_t unregister_instance_untype(const void *instance, const InstanceHandle_t& handle, const Time_t* timestamp);
    virtual ReturnCode_t get_key_value_untype(void *key_holder, const InstanceHandle_t& handle);
    virtual InstanceHandle_t lookup_instance_untype(const void *key_holder);
    virtual ReturnCode_t write_untype_w_dst(const void *data, const InstanceHandle_t& handle, const Time_t* timestamp, const InstanceHandle_t* dstGuid);
    virtual ReturnCode_t dispose_untype(const void *instance, const InstanceHandle_t& handle, const Time_t* timestamp);

protected:
    virtual ~DataWriter() {}

    DataWriter *m_impl;
};

} // namespace DDS

#endif // DataWriter_h__
