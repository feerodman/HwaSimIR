# P18 双通道运行手册

工作区 `D:\HwaSimIR`；板卡 `192.168.1.116`；构建 VM `192.168.203.128`；Windows 接收机 `192.168.1.188`。凭据仅放当前进程的安全环境/SSH agent，不写入命令清单、配置或交付包。实际执行命令和每轮覆盖值见 `logs/p18/runs/<case>/commands.json`、`sender.ini`、`sensor.json` 及板端 effective-config 日志。

## 构建与隔离部署

Windows renderer 沿用 VS2015 v140 x64，激励与新 Demo 使用 Qt 5.12.12 MinGW 7.3。SDK 安装目录为 `F:\Programs\ZRDDS\ZRDDS_VS2015\ZRDDS-2.4.5` 和 `F:\Programs\ZRDDS\ZRDDS_MinGW7.3.0\ZRDDS-2.4.5`。运行 banner 的 2.4.4-re6b418c 与安装包名2.4.5分别记录，不混称新版本。FFmpeg 位于 `.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1`。

```powershell
Set-Location D:\HwaSimIR
./tools/stage0_build.ps1
./tools/p18_build_customer_receiver.ps1 -BuildDirectory build-p18-customer-receiver-v4
./tools/p11_rk3588_source_build.ps1 -Mode Build -ConfirmSourceStable `
  -OutputDirectory logs/p18/build/aarch64_new
python tools/p18_prepare_instances.py --name candidate_new `
  --elf logs/p18/build/aarch64_new/HwaSim_IR.aarch64
```

构建前关闭同名运行中的 EXE；否则 Windows 链接器会报 Permission denied。AArch64 工具使用 VM 密钥或 `P11_VM_PASSWORD`，板端部署使用 `HWASIMIR_SSH_PASSWORD`；本文件不提供密码值。主程序、当前库和板端资源的 SHA256、Build ID、manifest 在 `logs/p18/deployment/candidate4/receipt.json` 及两个实例的 verification.log/deployment_version.env 中。

当前已部署的 P18 ELF：`f7dddbdb4e19f4e3676f9e288255c59ee86cf41ae0817a3aa724bc09e044e40d`，位于 `/userdata/HwaSimIR/p18/candidate4/HwaSim_IR`。两实例硬链接该 ELF，各自 cwd/Config/network.ini/logs/licence 运行副本；新资源替换独立 inode，不写回保留的 `/userdata/HwaSimIR` P17v12 基线。资源 manifest 摘要与逐文件校验以 `logs/p18/deployment/candidate4/receipt.json` 及 verification.log 为准。

## 正式双路

| 实例 | 绝对工作目录 | 本地身份 | 有效INIT | 客户输出 |
|---|---|---|---|---|
| coarse | /userdata/HwaSimIR/p18/candidate4/coarse | 1001/1 | 320×320、h264En=false | TX/LDecode、Gray8、102400字节/帧 |
| precise | /userdata/HwaSimIR/p18/candidate4/precise | 1001/2 | 800×800、h264En=true | TX/HD264、灰度H264、有效片≤1400字节 |

两份完整 runtime INI 位于 `logs/p18/deployment/candidate4/coarse/` 与 `precise/`；network.ini 配置也在同目录。当前隔离配置 coarse UDP 8888 / TCP5554、precise UDP8889 / TCP5555，均彼此独立；生产控制走 DDS，不依赖这些 TCP 连接。角色默认模板的端口与隔离实例可能不同，必须看实际 network.ini。

分别在两个板端 shell 启动（日志目录预先创建，不能覆写已有验收目录）：

```sh
cd /userdata/HwaSimIR/p18/candidate4/coarse
env HWASIMIR_ROOT="$PWD" HWASIMIR_CHANNEL=coarse \
  HWASIMIR_NETWORK_CONFIG="$PWD/network.ini" \
  HWASIMIR_RUNTIME_CONFIG="$PWD/Config/HwaSimIRRuntime.ini" ZRDDS_HOME="$PWD/zrdds" \
  RenderPresentationMode=HeadlessOffscreen HwaSimIRExitOnStop=true \
  HwaSimIRLocalRecordingEnable=false HwaSimIRCustomerAuditPrefix=logs/manual_coarse \
  ./run_precise.sh
```

```sh
cd /userdata/HwaSimIR/p18/candidate4/precise
env HWASIMIR_ROOT="$PWD" HWASIMIR_CHANNEL=precise \
  HWASIMIR_NETWORK_CONFIG="$PWD/network.ini" \
  HWASIMIR_RUNTIME_CONFIG="$PWD/Config/HwaSimIRRuntime.ini" ZRDDS_HOME="$PWD/zrdds" \
  RenderPresentationMode=HeadlessOffscreen HwaSimIRExitOnStop=true \
  HwaSimIRLocalRecordingEnable=false HwaSimIRCustomerAuditPrefix=logs/manual_precise \
  ./run_precise.sh
```

launcher 文件名沿用 run_precise.sh；实际角色由上述环境和 network.ini 决定。它设置既有板端性能策略并在退出时恢复，不降低60FPS/80ms门槛。

Windows 启动两个接收实例，分别使用新输出目录和有效 licence 工作目录（SDK licence 文件不能公开打包）：

