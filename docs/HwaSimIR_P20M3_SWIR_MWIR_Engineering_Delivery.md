# P20M3 SWIR / MWIR 工程成像交付

> 尾焰/尾部热源快速修复：最新候选为 `P20_P20M3_SWIR_MWIR_Engineering_20261021_v6`，`tools/p20m3_run.py` 默认指向v6。下面v5长测数据保留为历史证据，不能算作v6性能认证。本次仅做短时成像验证，详见 [尾部热源修复交付](HwaSimIR_P20M3_Plume_Rear_Hotfix.md)。

本轮在 `D:\HwaSimIR` 接续 `5d1d146b4ab14984559e4f2d3c949ece2d228616`，保留用户原有任务文件及 P19/P20/P20M/P20M2 证据。工程运行与严格精度认证分开。新模型为 `P20M3_GREY_GAS_1`，质量为 `ENGINEERING_APPROX`；不是 P14 数据改名，也不宣称正式 MODTRAN 全域认证已完成。

## 程序与运行入口

- Windows renderer：`D:\HwaSimIR\build-p20m3-renderer\bin\HwaSim_IR.exe`。
- 正式 Qt stim：`D:\HwaSimIR\build-p20m3-stim\release\DataDrivenTestQT.exe`。
- 接收器：`D:\HwaSimIR\build-p20m3-customer-receiver\release\HwaSimIRCustomerTxVideoReceiverDemo.exe`。
- 本轮证据：`logs/p20m3/20261021_engineering`；双路运行证据：`logs/p20/runs/p20m3_*`。
- 板卡最终隔离候选：`/userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v5`。v1/v2/v3也保留。`current` 和原 P19/P20 启动入口未修改。

自动双路运行会启动板卡 renderer、正式 Qt、Windows 接收解码器，执行 INIT/ACK/START/STOP、保存视频及逐帧审计，结束后恢复板卡性能策略。先在 PowerShell 设置 `HWASIMIR_SSH_PASSWORD` 为已授权 root 账号的密码，再在工程根目录执行：

```powershell
F:\Programs\anaconda3\python.exe tools/p20m3_run.py --case native --name p20m3_user_native --utc-date 2026-10-21 --utc-hour 3.5
F:\Programs\anaconda3\python.exe tools/p20m3_run.py --case all-csv --name p20m3_user_csv --utc-date 2026-10-21 --utc-hour 3.5 --physical-evidence
```

每次使用新的 `--name`，避免覆盖证据。UTC 是显式回放起点，应按场景修改，不读取操作系统日期冒充协议时间。CSV 原始 10 ms 时间步长不改写；60 Hz 逐行发送是减速回放。`native` 生成 4261 行原生 60 Hz、71 秒时间轴（去除前600帧后稳定段61秒）。性能测试不启用 GPU 线性帧抓取；物理抓取与性能测量分开。

交互式 Qt 使用自己的板卡配置：

```powershell
F:\Programs\anaconda3\python.exe tools/p20m3_prepare_launch.py
& D:\HwaSimIR\build-p20m3-stim\release\DataDrivenTestQT.exe --network-config=D:/HwaSimIR/build-p20m3-stim/release/P20M3_Board_NetworkConfig.ini --channels-json=D:/HwaSimIR/build-p20m3-stim/release/P20M3_Board_Dual.json
```

先启动配套板卡候选和接收器，再在 Qt 中 INIT、等待两路 ACK、START；STOP 后等待视频排空。自动运行入口已经包含接收器启动和排空，适合直接复核。

## 模型、单位与几何

共享实现 `Shared/EngineeringAtmosphere.h` 同时用于 renderer、Qt 单/双路预检与数值扫描。协议 `0=SWIR` 对应 1.1–2.5 μm，`2=MWIR` 对应 3–5 μm，矩形响应的波段平均。既有 NIR 数据未改名；NIR 诊断/兼容使用 `StrictModtran` 策略。

参数文件位于 `HwaSim_IR/Bin/Config/Atmosphere/Engineering/P20M3_GREY_GAS_1/parameters.txt`，SHA-256：

`93dcb6b592ae32a31615422fcc5fd744156827020eb6bedfda875d197632ccb4`

候选加载一次并校验身份。源码快照和 ELF 身份另由 build receipt、source manifest、部署 manifest 绑定，参数 hash 不冒充全部物理实现身份。`EngineeringContinuous` 默认供本候选 SWIR/MWIR 使用；`StrictModtran` 保留原严格 LUT 检查。工程输出的 `engineeringUsable` 与严格 LUT `valid` 分离。

