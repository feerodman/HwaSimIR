# P19 客户 raw DDS 排查与恢复

本轮不改四个 IDL，不改图像效果，不先归责客户。现场最初 transport 码 **7** 的根因尚未确认；本轮已真实复现 **SDK write 返回 10** 的可靠历史空间超时，并完成同 PID 恢复。

## 因果链与码值

原附件先有 `(TX/LDecode) Send message failed:7, locator deleted.`，再有 `Waiting sample ... space failed:10`，最后进入 `CustomerDdsVideoPublisher::failLocked`。前两者是 SDK/传输问题；后者是我方明确中止当前客户回合、清空有界队列并拒绝后续 submit 的策略。撤走 Reader 不会自动清除已经失败的当前回合，须新回合重建客户 Writer。

`DDS/CustomerTX/CustomerTxWriter.h::Result` 保留 API、原始返回码、帧号、字节数、已写分片和耗时；`snapshot` 记录实际 QoS、partition、域、匹配 Reader、GUID、locator/transport、资源限制和 liveliness。`failLocked` 记录队列深度、accepted/sent/aborted/rejected。正常低频汇总，错误时快照。

SDK 包目录为 2.4.5，实际运行横幅是 **2.4.4-re6b418c**。该 SDK `ReliabilityQosPolicy.h` 定义 1=BEST_EFFORT、2=RELIABLE；`ReturnCode.h` 定义 API 返回 10=RETCODE_TIMEOUT。**仅当真实调用 DataWriter::write 返回 10 时才作此解释**。内部 transport 日志 7、18、77 与 API 返回码分开保留，未取得厂商内部定义，标为 `UNKNOWN_VENDOR_CODE`，不能直接解释成 errno。

## 实际 QoS 与测试边界

配置请求 RELIABLE、KEEP_ALL；raw 最多 16 样本，H264 最多 4096，write max_blocking_time=100ms。发送器读回并拒绝不可靠、无界或超预算历史配置。raw320 Gray8 每帧 102400 字节，60Hz 有效载荷 6.144MB/s；16 样本是约 267ms 的生产量，**不是单样本 16 字节/16KiB 上限**。

本机与板端实测：相同 XML 下 TCP Reader `get_qos reliability=1`，UDP Reader=2；Writer=2。TCP 的 API ACK 完成不能当成可靠 Reader 收讫证明。未全局改原协议/QoS，不把 XML 字符串当实际生效值。TCP locator 中已连接 `192.168.1.188` 为 transport=8/status=2，其它公布的网卡 locator 未连接不能单凭存在就判为根因。共享 Participant 的 receive_buffer_length=0 是 SDK/transport 默认值，不能解读成不允许大 Sample。

| 证据目录（均在 logs/p19） | 实测结果 | 结论边界 |
|---|---|---|
| tests/domain1_tcp_matrix01 | Windows 27 组尺寸/速率计数一致 | 本机 SDK 功能 |
| tests/board_domain1_tcp_matrix01 | RK3588→Windows 27 组一致，无缺口/重复 | 跨机 TCP，实际 Reader QoS=1 |
| runs/raw_fault_samepid_01 | 暂停专用 TCP Reader，内部码77；正常 Reader 1800帧完整 | 未触发 API write 失败 |
| runs/raw640k_fault_samepid_02 | 640000字节；内部码18/77；正常 Reader 1800帧完整 | 不是现场码7的复现 |
| runs/udp_real_fault_samepid | 实际 RELIABLE，write第17帧返回10、100.459ms；下一回合900帧完整 | 真实 SDK 超时与同PID恢复 PASS |
| runs/raw_application_fault_recover | 一次显式应用故障后，下一回合120帧一致 | 独立状态机诊断，绝非SDK超时证据 |

尺寸矩阵：1399、4096、16384、65500、65536、65540、102400、307200、640000，每种 1/10/60Hz。非图像长度只标 DDS payload probe，不作为图像验收。

## 同 PID 的操作

1. 停止/退出故障客户 Reader，保留其日志、QoS、实际 locator、类型生成工具和 SDK 版本。
2. 控制 UI 按 **STOP**，等待应用停止状态；失败回合 `outputDrainOk=0` 是如实报告，不能因快速返回就改写成成功。
3. **RESET**，分别为选中两路 **INIT**，等两份真实 ACK，再 **START**。不杀 renderer，不重新 DDSIF::Init/Finalize。
4. 客户 raw 用 frameId、实际 sequence length、接收数、缺口/重复和像素摘要验证新回合。H264 新回合从新 SPS/PPS+IDR 起，失败的半 AU 不重播尾半片。

真实 UDP 故障中，同一 renderer PID 保持不变：第一回合 accepted23/sent16/aborted7，失败帧17，STOP 32.707ms；第二回合 accepted=sent=900，像素逐帧一致，STOP33.764ms，runtimeInitCount始终1。旧分支仍有持续输出/Meta/Annotation计数，控制 ACK 和下一回合正常。此结果证明恢复闭环，**没有证明客户现场 Reader 或 TCP transport码7的确切原因**。

## 客户现场最少采集

先独立运行 [最小计数器](../DDS/CustomerRawReceiverMinimal/README.md)，Domain1、TX/LDecode、LDDecoder、102400字节；再分别做仅我方、仅客户、二者同时、客户退出后恢复。保留双方实际 get_qos、匹配变化、发送 API 返回、SDK内部日志和网卡地址，检查有界序列 CDR、32位frameId、take/loan、回调副本生命周期。不得用 native struct、strlen 或 sizeof(sample) 代替 sequence 实际长度。

客户程序和其硬件 SDK 不在当前环境，真实客户 raw 最终互通保持 **NOT_VERIFIED**。用户确认的 H264 软件解码记为 **USER_REPORTED**，不改写成硬解通过。