```powershell
$rx='D:\HwaSimIR\build-p18-customer-receiver-v4\release'
Set-Location $rx
./HwaSimIRCustomerTxVideoReceiverDemo.exe --topic TX/LDecode --codec raw `
  --pixel-format gray8 --width 320 --height 320 --fps 60 --domain 150 `
  --qos "$rx/ZRDDS_PROTOCOL_QOS.xml" --output D:/HwaSimIR/logs/p18/manual/coarse `
  --display 1 --idle-ms 12000 --timeout-ms 210000 --ffmpeg "$rx/ffmpeg.exe"
```

```powershell
$rx='D:\HwaSimIR\build-p18-customer-receiver-v4\release'
Set-Location $rx
./HwaSimIRCustomerTxVideoReceiverDemo.exe --topic TX/HD264 --codec h264 `
  --pixel-format gray8 --width 800 --height 800 --fps 60 --domain 150 `
  --qos "$rx/ZRDDS_PROTOCOL_QOS.xml" --output D:/HwaSimIR/logs/p18/manual/precise `
  --display 1 --idle-ms 12000 --timeout-ms 210000 --ffmpeg "$rx/ffmpeg.exe"
```

客户 Demo 不启动或控制 renderer。随后启动唯一激励（用已验收 sender.ini 的绝对路径）：

```powershell
Set-Location D:\HwaSimIR\build-DataDrivenTestQT-codex-mingw73_64-Release\release
./DataDrivenTestQT.exe `
  --network-config=D:/HwaSimIR/logs/p18/runs/dual_perf_on_v4/sender.ini `
  --input-file=D:/HwaSimIR/DataDrivenTestQT/1.txt --channel=precise `
  --plat-id=1001 --sensor-id=2 --dual-channel=1 --duration-sec=120 `
  --sensor-pixel-angle-urad=25 --save-mp4=0
```

`--dual-channel=1` 明确生成两个 INIT（1/320/raw，2/800/h264），不以广播INIT覆盖两种几何。`StimDualInitBarrier ackMask=3` 后才START。Realtime每行一次sensor255广播，原4318行不跳过，engineState/源时间/ViewValid不覆盖；每路本地sourceSeq由1连续到4318，首659隐藏目标按原输入保留。80ms不是发送API计时，跨主机时钟未经校准时仍 NOT_VERIFIED。

可用一条已实现的编排命令重做同板同步验收，不启动 VideoDisplay：

```powershell
Set-Location D:\HwaSimIR
python tools/p18_dual_run.py --name dual_manual_new --deployment candidate4 --band SWIR
python tools/p18_analyze_dual.py logs/p18/runs/dual_manual_new
```

脚本拒绝覆盖目录，检查无后台 VideoDisplay/其他激励和无已有板端 renderer。expected每路4318；raw Sample数4318；H264片数随内容变化，不能写死等于帧数。compare脚本逐行核验业务摘要及sourceSeq，不凭截图相似判断同激励。

## 生命周期与其它路径

正常顺序：接收端→renderer→发现→RESET→两个INIT→两份ACK→START→两路running→Realtime→STOP→板端输出排空→接收本地idle flush→ffprobe/摘要核验。无 VideoDisplay 时这一流程完整可用。原分支与新分支同时启用时可另开原 VideoDisplay，其关闭不构成 renderer 输出开关。

客户独开示例：

```powershell
python tools/p18_single_run.py --name independent_new --role coarse --width 320 `
  --codec h264 --pixels rgb24 --band SWIR --customer-only
```

该隔离测试设置 `HwaSimIRDdsVideoEnable=false`、`HwaSimIRLocalRecordingEnable=false`、`TcpSendVideo=false`、`TcpSendRealtimeData=false`、`TcpSendAnnotation=false`、`TcpForwardInitControl=false`。原typed控制仍启用。此时 `--no-video-status-barrier=1` 使用明示控制ACK和2秒settle，应用ready状态不能从旧VideoStatus推断。

其它测试入口：`p18_single_run.py` 的 `--old-display --old-reopen --late-seconds --reconnect --absent --slow-ms --reject --disabled`；`--color-card` 是客户分支诊断色卡，不能作为IR场景。四Topic由role+codec选择，RGB/gray由pixels明确指定。`--rounds=2` 对原始输入执行完整多回合，每回合先真实STOP屏障再RESET/INIT。改变raw几何必须同步重启/配置接收端。

## 回滚与失败定位

P18 没覆盖板端保留的 P17v12 主目录，停止已确认 cwd 位于candidate4的实例后，从 `/userdata/HwaSimIR` 用原 launcher启动即可恢复P17。原始输入、模型、旧协议和历史证据保留；不要删除失败目录后重跑同名案例。当前工作区恢复客户开关只需 `[CustomerDdsVideo] Enable=false`；尾焰单独回退可用 baseline/sprite.vert、sprite.frag 对照，不能整体回退P17。

先看 RuntimeInstance、DeploymentVersion、CustomerTX sharedRuntime/runtimeInitCount、INIT requested/actual mode、StimDualInitBarrier、原生缓冲几何、CustomerTXDrain。raw容量拒绝的ACK超时是预期失败，不得算正向通过。连接reset时API OK不保证送达，保留端点日志和部分载荷，再排查连接/进程；禁止把缺失帧删掉后宣称完整。

GUI窗口刷新、解码、录像保存和屏幕呈现是四个不同指标。Demo summary中的configured fps是配置，不是性能结论；最终严格性能结果见结项记录及机器可读报告。
