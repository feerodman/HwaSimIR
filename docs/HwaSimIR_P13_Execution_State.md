# P13 最终执行状态

状态：`ENGINEERING_PASS`。真实标定状态独立保持 `NOT_VERIFIED_CALIBRATION`。

本轮已在 Windows 控制端、RK3588 板端和构建虚拟机上完成实现、构建、部署、普通 DDS 回放、媒体完整性检查和性能长测。原始 `DataDrivenTestQT/1.txt` 未修改、未改名、未派生替代；其 SHA-256 仍为 `f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901`。

## 最终运行身份

| 项目 | 最终身份 |
|---|---|
| RK3588 ELF SHA-256 | `779e7431228326789aa5daffea14ddcef7089bb23686882a362db764ee699765` |
| ELF build-id | `ce57610d48d9065e2e126b10d587e71e310b67c0` |
| 板端配置 manifest SHA-256 | `6dc22e3f9e3e84e5a102efb46272028d29b194af98abdeeba3d0940c11d9d2ff` |
| Runtime INI SHA-256 | `bbe2fad9fa9fffad9876bda8b1a106f986cad975f7812fbfdd254bdfedd05d67` |
| 正式 LUT SHA-256 | `de72d333dfab19989ba56856075a7d95ea5d9305f84bc1ab2a3ef9e5a97464be` |
| 共享覆盖 manifest SHA-256 | `be2cf0d2e7f89827b44ba0952c89dceb3c040b13c28881ab92ebb7fef4c752dc` |

## 最终同一 ELF/配置实测

| 用例 | 输入 | 波段/天气 | 接受/录像帧 | MP4 | 工程结果 |
|---|---|---|---:|---:|---|
| `final13_original_1_SWIR_Clear` | 原始 `1.txt` | SWIR/晴 | 4318/4318 | 71.950 s, 800×800 H.264 | PASS |
| `final13_original_1_MWIR_Snow` | 原始 `1.txt` | MWIR/雪 | 4318/4318 | 71.964 s, 800×800 H.264 | PASS |
| `final13_MWIR_Snow_300s` | 性能专用 18000 行 fixture | MWIR/雪 | 18000/18000 | 300.002 s, 800×800 H.264 | PASS |

普通运行均为 DDS-only、严格 `outerPlatID=1001/sensorID=2`、自动发现 `VideoStatus`，没有 UDP/TCP 大矩阵重跑，没有跳过已接受输入、复用旧帧或降低分辨率。三次最终性能运行均未启用 PFM/线性帧诊断。

## 已关闭的工程项

- 原始 `1.txt` 的表头/空行误报、全文件数值校验和逐行 60 Hz 发送已修复；一行接受输入对应一条 DDS 状态，不以跳行凑帧率。
- 控制端和板端共同绑定正式 LUT 与覆盖 manifest；缺覆盖时 fail-closed，不夹距、不将缺失填零、不把缺失透过率改成 1。
- SWIR/MWIR 的高空、不等高、2.4–50 km 通用网格已由受许可 MODTRAN 5.2.1 实际输出增量生成；旧 1724 行按字节前缀保留，NIR 1005 行不变。
- 正常材质验证使用 `P5MaterialView=0` 和正式 M1 实际运行；P12 的 `materialView=2` 只保留为法线诊断证据。
- 尾焰先精确复现 P12 的 `formalTauReady=0/coreVisible=0/haloVisible=0`，再关闭数据身份、初始化、缓存、draw、遮挡和合成问题；通用受控热源 on/off 已证明局部像素贡献。
- 普通轨迹的晴、云/能见度、雨、雪组合录像与图片已保留；云为世界坐标天气体，不绑定到目标。
- 最终 SWIR 冷启动 `accepted→writerSubmit` 超 80 ms 为 0；最终三次测试的冷启动和稳态该指标均为 0 次超限。
- STOP 控制响应、渲染排空、DDS 总排空、末帧和录像 mux 完成分别记录；完整退出约 10 s，没有提前杀进程。
- 固定映射与 AGC 的 raw PFM 诊断已独立采集，且相同序号原始 PFM 哈希一致；诊断数据不混入性能结论。

## 仍开放或有意限定的事项

- `NOT_VERIFIED_CALIBRATION`：工程链路通过不等于真实传感器、装备或大气绝对辐射标定。
- 50 km 是独立的通用高空不等高覆盖边界；原始 `1.txt` 的真实视线只到 22.269183094 km，不能声称原轨迹到过 50 km。
- 正式 P13 网格的能见度轴当前为 6 km；其他能见度不是本轮正式声明覆盖，超界查询会失败而不是夹值。
- Windows Qt 实际 paint 事件存在少量大于 80 ms 的间隔；产品序列、60 Hz 提交和录像连续性均通过，但该 UI 展示指标与板端硬门槛分开保留为开放观察项。
- 通用合成材料和通用热源仅证明工程因果与绑定，不代表具体装备红外特征。