两端海拔 3–20 km、WGS84 double ECEF 弦距 0–50 km；协议纬度/经度为度，海拔按现有协议椭球高约定使用。正式相机局部偏置为零，目标位置是协议模型参考点。此交付未引入大地水准面修正或模型表面逐像素路径；材质/几何仍由现有渲染器处理。删除底层 1 m 夹距，正式大气距离不再来自地球尺度 float 坐标相减。重合 LOS 的传播为 tau=1、路径项=0，观察方向状态仍单独诊断。

三个灰气吸收分量由既有可靠 MLS 透过率的多个 0.1–2 km 锚点拟合，去除重复太阳维度。高空采用明确的干气/水汽尺度高度延拓；气溶胶采用 Koschmieder 消光、Angstrom 指数与声明的单次散射反照率。32 段真实 LOS 递推分别累计吸收热发射与散射，不将全部消光当热源。RH 是 MLS 地表水汽缩放语义，非高空恒定局地 RH。已扫描 RH30–85、能见度6–23 km。

太阳为 5772 K 黑体形状归一到固定 1 AU 的 1361 W/m²；这是声明的工程源谱，不冒充 MODTRAN 官方太阳谱。球形直线太阳射线计算地球遮挡，晨昏采用平滑工程漫射闭合；未宣称折射或晨昏高精度。直射法向辐照度、半球漫射辐照度单位为 W/(m²·μm)，路径辐亮度为 W/(m²·sr·μm)。0.30–2.50 μm 加热独立积分为 W/m²，保留协议仿真时钟和材质热历史，未恢复整机发动机加热。

背景独立查询天顶到大气顶的方向辐亮度、天底到零高灰体地面的传播/照明，接入既有天空穹顶插值。这是简化角分布工程背景，不是完整方向 MODTRAN 天空。目标新增、隐藏、删除和失败不再提供背景 LOS。云雨和局部尾焰保持原功能；SI 分支沿用既有不重复施加 legacy 灰度雾变换的规则。

## 实际完成的验证

- Windows SWIR/MWIR 均通过正式 Shader 成像；`CompareOnly=false`、SI 实际运行，GPU 为 NVIDIA RTX 3070 Ti。板卡是 ARM Mali 硬件 EGL，非 llvmpipe。
- `provider_scan.json`：2400 个固定种子合法状态、4800 次双波段查询，失败0。另27324次边界/扫距覆盖3/20 km、0–50 km、上下/竖直、极点/日期线、SZA0/73/88/90/92/96/100/120/180。正短距包含0.1/0.5/1 m，无最小距离吸附。
- 近似均匀短段1段与64段的最大分量差约4.11×10⁻¹⁵；随机32/64段 tau 最大差1.252×10⁻⁴。这是数值收敛证据，不是独立 MODTRAN 物理精度。
- 缺失 LUT、损坏 LUT 身份均在隔离副本中实际启动，Qt INIT ACK/START/STOP成功，坏表不参与工程查询。
- 旧 Qt 单路界面两轮 RESET/INIT/START/STOP完成；双路正式入口运行三份完整原始 CSV。独立 DDS 发端另验证无目标、隐藏、删除、恢复。
- 固定相机/UTC 的 SWIR/MWIR 生命周期各180帧，背景分量只有一个唯一快照；目标0/1变化不影响背景。
- 原始 `circleDataAll.csv / circleDataAll2.csv / circleDataAll3.csv` 分别3940/319/2902行。三份都实跑，第二份未因是前缀而省略。前19行 ViewValid=false 单独处理，非大气缺表清零。

实际 GPU AGC前 PFM 位于 `windows_swir_formal_02`、`windows_mwir_formal_02` 和最终 `windows_*_halfk`。对应 PNG 是正常显示图，有标注；PFM 是独立线性SI浮点帧，不含错误提示或调亮底值。CPU `sensorRadiance` trace 是目标中心标量，未当作逐像素真值。

受控1 km目标、固定40×40中心ROI（含背景混合）示例：日间SWIR平均5.674838、MWIR平均0.426809 W/(m²·sr·μm)；仅隔离测试材质设350 K后MWIR平均0.857446。夜间SWIR平均9.47×10⁻⁵，MWIR平均0.214769。默认产品温度不变。0.5 K Planck 表后的同场景SWIR ROI平均不变、MWIR为0.426813；这是实际GPU对照。

Planck 表使用原CPU矩形响应积分生成R32F，0.5 K节点、手工线性插值，温度范围120–4215 K，域外仍用原公式；StrictModtran仍用原公式。冻结增量预算为 `1e-10 + 0.0025*abs(ref)` W/(m²·sr·μm)。163802次温度查询通过；220–1200 K对16384区间独立积分最大相对误差0.785%，保持已有1%门槛。未用降低温度、关闭热源或减少分辨率来换吞吐。

