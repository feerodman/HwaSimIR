# P20 源码与复现索引

生产构建源码快照、manifest与ELF回执位于 `logs/p20/build/aarch64_40_prefix_prewarm/`。当前源码未commit/push。行号以当前工作树为准；完整分域机制沿用P19。

| 文件 | 函数/入口 | 行 |
|---|---|---:|
| [HwaSim_IR/HwaSim_IR/IR/IRCloudPuffGeometry.h](../HwaSim_IR/HwaSim_IR/IR/IRCloudPuffGeometry.h) | `IRCloudPuffGeometryFragment` | 5 |
| [HwaSim_IR/HwaSim_IR/IR/IRCloudPuffGeometry.h](../HwaSim_IR/HwaSim_IR/IR/IRCloudPuffGeometry.h) | `IRCloudPuffGeometryVertex` | 57 |
| [HwaSim_IR/HwaSim_IR/IR/IRCloudPuffGeometry.h](../HwaSim_IR/HwaSim_IR/IR/IRCloudPuffGeometry.h) | `IRConfigurePuffNodes` | 82 |
| [HwaSim_IR/HwaSim_IR/IR/IRCloudPuffComposite.inl](../HwaSim_IR/HwaSim_IR/IR/IRCloudPuffComposite.inl) | `EffectiveStage6SiTexture` | 3 |
| [HwaSim_IR/HwaSim_IR/IR/IRCloudPuffComposite.inl](../HwaSim_IR/HwaSim_IR/IR/IRCloudPuffComposite.inl) | `SetupPuffSiComposite` | 9 |
| [HwaSim_IR/HwaSim_IR/IR/IRCloudPresentation.inl](../HwaSim_IR/HwaSim_IR/IR/IRCloudPresentation.inl) | `UpdatePuffVolumeConsumers` | 6 |
| [HwaSim_IR/HwaSim_IR/IR/IRCloudPresentation.inl](../HwaSim_IR/HwaSim_IR/IR/IRCloudPresentation.inl) | `SetupCloudPresentation` | 13 |
| [HwaSim_IR/HwaSim_IR/IR/IRCloudPresentation.inl](../HwaSim_IR/HwaSim_IR/IR/IRCloudPresentation.inl) | `UpdateCloudPresentation` | 124 |
| [HwaSim_IR/HwaSim_IR/HwaSimIR.cpp](../HwaSim_IR/HwaSim_IR/HwaSimIR.cpp) | `CreateStage7CloudVolumeProxyNode` | 578 |
| [HwaSim_IR/HwaSim_IR/HwaSimIR.cpp](../HwaSim_IR/HwaSim_IR/HwaSimIR.cpp) | `void HwaSimIR::SetupStage7VolumetricComposite` | 5140 |
| [HwaSim_IR/HwaSim_IR/HwaSimIR.cpp](../HwaSim_IR/HwaSim_IR/HwaSimIR.cpp) | `void HwaSimIR::InitStage7VolumetricCloudRenderer` | 5482 |
| [HwaSim_IR/HwaSim_IR/HwaSimIR.cpp](../HwaSim_IR/HwaSim_IR/HwaSimIR.cpp) | `void HwaSimIR::UpdateStage7VolumetricCloudAnimation` | 6121 |
| [DDS/Runtime/DdsRuntimeManager.cpp](../DDS/Runtime/DdsRuntimeManager.cpp) | `participantForDomain` | 111 |
| [DDS/Protocol/HwaSimIRProtocolEndpoint.cpp](../DDS/Protocol/HwaSimIRProtocolEndpoint.cpp) | `publishVideoStatus` | 362 |
| [DataDrivenTestQT/DualSensorPanel.cpp](../DataDrivenTestQT/DualSensorPanel.cpp) | `DualSensorPanel::DualSensorPanel` | 55 |
| [DDS/CustomerTX/tests/contract_test.cpp](../DDS/CustomerTX/tests/contract_test.cpp) | `int main` | 25 |
| [DDS/Protocol/DdsStimClient.cpp](../DDS/Protocol/DdsStimClient.cpp) | `bool DdsStimClient::start` | 122 |
| [DDS/Protocol/DdsStimClient.cpp](../DDS/Protocol/DdsStimClient.cpp) | `bool DdsStimClient::sendControl` | 219 |
| [HwaSim_IR/HwaSim_IR/HwaSimIR.cpp](../HwaSim_IR/HwaSim_IR/HwaSimIR.cpp) | `RenderState::clear_cache` | 1879 |
| [HwaSim_IR/HwaSim_IR/HwaSimIR.cpp](../HwaSim_IR/HwaSim_IR/HwaSimIR.cpp) | `m_renderCacheMaintenanceFrames=m_runtimeConfig` | 5301 |

`IRCloudPuffTemplates.h`保存12/24团派生模板，`tools/p20_build_puff_templates.py`保存资产来源与拟合误差，原模型/原输入不改。

测试工具：`p20_board_run.py`双路收发/逐帧守恒；`p20_analyze_performance.py`同机时钟配对与排空分段；`p20_gl_timing.c`GPU等待/读回诊断；`p20_board_optics_fixture.cpp`和`p20_puff_gpu_check.py`实际GPU与独立参考；`p20_transparent_cloud_contract.py`已知透明合成失败反例；`p20_mutable_shader_probe.py`状态缓存维护GPU像素检查。

