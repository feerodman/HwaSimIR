# HwaSimIR P13 前置复核：P12 的有效成果、输入域与成像证据

复核日期：2026-09-18。依据用户上传的 P12 ZIP 与 GitHub main 可读报告/源码。
本文件是离线证据复核，不是重新运行 Windows 或 RK3588 的实机报告。

## 1. 包身份与复核范围

- 文件：`HwaSimIR_P12_Delivery.zip`
- 本次实际计算 SHA-256：`f112b2fd445915b5b654149e169cef0149685b87f405bf39b46763de094b16a8`，与用户 sidecar 一致。
- ZIP 成员数：1957。
- 包内标准视频后缀 `.mp4/.mkv/.avi/.h264` 成员数：0。不能由此断言当时没有录像；只能确认交付包没有这些视频文件。
- 本次读取：四份 P12 文档、final_status、正式 LUT、发送端配置及源码、模型请求/日志/图片、天气图片/审计、性能 JSON 与 board.log。
- 查看图片：模型 received contact sheet、天气 received contact sheet。未逐帧观看不存在于包中的录像。
- 原始 `DataDrivenTestQT/1.txt` 未装入本次运行包；GitHub 返回了其表头和开头数据。本次没有取得原文件全量本地字节，不能声称已计算其完整轨迹极值或本次原文件 SHA。

除另有说明，下面相对路径均以 ZIP 内 `HwaSimIR_P12_Delivery/` 为根。

## 2. 普通启动为什么变成了约 1000 m 高度

证据：

- `runtime/DataDrivenTestQT/NetworkConfig.ini`
- `runtime/DataDrivenTestQT/ordinary_demo_1km.txt.json`
- `source/DataDrivenTestQT/mainwindow.cpp::loadNetworkConfig`

配置：

```ini
[Demo]
InputFile=ordinary_demo_1km.txt
TargetType=0x11
envSky=1
UtcHour=3.5
```

`ordinary_demo_1km.txt.json` 记录 observer/target altitude 都为 1000 m，declared_los_m 为 600。文件名中的“1km”不能被当作实际斜距。本轮原始时间、姿态和速度列被用于受控 UI 回归，但几何已经由派生文件替换。sidecar 记载 rows=4318、original_modified=false，并记载了当时 source SHA；这是交付 sidecar 的记录，不等于本次重新计算原文件身份。

GitHub `DataDrivenTestQT/1.txt` 开头可见：

- 首个数据时间为 20 ms，后续 30、40、50 ms；
- observer 高度字段为 11000.04785 m；
- target 高度字段为 10000.05371 m；
- 距离字段为 22274.208 m。

该距离是文件字段；运行时斜距必须按程序实际经纬高/坐标转换重新计算。不能从首行推断全文件最大距离，也不能把用户要求的 50 km 当作本文件已经包含 50 km。

## 3. 控制端不是读取 LUT 后动态判断有效域

证据：`source/DataDrivenTestQT/mainwindow.cpp`，`validateFormalAtmosphereCoverage`（GitHub main 本次读取约 1036–1137 行）。

当前代码写死：

| 轴 | 控制端允许值 |
|---|---|
| observer altitude | 1–1000 m |
| target altitude | 1–1000 m |
| 两端高度差 | ≤0.05 m |
| 几何斜距 | 100–2000 m |
| 能见度 | 6–23 km |
| 相对湿度 | 30–85% |
| 太阳天顶角 | 20–70° |

此外，`WeatherCameraInput` 存在时，控制端将覆盖判断 deferred 给显式测试 fixture/渲染器。这种测试路径不能冒充原文件普通入口。

结论：只在 INI 改回 `1.txt` 会被拒绝；只追加 50 km 数据也不会让控制端自动支持高空、不等高和远距离。数据发布与消费者覆盖协议必须一起修。不能通过删除保护、扩大常数或改回 1000 m 几何来冒充支持。

还应统一仿真 UTC：当前该预检取当前 UTC 日期，时刻可来自固定 `Demo/UtcHour`。同一小时在不同日期对应不同太阳角。正式回归需记录完整仿真日期/时刻，并与渲染器保持一致。

## 4. 正式 MODTRAN 表的实际内容

证据：`runtime/HwaSim_IR_Windows/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv`。

表 SHA 与 final_status 一致：`6f22d25d32bc00e753b454560af630d09c2e1a130d53835fa505e27d97f5fd99`。总行数 1724。

