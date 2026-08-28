var NAVTREE =
[
  [ "ZRDDS", "index.html", [
    [ "臻融数据分发服务（ZRDDSv2.4.0）在线文档", "index.html", [
      [ "ZRDDS介绍", "index.html#introduction", [
        [ "以数据为中心的订阅发布模型", "index.html#pub-sub-modle", null ],
        [ "实体", "index.html#entity-introduction", null ],
        [ "实体的生命周期", "index.html#entity-lifecycle", [
          [ "实体唯一标识", "index.html#entity-instancehandle", null ],
          [ "实体使能", "index.html#entity-enable", null ],
          [ "实体状态", "index.html#entity-status", null ],
          [ "获取实体状态", "index.html#get-plain-status", null ]
        ] ],
        [ "条件-等待", "index.html#waitset-introduction", null ],
        [ "监听器", "index.html#listener-introduction", null ],
        [ "QoS", "index.html#entity-qos", null ],
        [ "域/域参与者", "index.html#domain-introduction", [
          [ "域参与者工厂", "index.html#dpf-introduction", null ],
          [ "域参与者", "index.html#dp-introduction", null ]
        ] ],
        [ "数据类型", "index.html#data-types", [
          [ "键、实例、样本", "index.html#instance", null ]
        ] ],
        [ "主题", "index.html#topic", null ],
        [ "基于内容过滤的主题", "index.html#cft-topic", [
          [ "语法规则", "index.html#expression-grammer", null ]
        ] ],
        [ "发布数据", "index.html#publication", [
          [ "发布者", "index.html#publisher", null ],
          [ "数据写者", "index.html#datawriter", null ]
        ] ],
        [ "订阅数据", "index.html#subscription", [
          [ "订阅者", "index.html#subscriber", null ],
          [ "数据读者", "index.html#datareader", null ],
          [ "获取主题数据", "index.html#read-take", null ]
        ] ]
      ] ],
      [ "ZRDDS编译器", "index.html#DDSCompiler", [
        [ "语法支持", "index.html#DDSCompiler-Syntax", [
          [ "数组", "index.html#DDSCompiler-Syntax-Array", null ],
          [ "sequence", "index.html#DDSCompiler-Syntax-Sequence", null ],
          [ "string", "index.html#DDSCompiler-Syntax-String", null ],
          [ "struct", "index.html#DDSCompiler-Syntax-struct", null ],
          [ "union", "index.html#DDSCompiler-Syntax-union", null ]
        ] ],
        [ "扩展标记", "index.html#DDSCompiler-Annotions", [
          [ "@key", "index.html#DDSCompiler-Annotions-key", null ],
          [ "@Nested", "index.html#DDSCompiler-Annotions-nest", null ]
        ] ],
        [ "可扩展数据类型", "index.html#DDSCompiler-XTypes", null ],
        [ "zrddsgen用法", "index.html#DDSCompiler-Usage", null ]
      ] ],
      [ "问题反馈", "index.html#sugesstion", null ]
    ] ],
    [ "ZRDDS下载", "downloads.html", null ],
    [ "ZRDDS常见问题", "faq.html", [
      [ "1.技术支持", "faq.html#support_q", [
        [ "1.1.ZRDDS支持哪些操作系统以及编译器？", "faq.html#support_q_1", null ],
        [ "1.2.ZRDDS支持哪些通信协议？", "faq.html#support_q_2", null ],
        [ "1.3.ZRDDS支持哪些工具以及服务？", "faq.html#support_q_3", null ]
      ] ],
      [ "2.IDL问题", "faq.html#IDL_q", [
        [ "2.1.使用ZRDDS一定需要使用IDL文件，并使用编译器生成类型支持文件吗？", "faq.html#IDL_q_1", null ],
        [ "2.2.IDL文件中的字符串类型映射为什么数据结构？", "faq.html#IDL_q_2", null ],
        [ "2.3.IDL文件中的动态数组sequence映射为什么数据结构？", "faq.html#IDL_q_3", null ]
      ] ],
      [ "3.通信问题", "faq.html#transport_q", [
        [ "3.1.在Windows上简单的发布/订阅通信不了通常包含哪些原因？", "faq.html#transport_q_1", null ],
        [ "3.2.在Linux上简单的发布/订阅通信不了通常包含哪些原因？", "faq.html#transport_q_2", null ],
        [ "3.3.为何使用带有变长成员的数据结构，调用 DataWriter::write 方法发送数据时，程序容易发生崩溃？", "faq.html#transport_q_3", null ],
        [ "3.4.如何避免使用ZRDDS时造成内存泄漏", "faq.html#transport_q_4", null ],
        [ "3.5.在接收DDS数据时，为何在访问接收数据时可能会抛出异常（提示dataSeq[0]的内容为空）？", "faq.html#transport_q_5", null ],
        [ "3.6.sequence数据结构在使用时，长度有限制吗？", "faq.html#transport_q_6", null ],
        [ "3.7.DDS在以太网UDP/TCP传输中，最大传输数据长度为多少？", "faq.html#transport_q_7", [
          [ "3.7.1.UDP", "faq.html#transport_q_7_UDP", null ],
          [ "3.7.2.TCP", "faq.html#transport_q_7_TCP", null ]
        ] ],
        [ "3.8.DDS使用零拷贝数据类型和非零拷贝数据类型进行通信的区别", "faq.html#transport_q_8", [
          [ "3.8.1.使用规则", "faq.html#transport_q_8_rules", null ],
          [ "3.8.2.传输速度", "faq.html#transport_q_8_tp", null ]
        ] ]
      ] ]
    ] ],
    [ "ZRDDS版本记录", "releasenotes.html", null ],
    [ "ZRDDS调试日志信息表", "zrdds_log_info.html", null ],
    [ "ZRDDS多种接口风格及开发流程", "zrdds_interface.html", [
      [ "1. 标准协议接口", "zrdds_interface.html#zrdds_interface_std", [
        [ "1.1. 标准协议接口的使用", "zrdds_interface.html#zrdds_interface_std_use", [
          [ "1.1.1. 流程", "zrdds_interface.html#zrdds_interface_std_flow", null ],
          [ "1.1.2. 主要接口介绍", "zrdds_interface.html#zrdds_interface_std_introduce", null ]
        ] ],
        [ "1.2. 标准协议接口代码示例", "zrdds_interface.html#zrdds_interface_std_example", null ]
      ] ],
      [ "2. 扩展接口", "zrdds_interface.html#zrdds_interface_extend", [
        [ "2.1. 扩展接口的使用", "zrdds_interface.html#zrdds_interface_extend_use", [
          [ "2.1.1. 流程", "zrdds_interface.html#zrdds_interface_extend_flow", null ],
          [ "2.1.2. 主要接口介绍", "zrdds_interface.html#zrdds_interface_extend_introduce", null ]
        ] ],
        [ "2.2. 扩展接口代码示例", "zrdds_interface.html#zrdds_interface_extend_example", null ],
        [ "2.3. 扩展内容一览", "zrdds_interface.html#zrdds_interface_extend_list", null ]
      ] ],
      [ "3. 简化接口", "zrdds_interface.html#zrdds_interface_ddsif", [
        [ "3.1. 简化接口的使用", "zrdds_interface.html#zrdds_interface_ddsif_use", [
          [ "3.1.1. 流程", "zrdds_interface.html#zrdds_interface_ddsif_flow", null ],
          [ "3.1.2. 主要接口介绍", "zrdds_interface.html#zrdds_interface_ddsif_introduce", null ]
        ] ],
        [ "3.2. 简化接口代码示例", "zrdds_interface.html#zrdds_interface_ddsif_example", null ]
      ] ]
    ] ],
    [ "ZRDDS日志调试使用说明", "zrdds_log.html", null ],
    [ "XML配置实体QoS", "zrdds_qos_xml.html", [
      [ "XML配置QoS步骤", "zrdds_qos_xml.html#zrdds_qos_xml_flow", [
        [ "编写QoS配置文件", "zrdds_qos_xml.html#zrdds_qos_xml_create", null ],
        [ "管理ZRDDS内部QoS仓库", "zrdds_qos_xml.html#zrdds_qos_xml_interface_manager", null ],
        [ "QoS设置相关接口", "zrdds_qos_xml.html#zrdds_qos_xml_interface_set", null ],
        [ "XML示例", "zrdds_qos_xml.html#zrdds_qos_xml_example", null ]
      ] ]
    ] ],
    [ "TCP多核传输（>=v2.4.0）", "md_resources_docs_online_cpp_required_tcp_concurrent.html", null ],
    [ "大包零拷贝（>=v2.4.0）", "md_resources_docs_online_cpp_required_udp_large_package_zerocopy.html", null ],
    [ "DDS安全快速入门", "zrdds_security_xml.html", [
      [ "DDS安全快速入门", "zrdds_security_xml.html#zrdds_security_xml_main", [
        [ "内置插件使用", "zrdds_security_xml.html#zrdds_sec_builtin_plugin", [
          [ "安全基础知识", "zrdds_security_xml.html#sec_knowledge", null ],
          [ "内置安全插件功能介绍", "zrdds_security_xml.html#zrdds_sec_builtin_plugin_intro", null ],
          [ "配置CA以及签发证书", "zrdds_security_xml.html#zrdds_sec_config_ca_and_cert", [
            [ "配置CA", "zrdds_security_xml.html#zrdds_sec_config_ca", null ],
            [ "签发证书", "zrdds_security_xml.html#zrdds_sec_cert", null ]
          ] ],
          [ "编写安全配置文件", "zrdds_security_xml.html#zrdds_sec_config_file", [
            [ "权限配置文件", "zrdds_security_xml.html#zrdds_sec_config_persmission_file", null ],
            [ "管理配置文件", "zrdds_security_xml.html#zrdds_sec_config_goverance_file", null ],
            [ "配置文件签名", "zrdds_security_xml.html#zrdds_sec_sign_config_file", null ]
          ] ],
          [ "配置QoS并加载安全插件", "zrdds_security_xml.html#zrdds_sec_config_qos_and_load", null ],
          [ "加载接口", "zrdds_security_xml.html#zrdds_sec_load_plugin", null ],
          [ "QoS配置", "zrdds_security_xml.html#zrdds_sec_config_qos", null ]
        ] ],
        [ "内置插件代码示例", "zrdds_security_xml.html#zrdds_sec_example_code", [
          [ "身份验证", "zrdds_security_xml.html#zrdds_sec_example_code_auth", null ],
          [ "访问控制", "zrdds_security_xml.html#zrdds_sec_example_ac", null ],
          [ "数据加密", "zrdds_security_xml.html#zrdds_sec_example_encry", null ]
        ] ],
        [ "二次开发接口", "zrdds_security_xml.html#zrdds_sec_develop_plugin", null ]
      ] ]
    ] ],
    [ "模块", "modules.html", "modules" ],
    [ "类", "annotated.html", [
      [ "类列表", "annotated.html", "annotated_dup" ],
      [ "类索引", "classes.html", null ],
      [ "类继承关系", "hierarchy.html", "hierarchy" ],
      [ "类成员", "functions.html", [
        [ "全部", "functions.html", "functions_dup" ],
        [ "函数", "functions_func.html", "functions_func" ],
        [ "变量", "functions_vars.html", "functions_vars" ]
      ] ]
    ] ],
    [ "示例", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
".html",
"class_d_d_s_1_1_publisher.html#a1e6f6bae4f914d31b60f071c2866c568",
"group___core_base_function.html#ga8edd3acff0be411d7bdc986c47c8eeb3",
"group___core_qos_struct.html#gga4b2c4d85475a58758fba7601082a40f8afd0f507c4cc38a933f5817d0ea873d03",
"group___cpp_core_struct.html#ga966fbc1aa661fb904494be3ed1dc3d36",
"struct_d_d_s___deadline_qos_policy.html",
"struct_d_d_s___sample_info.html#adfe9c2c74b14f914e5b7da022b07cb86"
];

var SYNCONMSG = '点击 关闭 面板同步';
var SYNCOFFMSG = '点击 开启 面板同步';