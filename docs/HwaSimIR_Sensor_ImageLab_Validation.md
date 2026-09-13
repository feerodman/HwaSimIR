# P6C 独立二维传感器实验验收

本报告只验收 `experiments/ordinary_sensor_lab`。公共显示修正在另一个报告中验收，独立实验没有加载业务模型、生产 PFM 或视频，没有向 SensorWave 或生产启动器写入参数。基线源码为 `7a55fb545785a01951ad0c0af53ea263d5374206`，实施前快照见 `logs/p6c/baseline/`。

## 实际新增功能

复用原单像元单位/曲线/光电子核心，加入 NIR/MWIR 人工光谱范围、同一响应下的两个完整光谱基底、二维均匀/阶梯/边缘/球形明暗/等能不同谱图案、曝光内时间积分、图块计算、人工离散 PSF、固定 PRNU/暗信号差异、逐帧独立光/暗 shot 与读出噪声、满阱及数字化。固定图案每回合只生成一次；图块逆序不改变像元或随机图案。

[运行与单位说明](../experiments/ordinary_sensor_lab/IMAGE_LAB.md) · [整图 CLI](../experiments/ordinary_sensor_lab/image_cli.py) · [二维参考测试](../experiments/ordinary_sensor_lab/test_image_reference.py)

## 两组实际 800×800 输出

全部计算参数均为人工指定：QE 从 0.15 线性升到 0.75；两条互为镜像的三角谱具有相同带内总能量。像元 5 μm、满光敏面积 pitch²、曝光 10 ms、暗率 20 e/pixel/s、读噪 3 e RMS、满阱 20000 e、0.6 DN/e、黑电平 64 DN、14 位 ADC。PRNU 裁剪前 σ=0.02；DSNU 裁剪前 σ=4 e/pixel/s。3×3 单位和离散 PSF 使用周期边界，不冒充实测 MTF。

| 人工实验 | 两谱总辐照度 W/m² | 两谱单像元期望光电子 | 序列 | 实际生成耗时 |
|---|---|---|---|---|
| NIR | 0.032/0.032 | 12283.24442480/21948.74823449 | 40 帧 / 10 FPS / 4 秒 | 54.77 s（含文件输出） |
| MWIR | 0.003/0.003 | 5097.04302464/9250.18919286 | 40 帧 / 10 FPS / 4 秒 | 57.98 s（含文件输出） |

NIR 首帧满阱饱和像元比例为 10.7784%，ADC 裁剪比例为 0.0000%。人工图中高亮片及 NIR 较强谱区域用于展示满阱截断，不能把期望电子数与截断后的存储电子数混称。
MWIR 首帧满阱饱和像元比例为 0.1666%，ADC 裁剪比例为 0.0000%。人工图中高亮片及 NIR 较强谱区域用于展示满阱截断，不能把期望电子数与截断后的存储电子数混称。

NIR 为 700–1100 nm，MWIR 为 3000–5000 nm，均为人工光子模型，未建立真实器件或热型探测器模型。耗时是本机参考程序整体运行时间，不是业务视频帧时。

各帧原始 DN 保存在 uint16 数组与 PNG 中，未拉伸到 65535。8 位预览只按固定 ADC 全范围缩放；科学色彩图使用独立带单位色标，均不是原始数据。第一帧保留全部中间数组，其余帧保留原始 DN 与预览及逐层统计。

* NIR：[电子层诊断图](../logs/p6c/synthetic_nir_800_final/views/synthetic_layer_views.png)、[层开关对照](../logs/p6c/synthetic_nir_800_final/views/synthetic_layer_comparison.png)、[8 位预览](../logs/p6c/synthetic_nir_800_final/frame_0000/preview_u8.png)、[人工预览短视频](../logs/p6c/synthetic_nir_800_final/views/synthetic_preview.mp4)。
  [期望电子数组](../logs/p6c/synthetic_nir_800_final/frame_0000/expected_photoelectrons.npy)、[暗信号数组](../logs/p6c/synthetic_nir_800_final/frame_0000/expected_dark_electrons.npy)、[带噪电荷数组](../logs/p6c/synthetic_nir_800_final/frame_0000/noisy_charge_electrons.npy)、[原始 14 位 DN](../logs/p6c/synthetic_nir_800_final/frame_0000/raw_dn_uint16.png)、[完整数据清单](../logs/p6c/synthetic_nir_800_final/manifest.json)。
* MWIR：[电子层诊断图](../logs/p6c/synthetic_mwir_800_final/views/synthetic_layer_views.png)、[层开关对照](../logs/p6c/synthetic_mwir_800_final/views/synthetic_layer_comparison.png)、[8 位预览](../logs/p6c/synthetic_mwir_800_final/frame_0000/preview_u8.png)、[人工预览短视频](../logs/p6c/synthetic_mwir_800_final/views/synthetic_preview.mp4)。
  [期望电子数组](../logs/p6c/synthetic_mwir_800_final/frame_0000/expected_photoelectrons.npy)、[暗信号数组](../logs/p6c/synthetic_mwir_800_final/frame_0000/expected_dark_electrons.npy)、[带噪电荷数组](../logs/p6c/synthetic_mwir_800_final/frame_0000/noisy_charge_electrons.npy)、[原始 14 位 DN](../logs/p6c/synthetic_mwir_800_final/frame_0000/raw_dn_uint16.png)、[完整数据清单](../logs/p6c/synthetic_mwir_800_final/manifest.json)。

