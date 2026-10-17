# P19 双路部署

以下是本地部署配置，不会由 INIT 修改已运行 renderer。复制匹配版本的 Config 资源和许可到每个独立工作目录；控制端匹配控制域，VideoDisplay 匹配原视频域，客户 Demo 匹配客户域；每个 renderer 只初始化一次共享 DDS 工厂。

控制域=150，原视频域=150，客户域=-1（-1 跟随原视频）。环境变量可覆盖 INI；启动时核对 [DdsDomains] 日志。修改后重启对应进程，RESET 不会切域。

## route1

role=coarse platID=1001 sensorID=1，选择发送=1。将导出的 HwaSimIRRuntime.ini 放入本路 Config，NetworkConfig.ini 放到独立工作目录。启动：HWASIMIR_ROOT=$PWD HWASIMIR_CHANNEL=coarse HWASIMIR_NETWORK_CONFIG=$PWD/NetworkConfig.ini HWASIMIR_RUNTIME_CONFIG=$PWD/Config/HwaSimIRRuntime.ini ./run_precise.sh。核对日志中的实际 identity/domain/topic。

## route2

role=precise platID=1001 sensorID=2，选择发送=1。将导出的 HwaSimIRRuntime.ini 放入本路 Config，NetworkConfig.ini 放到独立工作目录。启动：HWASIMIR_ROOT=$PWD HWASIMIR_CHANNEL=precise HWASIMIR_NETWORK_CONFIG=$PWD/NetworkConfig.ini HWASIMIR_RUNTIME_CONFIG=$PWD/Config/HwaSimIRRuntime.ini ./run_precise.sh。核对日志中的实际 identity/domain/topic。

启动控制端：DataDrivenTestQT --network-config=SenderNetworkConfig.ini --channels-json=dual_sensor.json。QoS、LUT 路径及原始数据文件须可读；SDK 许可由部署方提供。平台级控制会联动同平台其它实例。固定 TX Topic 不能靠不同 platID 区分两个生产者。