## 精度与历史证据

本轮没有重跑数百个 MODTRAN case。保留官方原始输入、输出和 P20M2 失败；只复用可靠透过率拟合工程参数。拟合锚点最大 tau 绝对残差：SWIR 0.072044、MWIR 0.050017，已超过旧严格0.01门槛，因此严格全域精度明确 PENDING，工程运行 PASS 不代表精度 PASS。

旧 `g1_nearfield/overlap_report.json` 保持原状：SWIR路径热辐亮度最大绝对误差1.0776×10⁻⁹、最大相对误差17.409%；MWIR对应0.0011393 W/(m²·sr·μm)、51.532%。最大绝对误差状态为3 km、0.5 m斜路径、RH85/能见度6 km；最大相对误差可能是另一状态，完整关联保存在新acceptance的 `oldNearfieldErrors`。更细光谱报告也逐项纳入。上述是旧路径分量误差，不直接等于最终图像误差，也未宣称可忽略。

客户实际软件：`NOT_RUN`；真实辐射标定：`NOT_VERIFIED_CALIBRATION`；全域 MODTRAN 独立 holdout、折射、晨昏高精度、完整天空方向精度仍待后续。

## 构建与复核

```powershell
& tools/stage0_build.ps1 -VsBuildDirectory build-p20m3-renderer -QtBuildDirectory build-p20m3-stim
F:\Programs\anaconda3\python.exe tools/p20m3_package_windows.py
F:\Programs\anaconda3\python.exe tools/p20m3_preflight.py
F:\Programs\anaconda3\python.exe tools/p20m3_acceptance.py
```

ARM使用 `tools/p11_rk3588_source_build.ps1 -Mode Build -ConfirmSourceStable -StageId <新的p11阶段ID> -OutputDirectory <新目录>`，VM密码从 `P11_VM_PASSWORD` 读取。`p20m3_deploy.py` 校验新ELF和参数身份，生成新的 `P20_P20M3_*` 隔离目录，不覆盖旧候选。默认保持 `simMode=1` 一包一帧、coarse MWIR320×320/raw、precise SWIR800×800/H264、实际云天气/RH85/能见度6 km。

`p20m3_acceptance.py` 分别汇总运行、帧守恒、数值扫描、背景/故障、性能、精度及标定；传输exit0不直接代表性能通过。所有逐帧超80 ms记录保留在各运行的 `performance_p20m3.json`。延迟是同一Windows机器发端beginNs到接收decodedNs，两端共享steady-clock基准；不宣称显示屏呈现延迟已测。


## 最终执行回执

工程运行 `PASS`，质量 `ENGINEERING_APPROX`，双路60 FPS且每帧≤80 ms性能 `NOT_PASSED`。这里的运行PASS表示合法输入可计算、正式SI成像和协议闭环通过，不表示性能、全域MODTRAN精度或标定通过。未完成项仍包括SWIR冷启动和近距离大目标阶段的延迟；本轮没有把失败描述成外部阻塞。

最终板卡ELF SHA-256：`db4727d4de868d902a0e09fe2f1d1babe2d007aa57aed0b1557a95bcfd9c7ca7`，BuildID `2f68af9505562ec5f25d543708a859fd6d67f4e2`。构建源码manifest SHA-256：`d85950a09f0fc7c38bcd8d4a84f2d27dbacf3ed014c1bce404aae2415c9c5cca`。构建时HEAD仍为开头基线，实际未提交源码通过此manifest绑定，不把基线提交号当作本次修改身份。

Windows构建日志：`build_windows_halfk.log`、`build_qt_hires_final.log`；ARM：`build_arm_halfk/build_receipt.json`；部署：`deployment_v5/receipt.json`，均在本轮证据目录。最终Windows EXE哈希在 `windows_package.json` 和 `acceptance.json`。接收器也使用本轮 `build-p20m3-customer-receiver/release`，不是旧stim目录。

在板卡SSH终端手动启动两路（自动入口无需另手动启动）：

```sh
python3 /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v5/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v5
```

先在Qt STOP并等待排空，再在上述终端按Ctrl+C停止本次拥有的进程并恢复性能策略。切换策略需停止实例后修改该隔离实例 `Config/HwaSimIRRuntime.ini` 的 `[M1NirMwirPhysics] AtmospherePolicy=StrictModtran`，重新启动；StrictModtran恢复严格表域/身份要求，不能期望全域工程覆盖。切回 `EngineeringContinuous` 恢复本候选工程模型。Qt预检配置中的同名策略也需同步，不能只切换renderer。

回滚先停止新候选，然后使用保留的旧根目录：