## 数值正确性

原 28 项单像元基准全部通过。新增 34 项二维测试全部通过；下面给出独立数值与统计证据，不能将测试数量当成器件认证。

| 测试 | 实际证据 |
|---|---|
| independent_adaptive_spectral_integral | `{"expected_electrons": 12283.244424804212, "measured_electrons": 12283.244424804214, "quadrature_relative_error_estimate": 1.1102230246251565e-14}` |
| mwir_independent_integral | `{"expected_electrons": 5097.043024636994}` |
| equal_energy_distinct_spectra_remain_distinct | `{"irradiances_W_m2": [0.03200000000000001, 0.032], "electrons": [12283.244424804214, 21948.748234486207]}` |
| noise_mean_six_standard_errors | `{"samples": 122880, "expected": 12283.444424804213, "measured": 12283.319584900088}` |
| noise_variance_statistical_interval | `{"expected": 12292.444424804213, "measured": 12269.054732352894}` |

独立积分使用 SciPy 自适应积分与单独写出的 SI 公式，不调用被测三点 Gauss 积分；时变均匀场另用解析曝光均值。噪声统计使用 120 帧×32×32=122880 个独立观测，均值阈值为 6 标准误差，方差相对偏差阈值为 2.5%。无噪声二维/单像元、曝光线性、单位等价、图块逆序、PSF 直流/总和、满阱/ADC 分离、半向上量化、固定图案和原始码值保存各自单测。

## 运行与图像结果

两组 800×800 完整序列均实际生成，无运行异常。原始数组、PNG 及代码/输入 SHA-256 均记录在 manifest；查看图与预览视频另有 `logs/p6c/sensor_view_manifest.json`。输出目录必须为空，禁止把旧实验帧混入新清单。

图像可见连续灰阶、阶梯、球形明暗、移动亮边、独立小高亮片和等能量不同光谱的两个区域；固定像元差异与逐帧噪声同时存在。MWIR 人工图在固定 ADC 全量程预览中较暗，这是所选人工辐照度产生较少电子，不通过拉伸原始 DN 掩盖。PSF 使边缘平滑，并按声明在边界周期延拓；这不等于真实镜头的边缘行为。

## 增量资料补缺与器件状态

沿用 P6B 的 42 项清单，本轮只追查 5 个现有文档/声明文件，未重新盘点目录。只记录路径、哈希与结论，没有将商业曲线或手册内容复制到实验数据。

| 增量证据文件 | SHA-256 |
|---|---|
| `D:\Presagis\Suite22\Ondulus_IR_22_0\docs\help\Itensifier_Configuration_subsystem.html` | `e21c612f69c4df1f9c5c033145edd1f5990c78ee4f50c1051c57d6752d6b207f` |
| `D:\Presagis\Suite22\Ondulus_IR_22_0\docs\help\Itensified_Radiance_subsystem.html` | `84fcaf9be7f8b8a998ecf3ee182d45d91e3ca93949d231e1986b5a4cdc677b61` |
| `D:\Presagis\Suite22\Ondulus_IR_22_0\docs\help\Spectral_Response_subsystem.html` | `63d47a0571d1170b1ee9c066dcd47b5a8aa668ae47d35909ffe8c84d4026d6e6` |
| `D:\Presagis\Suite22\Ondulus_IR_22_0\include\Ondulus\Core\Dimension.h` | `91f1b58c7b7c50500c72396244c10e5f05d796a79566ff0e4676c67d950945ae` |
| `D:\Presagis\Suite22\Ondulus_IR_22_0\samples\IRUI\mainwindow_configurationtab.cpp` | `357e8b1f0e64abee5b11bf489068e894206ed4403350870722e266bf4722b62b` |

新增确认：像增强器文档的谱 QE 输出按峰值归一化；这不解决原始 QE 文本百分数/小数约定，也不能获得普通 CMOS 或 MWIR 器件绝对 QE。像元 UI 明确按 μm 构造和读取，泛型 Dimension 可另选存储单位，因此仍不能确定示例 JSON 序列化单位。SRF 文档说明相对能量响应与 QE×波长有关，另有后段常数 QE；不能不辨定义地再乘光子权重。

[缺项模板](../experiments/ordinary_sensor_lab/device_missing_template.json) 经 [元数据检查器](../experiments/ordinary_sensor_lab/device_readiness.py) 实际运行，报告 19 项待补。检查器不导入器件曲线，也不会因字段齐全就宣布 device_characterized 或 device_validated。当前状态只有 synthetic_reference；documented_example 只是资料来源层级；真实器件工作温度、读出模式和独立实测对照均未建立。

概念参考：[PBRT 4 §5.4](https://pbr-book.org/4ed/Cameras_and_Film/Film_and_Imaging) 的像面辐照度、曝光、像元能量与光子噪声；[EMVA 1288](https://www.emva.org/standards-technology/emva-1288/) 的相机参数表征框架。本实验没有执行完整标准测量或取得认证。

结论：人工模型的数值、文件输出和二维图像实验通过；真实器件验证未完成，生产传感器接入未实施。
