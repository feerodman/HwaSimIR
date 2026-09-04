/**
 * @file:       ZRDDSCWrapper.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef ZRDDSCWrapper_h__
#define ZRDDSCWrapper_h__

/**
 * @struct DDS_Condition
 *
 * @ingroup CInfrastruct
 *
 * @brief   ZRDDS中条件的“基类”。
 *
 * @details ZRDDS中提供条件-等待模型使得用户可以使用同步等待的模式获取ZRDDS底层的数据，@ref waitset-introduction 该类作为所有条件的基类。
 */

typedef struct ConditionImpl DDS_Condition;

/**
 * @struct DDS_StatusCondition
 *
 * @ingroup CInfrastruct
 *
 * @brief   实体状态条件，该条件用于获取实体状态改变。
 *
 * @details 该条件用于等待实体中简单通信状态变化，由ZRDDS在用户创建实体时，自动创建与实体关联的该状态，用户通过接口
 *          #DDS_Entity_get_statuscondition 方法获取底层引用。
 */

typedef struct StatusConditionImpl DDS_StatusCondition;

/**
 * @struct DDS_GuardCondition
 *
 * @ingroup CInfrastruct
 *
 * @brief   监视条件用于手动控制等待条件。
 *
 * @details 该类主要用于手动控制等待条件 #DDS_WaitSet_wait 方法的阻塞，监视条件的生命周期完全由用户控制。
 */

typedef struct GuardConditionImpl DDS_GuardCondition;

/**
 * @struct DDS_ReadCondition
 *
 * @ingroup CSubscription
 *
 * @brief   该类型用于表示ZRDDS中的读取条件@ref waitset-introduction 。
 *
 * @details ZRDDS中数据读者为每个存储的数据样本均维护三个状态：
 *          - #DDS_SampleStateKind ;
 *          - #DDS_ViewStateKind ;
 *          - #DDS_InstanceStateKind ;
 *
 *          满足读取条件 DDS_ReadCondition(sampleMask, viewMask, instanceMask)的数据样本，将同时满足以下三个条件：
 *          - #DDS_SampleStateKind 处于 sampleMask 所表示的状态集合中；
 *          - 且 #DDS_ViewStateKind 处于 viewMask 所表示的状态集合中；
 *          - 且 #DDS_InstanceStateKind 处于 instanceMask 所表示的状态集合中；
 *          读取条件用于同时表示这三种状态，读取条件主要在两个地方使用：
 *          1. 用于条件-等待模型@ref waitset-introduction 中，当数据读者中处于 DDS_ReadCondition 所指定状态的数据
 *              样本集合不为空时，该条件被触发；
 *          2. 用于 @ref read-take 系列方法，代替 sample_mask、view_mask、instance_mask，
 *              参数，用于读取数据读者中处于 DDS_ReadCondition 所指定状态的数据样本集合。
 */

typedef struct ReadConditionImpl DDS_ReadCondition;

/**
 * @struct DDS_QueryCondition
 *
 * @ingroup CSubscription
 *
 * @brief   查询条件。
 *
 * @warning ZRDDS当前未实现该功能。
 */

typedef struct QueryConditionImpl DDS_QueryCondition;

/**
 * @struct DDS_WaitSet
 *
 * @ingroup CInfrastructure
 *
 * @brief   该类型表示条件-等待中的等待集合 @ref waitset-introduction 。
 */

typedef struct WaitSetImpl DDS_WaitSet;

/**
 * @struct DDS_Entity
 *
 * @ingroup CInfrastruct
 *
 * @brief   该类型用于表示所有实体（@ref entity-introduction) 包括域参与者、主题、发布者、订阅者、数据读者、数据写者的“基类”。
 */

typedef struct EntityImpl DDS_Entity;

/**
 * @struct DDS_DataReader
 *
 * @ingroup CSubscription
 *
 * @brief   表示ZRDDS中的数据读者。
 *
 * @details 数据读者主要负责存储从发布端获取到的数据以及提供接口给上层应用获取接收到的数据，
 *          ZRDDS提供强类型安全接口的数据读者接口，详细的接口说明参见 ::FooDataReader 。
 */

typedef struct DataReaderImpl DDS_DataReader;

/**
 * @struct DDS_DataWriter
 *
 * @ingroup CPublication
 *
 * @brief   表示ZRDDS中的数据写者。
 *
 * @details 数据写者主要负责发布数据，ZRDDS提供强类型安全接口的数据读者接口，详细的接口说明参见 ::FooDataWriter 。
 */