本次“完整列”仅指以下五列均非空，不是自动宣布多维查询/插值有效：

```text
tau_up
path_thermal_W_m2_sr_um
direct_solar_irradiance_at_target_W_m2_um
downward_sky_diffuse_irradiance_W_m2_um
los_path_scattering_radiance_W_m2_sr_um
```

| 子集 | 行数 | 高度坐标 km | 距离坐标 km | 备注 |
|---|---:|---|---|---|
| SWIR | 192 | 0.001、1 | 0.1、0.5、1、2 | 五列非空；低空受控域 |
| MWIR 新完整列子集 | 192 | 0.001、1 | 0.1、0.5、1、2 | 五列非空；低空受控域 |
| MWIR 旧子集 | 335 | 3、5、10、15、20 | 1、2、5、10、20、35、50 | 太阳/天空/散射等列存在缺项，不能当作当前完整 M1 覆盖 |
| NIR | 1005 | 3、5、10、15、20 | 1、2、5、10、20、35、50 | NIR 兼容数据，不能改名用于 SWIR |

新 SWIR/MWIR 子集的能见度坐标为 6、23 km，SZA 为 20、45、70°；humidity_profile 有 default 与 scaled_mls_surface_rh30/60/85。必须区分 surface humidity profile 标签与某高空位置的局部相对湿度。

上述是轴值并集，不能推出所有笛卡尔组合存在。正式可用性需要查询器检查有效单元、几何约束、单位、波段响应与必需分量。

**结论：不是 MODTRAN 引擎只能到 2 km，而是当前 SWIR/MWIR 完整生产数据及控制端有效域没有覆盖原轨迹和一般性 50 km 需求。**

处理应为：保留合格旧数据，按实际输入及声明的通用大气范围补数据；先小批核对几何/输出列，再扩展。不重复全量跑已有有效点，不用平均透过率做任意距离外推。

## 5. 16/16 模型图没有证明正常物理材质效果

证据：

- `evidence/p12b/asset_views/audit.json`
- `evidence/p12b/asset_views/logs/*_request.json`
- `source/HwaSim_IR/HwaSim_IR/HwaSimIR.cpp`（约 13646 行）

审计声明：`synthetic normal diagnostic for geometry, pose and display only`。

本次遍历 16 个 request JSON，全部 `materialView=2`。源码对应：

```glsl
if (u_p5_material_view != 0) {
    // ...
    if (u_p5_material_view == 2)
        diagnostic = normalize(v_stage5_normal) * .5 + .5;
    // 将诊断数值编码到公共显示窗口
    gl_FragColor = vec4(clamp(diagnostic,0.0,1.0)*diagnostic_window_max,1.0);
    return;
}
```

该 return 位于正常表面热辐射处理之前。所以 16/16 通过表示对应诊断渲染和真实 DDS 解码成功，不代表这 16 张图已测正常材质发射、反射、尾焰或其波段差异。

P13 必须在正式模型兼容性案例中使用正常材质通路并保存有效 shader 状态；UV、法线、材料 ID 只作为旁证，不能替代正常图。现有资产继续用明确的通用合成材料，不声称具体装备红外特征。

## 6. 尾焰不是简单“已经画出来，只是不够亮”

证据：`evidence/p12b/asset_views/logs/*_board.log`。

本次抽取 16 个模型日志中的 49 条 `[Stage5 Plume]`，这些日志行均有：

```text
engineState=1
formalTauReady=0
coreVisible=0
haloVisible=0
```

这说明至少这批诊断案例在相关日志时刻存在正式透过率未就绪且尾焰节点不可见的状态。不能由 `EnableEnginePlume=1`、CPU 已有辐射值或笼统 visiblePlumeCount 推断传感器图像实际含尾焰。

它不等于已经证明最终普通入口也必然失败；这些是诊断案例证据。P13 应先在受控普通热源中复现并对照最终程序，区分输入关闭、数据未就绪、节点未绘制、遮挡、分辨率、后处理与编码因素，而不是直接增益。

P12 报告对尾焰/照明/气动/热惯性的级别为“组件回归 + 保留 P11 图”，并没有交付新的完整尾焰 on/off 双波段视频验证。

## 7. 云雨雪已有新的有效绘制证据，保留成果

证据：`evidence/p12c/weather_cloud/audit.json`、同目录独立 received 图片。

