# P20 RK3588 调度与恢复

本轮未超频、未关闭温控，也未移除Xorg/Mali图形上下文。使用原生MPP编码与Mali GLES，启动日志和构建回执保留实际后端。独立光学夹具驱动为Mali-LODX OpenGL ES3.2。

## 拓扑与实际策略

板端MIDR和capacity确认：CPU0—3为Cortex-A55（0x412fd050，capacity447），CPU4—7为Cortex-A76（0x414fd0b0，capacity1024）。不能按CPU编号猜大核。原有运行策略auto_min已把little/big最低频率设置到支持的1704/2016MHz，GPU/DDR运行最低频率为1GHz/2112MHz；候选performance改变governor，但并非新增更高硬件频率。原始系统值以每次policy_before.json为准，启动前/退出后可能回落，不能拿空闲频率判定运行降频。

每个测试保存全部CPU policy和devfreq governor/min/max。双路共同持有一次策略快照，两个进程都退出后逐项恢复并读回核验。控制器见 `tools/p20_runtime_policy.py`，每轮 `policy_restore.json` 记录差异和错误；不能分别让粗路先退出时恢复全局策略从而影响仍运行的精路。

## 比较原则

旧P19原调度、旧P19 performance、新P20原调度、新P20 performance均保留相同offset3000/600输入和原生320/800输出。早期18原调度三次主片段精跟约60.14FPS；历史23三次为60.228/60.223/60.440FPS，P95分别57.88/66.87/104.02ms。performance一次60.238FPS、P95 75.81ms，没有足够稳定收益据此替换默认。23后来因透明光程错误被否决，其性能不能冒充当前候选。完整回放是否积压另列，不能拿主片段代替。

旧版/早期P20主线程大核绑定曾无收益；共享GPU路径修复后另测线程绑定是否有净收益，结论只适用于对应构建和场景。实际线程亲和性保存在每轮thread_placement.txt。未测得收益的配置不自动写入启动默认。

GPU完成等待与读取分离由 `tools/p20_gl_timing.c` 实现：在glReadPixels之前单独glFinish计时，再计读回耗时。逐draw模式另强制同步、记录FBO与presentation参数，用于发现重复通道。两类插桩都会改变调度，不能当正常生产性能或简单相加推出理想FPS。没有重新使用同帧PBO、RGBA读回或缩小输出尺寸冒充优化。

## 重放、温度和清理证据

每个测试目录board_monitor.log按约1秒采集温区、CPU/GPU/DDR实际频率；完整回放、连续310秒压力结果在performance.json/csv和Closeout中汇总。压力输入为独立文件，31个运动子段、18600行、单回合；每600行发生明确坐标跳变，原始1.txt未改。不能把这条压力fixture当连续物理轨迹标定。

所有测试只停止控制器自己启动并核对工作目录的进程。最终退出状态及当前release需以deployment/final_state.json核验。若仍有测试运行，不能仅凭某一轮restore=true告知用户可以断电。

复现命令示例（先确认板卡没有他人的测试进程）：

```powershell
python tools/p20_board_run.py --name my_p20_check --receipt logs/p20/deployment/aarch64_40_prefix_prewarm/receipt.json --tmp-board-logs --frames 600 --source-offset 3000 --sky 1 --band 2 --control-domain 150 --video-domain 1 --customer-domain 1 --use-deployed-domains --use-deployed-qos --render-env HwaSimIRCloudAlgorithm=PuffHybrid
```

凭据通过本机环境变量HWASIMIR_SSH_PASSWORD提供，不写入脚本、INI或交付包。`--scheduling performance`和`--pin-render`仅是可复现候选，不能未测直接作为推荐。脚本异常时必须检查policy_restore.json与进程；禁止宽泛pkill或递归删除候选目录。

## 长测发现的状态组合缓存

候选18/19/20后段CPU线程时间随RenderState数量增长，GPU完成等待和真实读回基本稳定。候选19参数数组精确缓存、20跳过未使用cutout均没有改变增长，不能把这两项称为根因修复。

`diagnostic20_clear_cache`每60次读回后调用Panda `RenderState::clear_cache()`，数目维持约290，精跟完整轨迹60.004FPS、队列峰值1；该轮末条激励少收，功能仍为FAIL。候选21把维护放到绘制完成后的主线程，并调用`garbage_collect()`释放无场景引用的状态；每60业务帧约1.4～2ms，日常状态约86～87。默认`[Performance] RenderStateCacheMaintenanceFrames=60`，环境`HwaSimIRRenderStateCacheMaintenanceFrames`可覆盖，0仅用于诊断关闭。该维护不清理场景节点、纹理或业务输入，不缓存旧画面。

