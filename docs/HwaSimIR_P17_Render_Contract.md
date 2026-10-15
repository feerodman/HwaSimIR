# P17 尾焰与显示契约

状态：2026-09-22 候选实现；NOT_VERIFIED_CALIBRATION。构建、数值测试、采集成功均不代替普通入口视觉验收。

## 已确认与待确认

普通渲染器使用原始 4318 行输入、AIM120、MWIR。额外的 0.5 µrad 窄视场诊断仅改变 FOV，明确与原 25 µrad 场景分开。修改前该诊断的 sourceSeq=1200 显示规则椭圆卡片/梳齿，旧 shader 的 32 个周期 billboard、年龄包络与固定排序共同构成这种离散轮廓。profile 的 axialDecay/radialDecay/noise 当时上传但未被外置 shader 消费。

P17 改为每个喷口每层一个保守代理面、局部连续密度、16 步积分；不采样 atlas，消除当前路径的 atlas mip 串色和粒子重生边缘；不再用粒子数量 LOD 改变形态。相位沿用连续协议时间。实例几何保持静态，shader 按喷口局部坐标计算。保守 bounds 扩为 24 个节点局部单位。喷口绑定的原缩放是 `radiusX / 0.4`，连续包络必须保留 0.4 根部单位，不能误当作 1。

白块还有独立显示原因：修正几何后 sourceSeq=1200 raw 峰值为 13416 W/(m² sr µm)，原 MWIR 0..64 窗口、gain=3、Reinhard、gamma=2.2 将大范围压成接近白色。HDR 映射单独验证；旧映射的 before/after 几何证据仍保留，不能把两者混成同映射 A/B。

## 生效表

| 来源 | CPU | uniform / GPU 使用 | 边界 |
|---|---|---|---|
| engineState | 原模型状态与热瞬态 | u_plume_enabled、节点可见性 | 只影响局部喷口尾焰，不整体加热 |
| temperatureK / heatTauSec / coolTauSec | IREnginePlumeModel 温度瞬态 | 绝对源辐亮度进入 u_plume_gray | 未改变配置的温度、时间常数 |
| bandEmissivity | 绝对源的热/环境混合 | 源 RGB，与 alpha 分开 | 工程代理，非实测谱库 |
| opacity / 三个 opacity scale | 有效参考弦不透明度 a0 | `-log(1-a0)` × 密度路径积分 | a0=1 用数值下限 1e-6；现有 core scale=1.1 可使参考弦饱和，未暗改 |
| axialDecay | output.core/haloAxialDecay | u_plume_axial_decay → exp(-a·decay) | 当前真正生效 |
| radialDecay | output.core/haloRadialDecay | u_plume_radial_decay → exp(-r²·decay) | 当前真正生效 |
| noiseScale / noiseStrength | 原值传递 | 平滑平流正弦密度扰动 | 强度上限 0.55；固定时间相位，非每帧随机 |
| radiusRootM / radiusTailM | profile 值 | tail/root 比值、局部径向包络 | 已绑定喷口的实际根半径优先取 nozzle_attachments |
| lengthM | profile 输出仍保留 | 非附件模型节点 scale | 已绑定模型长度优先取附件 coreLength/haloLength，u_plume_length 不参与外置 shader |
| localPos / localDir | profile / 附件挂接 | 当前已配置喷口统一局部 -Y | 附件覆盖位置；任意 localDir 尚未通用化，不能声称全方向 profile 均生效 |
| 双发位置 / RadiusX / RadiusZ | nozzle_attachments / 节点 scale | emitter offset、椭圆截面 | 保持原件、模型缩放和双发间距 |
| display_art Length/RadiusMultiplier | 现有配置 1.12 | 包络长度、半径 | 是既有工程形态参数，不是物理标定；LuminousMultiplier 不作用于正式 SI 源 |

热核心和 halo 都是热气体工程层；1100 K 的 halo 不能称作冷烟。本次不新增冷烟物理层。

## 辐射与覆盖

旧 CPU 输出 `e·max(B(T)-B(Tambient),0)`，下游却直接用于 `tau*S + path` 的 straight-alpha RGB。它是增量而非绝对源，这个契约不一致已由调用链和等温测试确认；但其环境项很小，不能把它说成当前白块的主要成因。

P17 的代理绝对源为 `S=e·B(T)+(1-e)·B(Tambient)`。这里余项是显式声明的各向同性环境代理，不是新增谱数据；真实气体 LTE/散射/非平衡过程仍未标定。密度积分得到 alpha，RGB 不再预乘 alpha；`Iout=(1-alpha)·Ibackground+alpha·(tau·S+Lpath)`。背景已在传感器平面；这样每层合成保持共同 path 项，不会将背景 path 无故挖掉。没有强制 tau=1。

411 项原介质检查及 2 项 1800 K 独立积分参考检查均通过。1/2/8/32 层总光学厚度固定的 double 合成误差容差为 1e-6，等温平衡不增减亮度。SWIR/MWIR 1800 K 参考相对误差分别约 1.69e-5 / 3.43e-5。该测试不等同于 GPU 全视角混合验收。

仍保留 halo70/core71 的分层顺序；单层内沿视线积分并不解决两层之间、两个喷口之间、与目标/云穿插时的任意顺序。代理深度也不是逐采样实体深度。这些视角/遮挡项仍开放，不能称为完整体积合成。

## 固定 HDR 显示

SWIR/MWIR 新默认 HDR preset 使用 `log(1+x)/log(1+xWhite)` 后再应用既有 gamma/polarity。x 沿用原波段窗口和 gain；xWhite 来自现有最高尾焰配置温度 1800 K 的波段 Planck 参考，经相同窗口/gain 转换。白点只是显示参考，不改变模型温度、源辐亮度、透过率和 path；不取每张图的最大值。Game preset 保留，可通过现有 SensorDisplayPreset 选择，所有同波段效果开关共用相同 preset。

代价：中低辐亮度画面会变暗；没有任何 8 bit 映射能无损保留整个热动态范围。raw PFM 与固定映射参数同时交付。几何 A/B 和显示 A/B 分目录/分说明，禁止把不同映射图片当作相同映射比较。

## 天气修改及未完成范围

最终近地 AIM120 实拍进一步确认：Snow 的既有 0.026..0.075 m 半尺寸导致近处
雪粒偏大；短波雨丝呈暗纹，源项完整性仍需修复。当前相机附着与 FOV 参与排布
仍在，不能声称消除了贴屏、FOV 重排或循环边界问题。原高空 Cloudy 画面仍缺少
清晰可辨的体积结构。本轮数值修改和采集成功均不关闭天气外观问题。

世界云温度偏移原来按 Planck 比值缩放整个源项，连同反射和 path 一起缩放。现仅对 `tau·emissivity·ΔB` 修正热项，保留已有短波反射与 path。

雨的硬编码 40..70 长度倍率替换为 1/60 s 参考曝光下的投影速度轨迹；0.72 像素横向加宽按 `原宽/加宽后宽` 补偿覆盖率。参考曝光仍为明确工程假设，尚未接入真实曝光参数；沿运动方向完整曝光积分尚未完成。降水节点仍附着相机，FOV/移动引起的粒子重排和短波散射缺失尚未修复。雪保留原有不同尺度、1.5 m/s 与雨 11 m/s 的速度基准及旋转漂移，但不能据此关闭普通天气外观问题。
