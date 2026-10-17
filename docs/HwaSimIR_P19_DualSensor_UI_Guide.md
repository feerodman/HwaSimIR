# P19 双传感器 UI 操作

启动 `build-p19-stim/release/DataDrivenTestQT.exe` 即进入新双路界面。实验默认 Domain150；现场使用 [Domain1 SenderNetworkConfig.ini](../ConfigExamples/P19/customer_domain1/SenderNetworkConfig.ini)：

在本工作区或解压后的功能交付包中，可直接双击 `build-p19-stim/release/start_domain1.cmd` 使用现场Domain1。包内没有许可证，使用现有本机授权；配置中的IP应与实际网络一致。

```text
DataDrivenTestQT.exe --network-config=<customer_domain1>/SenderNetworkConfig.ini
```

![双路界面 125%](../logs/p19/ui/scale125.png)

两行常驻路由控制分别有发送勾选、role、platID、sensorID、状态和计数；下面两个标签页均提供完整 INIT 传感器/照明器、天气、环境、平台参数。长表单可滚动，RESET/INIT/START/STOP和配置按钮常驻底部。测试了 Qt 缩放因子1、1.25、1.5；未改变用户 Windows 系统缩放。

## 默认操作

默认粗路1001/1，320×320 raw Gray8；精路1001/2，800×800 H264 Gray8。共用原 `1.txt`、同一公共目标0x22。先按 RESET，再分别 INIT，等待两路真实 ACK 就绪，然后 START。源结束时共同 STOP，避免重复发送最后一行。STOP后 RESET 解锁编辑，再初始化新回合。

只发一路时只勾选该路。勾选控制 INIT/Realtime 的精确身份发送，默认不用 sensor255广播。**旧 Control 只有 platID，没有 sensorID**：同平台其它已部署实例可能随 START/STOP/RESET 联动；要独立控制请使用不同 platID。ACK 没有事务号，不能辨认同一身份延迟到达的旧应答，界面明确提示这个旧协议限制。

role决定客户 Topic 的 H/L 前缀，与sensor数字解耦。已实测 coarse sensor7 / precise sensor42，并使用不同平台1001/1002、不同已有模型。固定客户 Topic 不含身份，不能靠不同 platID 在同域同partition同时发布两个相同Topic的生产者；UI拒绝这类冲突。

## 数据与模型

公共文件/模型同时作用于勾选“跟随公共”的路；取消某路的跟随并选择本路文件/模型即可覆盖。只选择已经存在的模型资源，不修改模型文件。平台INIT可取该路数据首行，也可取消后手动填平台字段。目标位置和源 viewValid 保持数据驱动；强制显示是显式诊断勾选。

同一个规范路径只加载一份源、每 tick 推进一步，再分别按身份发送两路；每路默认60Hz，总write约120次/秒不等于单路120FPS。不同源各有游标，共用同一调度起点；任一路 EOF 就结束本轮，另一路显示“另一源 EOF，共同STOP”。

普通输入不查固定SHA、文件名、路径或4318行，接受符合原表结构的改名/短文件。验证列数、有限数值、整数身份、时间顺序与实际物理支持域；空/坏数据明确拒绝。LUT及coverage manifest完整性校验保留，不与输入准入混淆。

## 保存、迁移、部署

保存/读取双路 JSON，包含两路完整参数和公共设置；复制路参数保留对方身份、role、尺寸、编码和像素格式，避免复制后冲突。读取旧单路 INI 迁入当前页，并只勾选当前路；再次启用另一路须检查 ID/Topic。

导出产生两路 NetworkConfig.ini、完整 HwaSimIRRuntime.ini、SenderNetworkConfig.ini、dual_sensor.json 和启动说明。Domain、role、renderer本地身份、像素格式是**部署配置**，不在 INIT 中，导出不会偷偷修改正在运行的 renderer。修改Domain/QoS后需重启参与者；界面同时显示当前生效域/路径与待导出设置。

使用 [ConfigExamples/P19](../ConfigExamples/P19/) 的 lab_domain150/customer_domain1，配套完整冻结资源和许可。完整性受控板端配置变更后必须重新生成 deployment_manifest/version，不能绕过 launcher 校验。测试入口 `--validate-dual` 只做源与配置校验，不发 INIT；`--export-only` 导出后退出。

旧单路诊断 CLI 仍可通过显式 `--channel=coarse|precise` / `--legacy-single-ui` 使用。新双路的不同数据/模型/INIT参数使用双路JSON，不能假设所有历史单路诊断参数都应用到新面板。

## 三个域分别导出（UI构建12）

公共区域可分别编辑控制/激励域、原视频/VideoDisplay域、客户TX域；客户选择“跟随原视频域”时保存-1。JSON的domain仍表示控制域，新增videoDomain和customerDomain；旧JSON默认原视频跟随控制、客户跟随原视频。三者为启动部署配置，修改后须导出并重启相应进程，INIT不负责切域。当前进程有效域标签始终反映已启动的控制端。

提供 `ConfigExamples/P19/control150_video1`：控制150、原视频1、客户1；另保留全150、全1。导出说明列出三域，两个renderer INI分别保存三项。接收端命令行domain按其视频分支匹配。详见 [域配置与实测](HwaSimIR_P19_DDS_Domains.md)。17项配置校验和真实UI导出三域读回通过；原生保存对话框自动化仍未重测。