接口语义核对了Panda3D 1.10.15官方`panda/src/pgraph/renderState.cxx`：清除的是组合结果引用，仍被场景持有的状态不会消失。原始参考文件保存在`logs/p20/panda_reference/`。Windows GPU夹具已验证状态身份和像素不变；正式性能及长测应使用无插桩候选。

板端空间不足时可用`--tmp-board-logs`，结束后仍复制到本机。用户已明确授权对已完成P20日志逐文件核验SHA256后清理板端副本；执行记录在`logs/p20/board_log_archive/`，哈希不一致的文件保留，不删除版本、配置或目录。

候选23连续310秒两路各18600帧全部收齐，粗/精约60.007/60.008FPS，精跟P95 56.3525ms、P99 58.2977ms、max142.0354ms，超过80ms占0.129%；队列峰值5，STOP33.555ms。采集全部温区40.692～46.230°C，采样中GPU1GHz、DDR2112MHz，未发现持续热降频。原始每秒数据和完整帧曲线见 `logs/p20/runs/puff23_stress_310s/`，这些性能结果不消除透明光学FAIL。

## 同步RGB读回转换与诊断

38新增 `[RenderBackend] DirectRgbReadback=true`，环境 `HwaSimIRDirectRgbReadback` 优先；源码默认false，只用于Linux HeadlessOffscreen。保留RGB8、同步读取、原生尺寸，最终标注绘完后读取，再用已有NEON分量交换写入Panda底行优先BGR RAM。通过 `modify_ram_image()` 推进原图像身份，capture和两条视频分支仍读取同一帧。没有使用PBO、跨帧缓存、RGBA读取或修改Panda库。

依据是主线程采样中的复制开销及 [Panda 1.10.15 官方读回实现](https://github.com/panda3d/panda3d/blob/v1.10.15/panda/src/glstuff/glGraphicsStateGuardian_src.cxx#L6964)：RGB读取后还进行分量重排与复制。独立生产类FBO夹具 `logs/p20/board_rgb_readback38_native/result.json` 验证冷启动快速路径、标准路径、双读回审计、每帧图像序号及全像素坐标图案。120帧均值快速约8.77～9.00ms，标准约10.93～11.41ms；仅为夹具，不是主程序性能。

诊断 `HwaSimIRDirectRgbReadbackAudit=1` 保留Panda标准读回，并逐帧比较两个结果。它有两次同步读取和额外CPU比较，禁止用于正式性能。实际尺寸与RGB/BGR转换必须在正常INIT重建时重新建立；绑定阶段的POT纹理协商不能被当作最终800/320图像尺寸。


38主程序600同ELF开关比较：新读回60.506FPS、P95 100.232ms，原读回59.214FPS、P95 247.768ms，二者完整冷帧都统计。40正式默认短测三次精跟60.452/60.521/60.506FPS、P95 72.552/87.841/87.496ms；performance为60.458FPS/P95 95.833ms，没有净收益，继续原调度。短测偏差不等于80ms严格达标；完整MWIR4318帧精跟60.047FPS、P95 53.505ms，后续完整矩阵以报告为准。

新直接读回通过libGLESv2符号调用，旧LD_PRELOAD包装器不再拦截该路径。39以后用 `HwaSimIRDirectRgbTiming=1` 内建探针分开GPU完成等待、RGB读取、转换/状态恢复，默认关闭；不能把旧包装器未记录到新路径解释成读回为0。

## 最终40候选实测与交接

两轮310秒各18600帧/路完整守恒。精跟60.015/60.015FPS，P95 56.653/56.308ms；第一轮P99 184.957ms、max508.223ms，第二轮P99 58.399ms、max214.384ms。第一轮第2262条Windows写调用前有约478ms tick间隙，之后追赶发送；对应精跟绘制15.806ms。具体Windows停顿原因未确定，不能算成GPU单帧500ms，也不能删除该批时延。两轮温区分别39.769～45.307°C和38.846～45.307°C。

实际线程快照（完整MWIR启动阶段）记录38个线程允许CPU0—7、2个输出工作线程允许CPU4—7；主渲染线程未强制单核绑定。活动期间频率/温度逐秒采样另在board_monitor.log，不能把启动亲和快照当瞬时CPU占用。

最终600帧同步探针：粗跟GPU等待1.357ms、RGB读取0.605ms、转换/恢复0.055ms；精跟2.579/3.718/0.263ms。全部600条业务读回对应600条渲染记录，不含INIT空闲；一条日志字段被跨线程换行打断，按完整字段恢复解析，原日志保留。探针影响调度，这些分项不直接相加推算正式FPS。

`deployment/final_state.json`核验成对启动退出；`deployment/final_machine_audit.json`再次只读核对：P20测试进程为空、实际策略逐项等于恢复回执、P19/P18哈希未改变。默认保留原调度，不推荐无收益的performance或大核强绑。