```sh
python3 /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v5/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm
```

旧候选需使用其配套旧配置，保留原40 FPS入口与原数据限制；不要让两个候选同时占用同一DDS身份。

### 最终60 Hz实测

最终运行目录为 `logs/p20/runs/p20m3_v5_hires_{native,csv1,csv2,csv3}`。每项均保持MWIR320×320、SWIR800×800和原天气/编码要求，无丢帧、Latest覆盖或降分辨率。全部输入/执行/渲染/解码/保存计数相等，STOP后队列为0。

| 场景 | 每路帧数 | coarse解码FPS | precise解码FPS | coarse最大延迟ms / 超80帧 | precise最大延迟ms / 超80帧 |
|---|---:|---:|---:|---:|---:|
| 原生71秒，全窗口 | 4261 | 60.034210 | 60.077259 | 60.3746 / 0 | 151.3332 / 23 |
| 原生稳定61秒 | 3661 | 59.997221 | 59.993870 | 18.1231 / 0 | 61.4128 / 0 |
| CSV1 | 3940 | 59.995723 | 59.565298 | 39.8577 / 0 | 584.6264 / 98 |
| CSV2 | 319 | 60.242899 | 60.435897 | 35.5724 / 0 | 89.9011 / 4 |
| CSV3 | 2902 | 60.022362 | 59.504850 | 36.2879 / 0 | 495.9160 / 56 |

全窗口FPS可能因首帧启动延迟、随后排空而高于输入率，不能因此宣称稳定达到60。采用N-1帧间隔；稳定窗口小于60的值没有向上四舍五入成PASS。原生输入Hz为59.999859719 / 59.999980056，稳定render为59.998203041 / 60.001215594 FPS。各路P50/P95/P99/max、逐帧超80 ms清单、编码/输出阶段、队列斜率和STOP排空耗时完整保存在各 `performance_p20m3.json`。

原生稳定段队列斜率均0；CSV1/3 precise队列最大14/13帧，整段OLS斜率0.01520/0.01319帧每秒，轨迹末段存在积压后排空，不能判实时通过。原生precise全窗口延迟P50/P95/P99为43.1953/54.3489/56.5763 ms；最大151.3332 ms独立保留，不以P95替代最大门槛。

本轮实际性能修改包括SI分支预热、0.5 K Planck查表和Qt专用发送线程。旧GUI线程发送曾停顿971 ms；普通sleep版本又出现Windows约15 ms计时粒度。最终使用动态绑定的高分辨率waitable timer和累计deadline，原生双路最大输入间隔17.2999/17.5131 ms，超过25 ms间隔均0。发送线程只消费冻结协议包，不读取UI；停止/析构先取消并join。与旧71秒场景逐条比较8522个包的规范化业务摘要完全相同（`scheduler_payload_equivalence.json`），原仿真时间没有改写。未进行100 Hz测试。

SWIR近场大目标阶段仍出现较长doFrame和积压。现有阶段计时不能把全部doFrame都称为GPU耗时；首帧还包含物理/材质更新与Shader输入提交。优化后未达标结果全部保留，候选可运行但不承诺双路60FPS/80ms合同已满足。

### 实际图像、背景和故障证据

板卡最终v5双路实际GPU抓取与生命周期测试：`logs/p20/runs/p20m3_v5_lifecycle_capture`，每路180帧；各路 `board/linear.pfm` 是AGC前线性SI帧，`board/linear_rgb8.png` 是可查看图像。两路均有限、实际显示目标，日志确认Mali。固定环境下每路180个背景记录只有1个分量快照；无目标、新增、隐藏、删除、恢复不改变背景。三CSV正常视频在各运行目录中保存。

三CSV每路3940/319/2902帧，环境记录逐帧齐全，有效目标记录3921/300/2883；前19行ViewValid=false单独分类。所有工程评估的 `forcedBlackDueToAtmosphereMiss=0`。完整目标键错误仍拒绝，绝不按ID静默替换类型；Windows错误引用测试31–60帧无效，61帧恢复匹配。

最终Qt专用线程另完成两轮RESET/INIT/ACK/START/STOP，证据 `windows_worker_reset`；旧ordinary输入兼容冒烟为 `windows_ordinary_final`。原单路界面两轮测试 `windows_single_reset_fixed` 仍保留。数值一键入口已实际运行：

```powershell
& tools/p20m3_verify.ps1 -OutputDirectory logs/p20m3/your_new_verify
```

其 `final_verify/verification.json` PASS仅声明数值扫描与Planck插值，不冒充性能或独立物理标定。总验收入口为 `tools/p20m3_acceptance.py`，输出 `logs/p20m3/20261021_engineering/acceptance.json`。
