# 本机传感器资料实际核验（P6B）

只读核验目录 `D:\Presagis\Suite22\Ondulus_IR_22_0` 和 `D:\Presagis\Suite22\Vega_Prime_22_0`。机器清单 [HwaSimIR_Sensor_Resources_Readiness.json](HwaSimIR_Sensor_Resources_Readiness.json) 逐文件给出 42 个实际路径、完整 SHA-256、字节数、格式、单位、用途、出处及读取/实验/分发状态。复核命令：`python tools/p6b_sensor_inventory.py`。本次依据 HTML、SDK 头文件、示例 UI 和实际文本数据核验，无反编译、无许可证密钥访问。

| 项目 | 实际证据位置（相对 Ondulus 根目录，另标者除外） | 已核实含义 | 可用性与缺口 |
|---|---|---|---|
| SRF | `docs/help/Spectral_Response_subsystem.html`；`include/Ondulus/IRSensorConfig/SpectralResponseSystem.h` | 归一化相对能量响应；光子型响应可含波长与 QE 关系，常数 QE 还可能在后段应用 | 可读取定义；没有找到可归属某普通相机的实测 SRF，不能把归一化曲线直接当绝对 QE |
| QE | `data/IIT/QE_GenII_*.txt`、`QE_GenIII_*.txt`，共 6 文件 | 波长头标为 microns；纵轴头标为百分数；数值主要为 0–0.3 | 是像增强器阴极示例。头标与数值习惯存在歧义，未猜测归一化；不能充当普通 CMOS 校准 |
| 常数 QE | `include/Ondulus/IRSensorConfig/SensorConfigurationSystem.h` | Photon detector API 明确 0–1 | 可作参数接口参考，数值需真实器件资料或明确人工指定 |
| 光学透过率 | `docs/help/Optics_Transmission_subsystem.html`；对应 SDK 头文件 | 默认常量 1，允许 API 替换，输出为 0–1 | 未找到特定镜头/滤片实测曲线；大气 transmittance 与镜头透过不能混用 |
| MTF | `data/MTF/MTF_f{20,50,100}_N{8,15,30}.txt`；`IIT_MTF_f50.txt` | 10 个文本文件，cycles/mm，MTF 归一化无量纲 | 文件可读；示例不等于指定传感器实测。cycles/pixel = cycles/mm × pitch_mm |
| 曝光 | `include/Ondulus/IRSensorConfig/SensorConfigurationSystem.h` 的 IntegrationTime | API 单位 μs | 人工实验统一秒，显式转换。已有示例数值不是校准证据 |
| 像元 | `samples/IRUI/mainwindow_configurationtab.cpp`；`mainwindow_documentation.cpp`；`include/Ondulus/Core/Dimension.h` | UI 用 μm，API 为带单位 Dimension | 示例 JSON 的序列化单位未直接由可读实现确认；不能把 0.02 按 UI 单位直接解释 |
| 暗电流 | SensorConfigurationSystem.h；`docs/help/Sensor_Noise_subsystem.html` | API 归一化暗电流密度 A/m²；噪声与曝光、面积和状态有关 | 实验使用人工 electron/pixel/s，两者需除电子电荷并乘面积；本轮没有真实器件温度曲线 |
| 读出噪声 | `include/Ondulus/IRSensor/SensorNoiseSystem.h`；Sensor_Noise_subsystem.html | 软件模型及 NETD/NEP 的噪声表征 | 未找到有器件出处的单像元 electron RMS 读出噪声数据；不能把 NETD 直接填入读出电子噪声 |
| 满阱 | SensorConfigurationSystem.h | 光子探测器 WellCapacity 单位 electron | 可读接口，示例配置不代表实测器件 |
| 增益/ADC/黑电平 | `docs/help/Detector_Signal_Transfer_subsystem.html`；SensorConfigurationSystem.h | 光子分支 Counts/electron、ADC 位数；热型分支 V/nW 与 V | 分支含义不同。没有普通相机实测转换增益/黑电平；实验全部人工设定并单独测试裁剪 |
| 配置实例 | `data/configuration/default_LLLTV.json`、`LLLTV_VIS-NIR.json`、`default_NVG.json`、`default_MWIR.json`、`default_LWIR.json` | 软件示例，包含不同传感器及像增强器设置 | 仅核对字段和来源；未覆盖现有生产 profile，未进入实验计算 |
| 云纹理说明 | Vega 根目录 `docs/help/3D_clouds.html`、`3D_cloud_textures.html`；`include/vegaprime/vpEnvCloud3D*.h` | 灰度纹理/透明度用于云精灵；可组合为 3D 外观 | 本机实际打开图示。说明中的 `.inta` 原文件未在指定 Vega 根目录找到；图示只作本机参考，不是输出素材或三维实测切片 |

所有上述安装资料都能读取。单位未解、器件归属不明的曲线暂不作为数值实验输入；其余文档可用于本机接口和单位核对。安装目录中的示例源文件头含针对已授权用户的 sample source 使用许可，但不能据此推出相邻数据、手册插图和产品资源允许再分发。42 项均标为再分发授权未建立，没有复制商业资源到仓库或板卡，也没有公开上传。

项目自身 Weather 纹理另外做了通道核验：`cloud_scattered.png` 是 512×512 LA，约 73.24% 像素全透明；`cloud3d_001/002/014/020.png` 为 1024×1024 LA。001/002/020 的轮廓主要在 alpha；只看灰度会误判为整张矩形噪声。014 无完全为零的 alpha，派生构建需显式边界。详细测量在 `logs/p6b/weather_channels/channels.json`。96³ 派生密度是从仓库现有素材构造的艺术数据，非测量值。

独立实验入口与说明见 [ordinary_sensor_lab](../experiments/ordinary_sensor_lab/README.md)。已用人工参数验证光谱积分、单位、曝光、电荷、噪声、饱和与数字化；未宣称真实器件标定完成。下一阶段仍需有来源、单位和使用权明确的目标普通相机 SRF/QE、镜头/滤片透过率、像元和曝光、暗电流温度曲线、读出噪声、满阱、转换增益、黑电平及 ADC 定义。
