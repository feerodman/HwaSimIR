# SensorWave 配置的实际职责（P6A）

继续使用独立的 default_NVG.json（实际 Band=NIR）和 default_MWIR.json。原始文件完整保存在 SensorWave/Archive/P5，精简后的 Systems 只保留白名单；项目新字段放在 HwaSimIR.SchemaVersion=1 段。NIR 文件名的 NVG 不代表已经实现像增强器。

完整逐字段盘点见 [P6A Profile Field Usage](HwaSimIR_P6A_Profile_Field_Usage.md)。其中列明原始 JSON 的每一个叶节点、是否仍在活动配置中、解析成员、消费者、单位及状态。“解析成功”不代表遗留算法已接通。

## 解析与资源根

IRConfig.cpp 使用已链接的 OpenCV FileStorage JSON 树，按明确完整路径读取。IRJson.h 在前面验证 JSON 数字/布尔类型、重复键、非有限值、尾随数据及嵌套深度；不会在整段字符串中搜索同名字段。当前白名单的对象键采用明文 ASCII，转义形式的键会给出明确不支持错误。字符串值保留 JSON 转义，包含原归档路径使用的可选转义斜线。

根目录由实际可执行程序所在目录确定为 Config/SensorWave，Windows 转成操作系统原生路径供文件读取。每个波段分别记录有效状态、文件、版本、内容 FNV1a64 和失败原因；部署清单另提供全部文件的 SHA-256。目录选择之后不从其他目录补齐无效文件。NIR/MWIR 都通过才将 requiredBands 标为完整有效，其余波段仍独立报告。读取发生在初始化，正常逐帧不读文件。

必需项为宽高及与固定波段一致的光谱范围；项目新段存在时还必须验证版本、波段及显示预设完整性。失败时该波段使用有明确状态的代码缺省，不保留半解析值。VIS/SWIR 遗留范围与固定波段不一致会报告冲突，不在本轮扩展其辐射模型。

## 按字段类别的优先级

| 类别 | 有效优先级与消费 |
|---|---|
| 宽高 | 有效 DDS 初始化接口或显式兼容接口优先；256–4096 范围外才取 profile 缺省，随后经过 IRSensorModel 校验；640×480 不覆盖有效 800×800 |
| FOV / pixel angle | 有效接口的 urad/pixel 优先；无效时由 FOVH 反推缺省，再经过现有 pixel angle 上限校验；FOVV 当前只存储/日志 |
| 光谱范围 | 按 um 解析并与已有固定矩形波段一致性校验；没有重建 SRF/QE 或改变固定波段物理模型 |
| Gamma / Gain / OffsetGray / WhiteHot / Mode | 环境显式覆盖 > 旧 INI 显式键 > 当前波段 HwaSimIR.Display 预设 > 代码缺省 |
| 旧 NETD、焦距、像元间距、F 数、ADC/DisplayBits、BlackHot | 白名单存储/日志；不驱动完整探测器、编码位深或遗留极性算法 |
| 旧 DisplayGamma、GainControlSystem.Automatic、NoiseEffectEnabled、MTF、IntegrationTime、QuamtumEfficiency、WellCapacity | 原样归档；不启用未实现算法，不推断单位 |

实时与初始化的正常输入是 DDS；保留的 UDP/TCP 只属于显式兼容路径。控制协议、目标温度、热区和发动机/气动状态不由显示配置改写。

## 显示迁移与兼容

每个波段包含 Legacy、Game、Auto、Black 独立命名预设。DefaultPreset=Legacy；Gamma=1、Gain=1、OffsetGray=0、WhiteHot=true、Mode=Fixed，保持 P5 默认显示。Game/Auto/Black 需要显式 SensorDisplayPreset 选择；不会因为原 JSON Automatic=true 就启用自动显示。

旧 INI 键保留为注释示例，取消注释即成为显式覆盖。旧环境变量名称也继续有效：SensorInputDisplayGamma、Stage6DisplayGain、Stage6DisplayOffset、Stage6WhiteHot、EnableAGC。冲突由每字段 DisplayEffective 行记录唯一有效来源及 legacyConflict；不叠加应用两套值。

业务源的公共范围缩放仍由既有 M1/Stage5 配置负责。本轮只迁移已知单位的最终通用显示参数；OffsetGray 的单位为 8-bit 灰阶，进入 shader 时除以 255。Gamma 在最终通道只执行一次，之后执行极性，编码固定为 H.264 8 bit。

自动显示使用有效画幅中的 RGB16F 公共线性场景，统计阶段是最终显示增益/偏置之前的源，再明确应用一次固定增益/偏置以估计 AGC 范围。它不再从上一帧已经量化、截断、gamma 处理的 RGB8 逆推。统计值控制下一帧的全局单调映射；不依据目标框单独增强。原参数名 previous_readback 已过时，有效日志以 raw_linear_pre_display 为准。原生深度方案不变，AGC 只在显式启用时增加浮点颜色读回成本。

没有来源一致的本地 SRF/QE、曝光单位及电子噪声数据就记为未提供；IntegrationTime 等遗留数值不直接接入光子/电子模型。原浮点缓冲储存公共缩放后的线性场景值，不能称为原始物理辐亮度。