typedef struct DataWriterImpl DDS_DataWriter;

/**
 * @struct DDS_TopicDescription
 *
 * @ingroup	CTopic
 *
 * @brief	抽象主题的基本属性信息。
 *
 * @details	与主题相关的信息包括关联的一个已存在的数据类型（由数据类型名和 DDS_TypeSupport 类描述），
 * 			主题名称以及与该主题关联的 DDS_DomainParticipant 。这些描述信息包含于该类的对象中并可通过该类的对象获取。
 */

typedef struct TopicDescriptionImpl DDS_TopicDescription;

/**
 * @struct DDS_Topic
 *
 * @ingroup CTopic
 *
 * @brief   主题是发布订阅模型中数据基本的抽象。
 *
 * @details 主题由主题名标识，主题名在域内唯一，并且通过关联的类型名称定义主题下数据的数据结构。
 *          数据写者与数据读者通过关联主题进行通信。
 */

typedef struct TopicImpl DDS_Topic;

/**
 * @struct DDS_ContentFilteredTopic
 *
 * @ingroup  CppTopic
 *
 * @brief   抽象基于内容过滤的主题。
 *
 * @details  基于内容的过滤主题是更复杂的主题抽象，表明订阅端不需要主题下的所有数据，而只关心满足一定条件的数据。
 *           基于内容过滤的主题可以用来限制订阅端需要处理的数据量，以及减少网络内发送的数据量（发布端过滤）。
 *           通过过滤表达式以及过滤参数来表示订阅端需要的数据。过滤表达式与SQL语句中的WHERE块类似，
 *           而过滤参数则提供值给过滤表达式中的参数（使用%n表示），提供的过滤参数的个数需与过滤表达式中要求的参数一致。
 *           过滤表达式以及过滤参数的语法参见 @ref expression-grammer 。
 *
 *           注意： ZRDDS该版本的过滤在订阅端应用，故而基于内容过滤的主题不会减少网络带宽，反而会增加接收端的负载（每个样本
 *           均需要应用用户设置的过滤规则）。
 */

typedef struct ContentFilteredTopicImpl DDS_ContentFilteredTopic;

/**
 * @struct DDS_Subscriber
 *
 * @ingroup  CSubscription
 *
 * @brief   ZRDDS提供的订阅者接口，应用创建订阅者表示自身想从指定的域内获取数据；
 *
 * @details 订阅者主要完成以下几个功能：
 *          - 实体功能；
 *          - 作为数据读者的工厂；
 *          - 统一处理数据读者的数据样本通知；
 */

typedef struct SubscriberImpl DDS_Subscriber;

/**
 * @struct DDS_Publisher
 *
 * @ingroup  CPublication
 *
 * @brief   ZRDDS提供的发布者接口，应用创建发布者表示自身想向指定的域内提供数据；
 *
 * @details 发布者主要完成以下几个功能：
 *          - 实体功能；
 *          - 作为数据写者的工厂；
 *          - 统一处理数据写者的数据样本发送；
 */

typedef struct PublisherImpl DDS_Publisher;

/**
 * @struct DDS_DomainParticipant
 *
 * @ingroup CDomain
 *
 * @brief   该类抽象域参与者实体。
 *
 * @details 域参与者主要提供以下接口功能：
 *          - 实体功能；
 *          - 作为其他实体工厂；
 *              - 作为发布者实体工厂；
 *              - 作为订阅者实体工厂；
 *              - 作为主题实体工厂；
 *          - 内置实体管理；
 *          - 通信管理；
 *          - 域信息查询；
 *          - 存活性管理；
 */

typedef struct DomainParticipantImpl DDS_DomainParticipant;

/**
 * @struct DDS_DomainParticipantFactory
 *
 * @ingroup CDomain
 *
 * @brief   域参与者工厂接口的核心功能在于是创建以及销毁域参与者实体，即作为域参与者实体的工厂。
 */

typedef struct DomainParticipantFactoryImpl DDS_DomainParticipantFactory;

typedef struct ZRDynamicDataDataReader ZRDynamicDataDataReader;
typedef struct ZRDynamicDataTypeSupportImpl ZRDynamicDataTypeSupport;

typedef struct TypeCodeHeader TypeCode;
typedef struct TypeCodeFactoryImpl TypeCodeFactory;

typedef struct DataWriterListenerImpl DataWriterListenerImpl;
typedef struct DataWriterImpl DataWriterImpl;
typedef struct DataReaderImpl DataReaderImpl;

#endif /* ZRDDSCWrapper_h__ */
