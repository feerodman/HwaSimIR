# Extensible类型的可扩展示例

- 示例中app1（发布数据）与app2（订阅数据）模拟早期的程序，进行数据结构为OrignalType的主题数据订阅发布，OriginalType的数据结构如下：

    ```c
    @Extensibility(EXTENSIBLE_EXTENSIBILITY)
    struct OrignalType
    {
        long x;
        long y;
    };
    ```

- 随着业务的发展，原有的OriginType数据类型的xy两个成员不足以表达新的主题数据，此时可在老的结构后面增加成员z以及angle，形成新的数据类型NewType，其数据结构如下：

    ```c
    @Extensibility(EXTENSIBLE_EXTENSIBILITY)
    struct NewType
    {
        long x;
        long y;
        long z;
        float angle;
    };
    ```
    
- 实例程序中app3（发布数据）以及app4（订阅数据）模拟现有程序，进行新版本的主题数据交互；
- DDS的Extensible类型的数据类型可以保证：
    - 程序app1发布的”老“数据，可以被订阅“新”数据的程序app4订阅到；
    - 订阅“老”数据的程序app2可以接收到程序app3发布的“新”数据；
    
## 编译运行

- 依赖CMake编译工具；
- windows平台
    - cd ExtensibleTypeExample && mkdir build && cd build
    - cmake .. -G "Visual Studio 12 2013" -DZRDDS_HOME="ZRDDS安装目录"
    - 打开build目录下的ExtensionTypeExample解决方案
    - 编译并分别运行程序；
- linux平台
    - cd ExtensibleTypeExample && mkdir build && cd build
    - cmake .. -DZRDDS_HOME="ZRDDS安装目录"
    - make 
    - 编译并分别运行程序；