复现/交付：`p20_final_matrix.py`串行实机矩阵；`p20_runtime_policy.py`双路共享策略恢复；`p20_start_candidate_pair.py`板端成对启动；`p20_candidate_handoff.py`最终实际身份及退出检查；`p20_archive_owned_board_logs.py`授权后逐文件SHA核验清理；`p20_closeout_report.py`和`p20_delivery_report.py`从真实回执生成报告，明确保留失败。

接收回归：`p20_test_h264_end.py`标准结束/精确1400边界/长暂停/多回合，`p20_test_receiver_reconnect.py`真实SDK晚加入与重连；`DDS/CustomerTX/tests/contract_test.cpp`四Topic和raw容量640000/640001，原客户生成类型不变。

## 透明内容局部光程与共享附件（最终候选）

| 文件 | 关键函数 | 当前行 |
|---|---|---:|
| [IR/IRCloudTransparentTransport.h](../HwaSim_IR/HwaSim_IR/IR/IRCloudTransparentTransport.h) | `inline std::string IRCloudPrefixGlsl` | 8 |
| [IR/IRCloudTransparentTransport.h](../HwaSim_IR/HwaSim_IR/IR/IRCloudTransparentTransport.h) | `inline void IRApplyPlumeCloudPrefix` | 106 |
| [IR/IRCloudTransparentTransport.h](../HwaSim_IR/HwaSim_IR/IR/IRCloudTransparentTransport.h) | `inline void IRApplyRainCloudPrefix` | 135 |
| [IR/IRCloudTransparentRuntime.inl](../HwaSim_IR/HwaSim_IR/IR/IRCloudTransparentRuntime.inl) | `void HwaSimIR::SetupPuffTransparent` | 4 |
| [IR/IRCloudTransparentRuntime.inl](../HwaSim_IR/HwaSim_IR/IR/IRCloudTransparentRuntime.inl) | `void HwaSimIR::UpdatePuffTransparent` | 60 |

`IRCloudPrefixGlsl`使用R32F高精度参数、真实不透明深度、puff部分光程和世界层面；气体保留原24/40步和alpha。`SetupPuffTransparent`在已有云相机注册shader状态，原几何通过实例复用，分通道保存SI与白云源；`UpdatePuffTransparent`保留原父状态/变换/业务隐藏、绘制顺序、真实共享云路径及有界参数上传。

实际GPU工具：`p20_transparent_shader_dump.cpp`导出生产C++生成shader；`p20_transparent_gas_gpu_fixture.py`检查核心/降水、跨边缘和合成；`p20_shared_camera_fixture.py`检查相机合并像素不变；`p20_board_transparent_fixture.cpp`/`p20_run_transparent_fixture.py`/`p20_check_board_transparent.py`检查真实Mali尾流和CPU独立参考。`p20_graphics_log_audit.py`追加历史日志图形错误检查，不让计数PASS覆盖shader断言。

当前构建身份以同名build_receipt.json/source_manifest.sha256为准。23曾性能良好，但透明光程错误，不能作为当前性能或光学证据。

## 原生RGB读回、预热和新增验证

| 文件 | 入口 | 行 |
|---|---|---:|
| [IR/IRDirectRgbReadback.h](../HwaSim_IR/HwaSim_IR/IR/IRDirectRgbReadback.h) | `class IRDirectRgbReadback` | 18 |
| [IR/IRDirectRgbReadback.h](../HwaSim_IR/HwaSim_IR/IR/IRDirectRgbReadback.h) | `void compareAfterPanda` | 42 |
| [IR/IRDirectRgbReadback.h](../HwaSim_IR/HwaSim_IR/IR/IRDirectRgbReadback.h) | `void do_callback` | 50 |
| [IR/IRDirectRgbReadbackRuntime.inl](../HwaSim_IR/HwaSim_IR/IR/IRDirectRgbReadbackRuntime.inl) | `void HwaSimIR::SetupDirectRgbReadback` | 1 |
| [HwaSimIR.cpp](../HwaSim_IR/HwaSim_IR/HwaSimIR.cpp) | `const bool prefixPrewarm` | 11120 |

`p20_samepid_resize.py`验证同PID 800→320→800的真实INIT/ACK/START/STOP、客户解码与保存。`p20_rgb_readback_production_fixture.cpp`及对应运行工具验证实际生产读回类，`HwaSimIRDirectRgbReadbackAudit=1`用于生产主链双读逐字节审计，`HwaSimIRDirectRgbTiming=1`分别测GPU完成等待、RGB读取与转换。正式性能关闭所有这些同步审计/探针。

`p20_validate_recording.py`独立解析实际MP4的SEI，逐帧核验身份、原SEI JSON哈希、Qt侧文件哈希及全部JSON字段，并实际解码每帧。原VideoDisplay会重排侧文件的JSON键，因此不宣称两个JSON序列化字节一致；严格P7旧工具的字节比较失败日志也保留。
