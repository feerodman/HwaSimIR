# P17 离线交付说明

本包是实际实施和证据交付，不是整体验收通过证明。状态以 Closeout 和 Issue Ledger 为准。

入口：`index.html` 为普通生产输出与受控案例图库；`diagnostics/plume_comparison/index.html`
区分同映射几何比较与单独 HDR 显示比较；`diagnostics/flicker/index.html` 为尚未关闭的
闪烁诊断。原始全长 MP4 保留开头 ViewValid=0；review_25s.mp4 是另存的重编码评审片。
图片按索引里的 sourceSeq 查找对应 storageIndex 后提取，不按文件名推定序号。

## 程序与依赖

基线 HEAD：`ae43149a2f05809eb23fbf84e98da2e65990f899`，工作区修改未提交。
最终候选 4 的 AArch64 ELF SHA256：
`64590ce93bb9317b73478931d38e311304d46d4fe4eb02a4d58382cc73f6d33f`；
Build ID：`4371742e8fdffafcee89e178c3cb88c9d0528c72`。
Windows Release/x64 EXE SHA256：
`23f70fcf7f44e5972fc81a38be213a93fc96b04f6d0fda29666f756ee88beda6`。
板端完整 Config manifest SHA256：
`2a0c1fb367788995b5bf19d57a320028afd1c8521dcaed6bd89c22e02422f9a9`。

`program` 内包含两种实际程序；`overlay/Config` 只含 6 个变化资源。
它们依赖当前 P16/P17 基线的完整 Config、原 models、DDS/MODTRAN 数据以及离线
Panda3D/Qt/视频运行库。不得拿空 Config 启动；不要只换程序而保留旧 shader。
完整依赖文件清单、各文件哈希见 `support/deployment/candidate4/deployment_manifest.sha256`。
未重复打包巨大 models；`support/baseline/immutable_final_check.json` 验证原 46 个模型文件不变。

Windows 保持 VS2015/v140、Qt 5.12.12 项目兼容构建。RK3588 保持 Debian 11/aarch64
与现有 Mali 硬件启动条件，实际运行记录在每案 board.log。部署/运行不需外网。

## 重跑与验证

在配套原仓库中，设置当前进程的 SSH 认证环境变量后运行 `tools/p17_dds_case.ps1`。
认证值不包含在本包。`p17_media_matrix.ps1` 运行 NIR/SWIR/MWIR 七案例；
`p17_ground_weather_matrix.ps1` 明确使用独立 500 m / 1 m 高度诊断轨迹和原 AIM120；
`p17_performance_matrix.ps1` 关闭重型 raw 导出，使用原始 4318 行轨迹运行
Clear、尾焰、Overcast+尾焰 A/B；预热 10 秒，统计随后 60 秒。
三案使用相同板端/模型/映射/分辨率；轻量计时每 120 帧记录。
曾生成的 120 秒近地候选未通过已有输入身份清单检查，未 INIT、未用于性能证据。
它仅作为失败尝试留存，不能通过重命名冒充原输入或已验证的 30 秒轨迹。
`p17_manual_ui_start.ps1` 只启动普通零参数控制 UI、接收器和板端，按钮操作另行进行。
不要并发启动这些脚本，它们使用同一身份和 DDS 通道。

交付包验证：`python tools/p17_verify_delivery.py <交付目录>`，需已有离线 ffmpeg。
它核对文件存在、SHA256、全部 MP4 解码；这不等于视觉/性能通过。
证据生成脚本需要本机已有 numpy/scipy/Pillow；程序运行不依赖这些诊断库。

`diagnostic_linear` 内 PFM 为实际浮点数据，NPZ 为连续帧中央 ROI 的 float32 副本。
`*_rgb8.png` 是 Stage6 后编码前输出，不是浮点辐射图。没有把 signed PNG 充当 raw。
高开销读回案例不得参与实时性能判定。

## 回滚

先停止控制输入并等待 STOP/DDS/编码队列排空，退出渲染器和接收器。
Windows：备份当前候选后，使用 `rollback/windows/HwaSim_IR.exe` 和
`rollback/windows/Config` 覆盖相同位置的程序及 6 个资源，完整其他 Config 保留。

板端真正的 P17 前原始备份是：
`/userdata/HwaSimIR/HwaSim_IR.before_p17_20260922-014904` 与
`/userdata/HwaSimIR/Config.before_p17_20260922-014904`。
停止程序后，将当前 HwaSim_IR、Config 重命名为新的保留名称，再复制这两个备份
恢复原位置；按备份 Config 内 manifest 校验，然后用原 run_precise.sh 启动。
candidate4 receipt 中的 022130 备份只是上一候选，不是最初 P16 版本。
禁止在程序运行时替换资源、删除原 models，或用 git reset --hard 回滚工作区。

原始 `1.txt` SHA256：`f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901`。
原件未改；`inputs/1.txt` 是包内离线证据副本。
