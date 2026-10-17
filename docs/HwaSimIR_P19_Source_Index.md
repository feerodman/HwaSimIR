# P19 源码与验证入口

路径均相对仓库/交付源码根目录。原四个客户IDL/generated、原控制协议结构未改。

| 文件 | 关键函数与职责 |
|---|---|
| DDS/CustomerTX/CustomerTxWriter.h | Writer::open/send/drain/close；typed SDK返回码、get_qos、locator、资源边界、标准Sample写入 |
| HwaSim_IR/HwaSim_IR/Video/CustomerDdsVideoPublisher.cpp | Impl::failLocked/run，beginRound/endRound；客户独立有界队列、回合恢复、clean AU、标准结束NAL、健康Writer保留 |
| HwaSim_IR/HwaSim_IR/TcpCommThread.cpp / TcpCommThread_Linux.cpp | INIT有效参数、原输出和客户分支、客户先排空、共享编码配置 |
| HwaSim_IR/HwaSim_IR/Video/DdsVideoPublisher.cpp | endRound；原分支队列/ACK排空，移除健康Writer保留时固定5秒等待 |
| DDS/Runtime/DdsRuntimeManager.* | 原有进程级唯一Init/Finalize；客户复用，不另建全局运行时 |
| HwaSim_IR/HwaSim_IR/Video/H264Compatibility.h | profile/level/宏块容量/DPB/码率检查，显式拒绝不支持选项 |
| HwaSim_IR/HwaSim_IR/Video/H264MppEncoder.cpp / H264FfmpegEncoder.cpp | 后端参数传入、返回值检查、MPP读回、实际编码 |
| DDS/CustomerTX/AnnexBEndScanner.h | 跨Sample起始码/标准结束NAL扫描，保留不完整尾部，无idle重置 |
| DDS/HwaSimIRCustomerTxVideoReceiverDemo/main.cpp | 参数、四种typed订阅、GUI定时显示、DDS/线程RAII退出顺序 |
| 同目录 CustomerDdsSubscription.h | 回调长度校验、有效内容深拷贝、SDK loan责任边界 |
| 同目录 CustomerReceiver.h | push有界队列，parseBytes/packet/receiveFrames，finishStream/flush，增量解码及逐回合录像 |
| 同目录 ReceiverOptions.h | 显式像素/尺寸/Topic/Domain/退出策略校验；诊断默认关闭 |
| DDS/CustomerRawReceiverMinimal/main.cpp | RawReceiver::on_process_sample/consume、只SDK/generated的计数程序、复制与释放示例 |
| DataDrivenTestQT/DualSensorPanel.* | collect/initialize/receiveAck/start/pump/stop/reset；各路完整INIT，source共享游标、平台级控制 |
| 同目录 DualSensorPanel.cpp | snapshot/saveConfig/loadConfig/copyChannel/exportDeployment；保存、迁移、复制、部署导出 |
| DataDrivenTestQT/ReplaySource.h | 普通文件内容/列数/有限值校验，接受不同名称和行数；不限制输入SHA |
| DDS/Protocol/DdsStimClient.* | 任意platID/sensorID路由的ACK及状态观察，运行时域/QoS日志 |
| HwaSim_IR/HwaSim_IR/HwaSimIR.cpp | INIT容量校验、独立角色选择、H264配置加载；恢复测试后新增三域有效值/来源校验；云性能状态以最新实测报告为准 |

## 可复跑验证

- `tools/p19_board_run.py`：真实双renderer和双路UI、四Topic/像素组合、两回合、有效域1、故障Reader、独立数据源、VideoDisplay退出重开；每例独立目录。
- `tools/p19_raw_matrix.py`：typed大Sample尺寸/频率矩阵，Windows或板端发送，读取最小Demo真实计数。
- `tools/p19_test_h264_end.py`：1/30/120帧、整1400倍数AU、起始码跨Sample、中途暂停、两回合、截断负例。
- `tools/p19_test_receiver_reconnect.py`：真实SDK晚加入/进程重开、IDR恢复、显示/保存；不声称补回离线帧。
- `tools/p19_validate_ui.py`：普通17行改名输入、坏数据、非法ID/容量/重复Topic、老INI迁移。
- `tools/p19_inspect_h264.py` / `p19_encoder_probe.cpp`：实际SPS/PPS与生产FFmpeg编码类探针；硬件互通独立标记。
- `tools/p19_archive_p17.py` / `p19_archive_history.py` / `p19_verify_history.py` / `p19_prepare_releases.py` / `p19_cleanup_releases.py`：已执行的归档、校验、搬迁和白名单清理；不要无目的重复运行带exist_ok=False的已完成步骤。
- `tools/p11_rk3588_source_build.ps1` / `p19_update_deployment.py`：源码摘要、ARM构建回执、原子部署及资源manifest。

实际测试状态以 `logs/p19/` 的结果文件及结项索引为准；存在历史FAIL不应覆盖或删除。

## 独立DDS域补充索引

- `DDS/Runtime/DdsRuntimeManager.{h,cpp}`：`participantForDomain` 统一持有按域缓存，`start/shutdown` 保持全局一次初始化/释放。
- `DDS/Protocol/HwaSimIRProtocolEndpoint.{h,cpp}`：输入/ACK使用控制域；旧Meta/Annotation/Status使用原视频域，分域时Status镜像到控制域。
- `DDS/Runtime/BoardTelemetryService.h`：`start` 的writer与clockReader跟随原视频域。
- `Video/DdsVideoPublisher.cpp` 与 `Video/CustomerDdsVideoPublisher.{h,cpp}`：借用对应Participant，不触发第二次Init。
- `DataDrivenTestQT/DualSensorPanel.{h,cpp}`：三域表单、JSON严格数值校验、旧配置继承及部署导出。
- `tools/p19_configure_domains.py`：停止状态下只修改P19三个域，备份INI并同步完整性摘要；不改P18。