- 12/12 案例；包含两波段云前后、两个实际 cloudId 分别隐藏、雨雪；
- 实際 cloudId 为 `198EE8338358C182` 与 `D73EEA5ECB3EA0B8`；
- cloud target sample 从 1000 m 改到 6500 m 时，场景记录的云进入/离开区间相对目标改变；
- 单云隐藏与 MWIR rain/snow on/off 有线性与 received 像素差；
- 图像中可见云团、雨线和雪状粒子，不能再把 P11 的空视野结论套到 P12。

这些仍是受控 fixture 下的世界空间绘制/遮挡/传输验证，不是一般性 6.5 km 目标大气链已经齐全，也不是实测天气辐射标定，更不是原 `1.txt` 的混合演示已通过。

P13 应保留几何与流式云成果，增加普通输入中的真实视线覆盖与时间连续性，不把固定世界云改为跟目标粘连。

## 8. 性能结果必须按实际版本归属

### 最终 60 秒版本

`evidence/p12d/performance/ordinary-performance-20260918-043110/*/board.log` 记录：

```text
elfSha256=2d5791360307af746e103d0fdb394a003ef72f70c02f25edcfd3cec3ead2d583
```

它与 final_status 的最终 ELF 一致。SWIR 冷启动 3 帧 >80 ms，max 84.159 ms；MWIR 60 s 无 >80 ms。两者稳态同机受理→writer 均无超限。

### 300 秒雪天不是最终同一个 ELF

`evidence/p12d/performance/MWIR_300s_Snow/board.log` 记录：

```text
elfSha256=99f85b7052b43d577acf87493e2eae663d0b66c4dba608ccd2a2ce98a5d13a1e
sourceIdentity=...-dirty-p12d-async-writer
```

此轮 18000 帧、冷启动 46 帧 >80 ms、max 282.960 ms，Qt paint max gap 119.649 ms，但不能把它直接归为最终 `2d579...` 已证实的性能失败；同样不能用后面的 60 s 通过证明最终长测也通过。应将旧轮如实保留，补最终候选的 300 s 普通天气测试。

### STOP 与诊断采集

报告记录控制响应约 0.12–0.13 ms，但 DDS total drain 约 5 s、完整停止约 10 s。这些不是同一指标。排查是否等待固定 timeout/ACK；不通过提前杀进程丢录像来优化。

PFM 异步 CPU writer 已减小开销，但渲染线程仍有约 44–46 ms GPU wait/readback、约 6 ms CPU 拷贝。普通运行与证据采集必须继续分开统计。不同窗口的 decodedFrames 与 Qt paintFrames 不能直接相减判定掉帧。

## 9. “很多数据报错”中存在可定位的解析噪声

证据：`source/DataDrivenTestQT/mainwindow.cpp::readData`（约 1780–1860 行），以及最终 SWIR 60 s 的 `ordinary_ui/sender.err.log`。

代码只在 `list1.length()==63 && index!=0` 时读取数据，其余情况都打印：

```text
OpaPg data error,the line number is ...
```

因此表头 index=0 和末尾空行也进入“error”分支。本次最终日志中确实找到 `line number is 0` 和 `line number is 22001`。它们不等于 GPU 图像错误。

另一个需审计点：readData 未保留第一列 Time(ms)，`step()` 按每次一行递增，并以 `1/m_inputHz` 增长自身时间。原文件开头是 10 ms 间隔，60 Hz 逐行回放则有不同的播放时间尺度。须明确源时间、发送时间、仿真时间和视频时间，不能用盲目改 FPS 或跳行遮掩。

这只能解释对应解析消息，不能把所有 GL、解码、数据覆盖或像素校验错误都归类为噪声。

## 10. 给 P13 的优先级

1. 原 `1.txt` 身份/全文件输入需求分析；补齐可验证通用大气覆盖并取消控制端写死试验域，以元数据/实际查询结果取代常数。
2. 原文件普通 DDS 入口运行；正常 shader，不借用诊断视角或受控几何替代原输入。
3. 普通材质图像与通用热源的真实绘制/组合对照；保留 P12 云雨雪/玻璃成果。
4. 混合场景独立图片和真实接收端 MP4；包内必须真正携带可播放视频。
5. 最终版本冷启动、长测、STOP、UI 点击、解析与部署回归。

真实材料/传感器标定仍单独为 NOT_VERIFIED_CALIBRATION。没有实测真值时，不把工程一致性、合成材料或诊断截图称为真实装备特征验证。
