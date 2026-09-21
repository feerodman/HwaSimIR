# P14 特效与背景辐射度合成契约

## 总原则

正式 Stage6 raw 域的 RGB 是矩形带平均谱辐射度，单位统一为 `W/(m² sr µm)`；alpha 是无量纲覆盖/介质不透明度。显示映射发生在 raw 合成之后。任何 display gray、白热/黑热极性和 AGC 都不能反馈到物理辐射度，也不能用来掩盖缺失大气数据。

对 straight-alpha 图元使用 `C_out = alpha*C_src + (1-alpha)*C_dst`。若表示吸收烟，`C_src` 可以低于背景，因此带符号 `On-Off` 可为负；若表示热发射，通常为正。绝对差只用于定位变化，不能证明“变亮”。

## 实际 draw 清单

| draw | 正式 RGB 输入/单位 | alpha | blend/深度/排序 | 大气与显示 |
| --- | --- | --- | --- | --- |
| 普通材料目标 | M1 材料辐射 + 反射太阳，经 LOS 五分量传播后的带平均 SI | 实体覆盖 | opaque，depth test/write；模型正常队列 | raw 后统一固定映射或 AGC；`P5MaterialView=0` |
| 尾焰核心 | CPU Planck 带平均源函数经目标 LOS 后的 SI；`sprite.frag` 不截 0–1 | profile opacity 经纹理/粒子权重转换一次 | straight alpha；depth test on、depth write off；实际 shader priority=100 | 不重叠加经验雾；不使用显示 gain；可产生正辐射差 |
| 尾焰烟/halo | 相同 SI 源函数，可低于局部背景 | 同上 | 同上；透明队列 | 冷源允许产生负带符号差，不强制白色 |
| 世界云 | SWIR 反射或 MWIR 有效温度/发射率形成的 SI 环境代理 | 云密度/光学覆盖 | 世界空间，稳定 ID、对象池、距离/屏幕 LOD、frustum cull | 由当前天气/太阳和公共显示窗统一处理，不绑定目标位置 |
| Batch 雨雪 | `u_precip_state.w` 中的传感器平面带平均 SI 源函数；纹理只提供 alpha mask | SGI alpha × edge × particle alpha | priority=100；straight alpha；depth test on、write off；transparent bin 30 | 不从纹理 RGB 推断辐射；雨雪保持 Batch 几何 |
| 天空 | SWIR 天空辐照代理或 MWIR 天空有效温度/发射率转换后的 SI | 不透明背景 | 背景深度/队列 | 与目标同一公共辐射窗；不是目标专用增益 |
| 地面 | 太阳反射 + 热发射形成的 SI 环境代理 | 不透明 | 地面深度 test/write | 与目标同一公共辐射窗；不是现场标定 |

实际尾焰资源安装位置为 `IRGameSpriteBatch::apply`，从 `Config/GameVFX/sprite.vert` 与 `sprite.frag` 读取并以 priority=100 设置。P14 修改的是这条实际路径，不是 HwaSimIR.cpp 中未接管实际 draw 的替代字符串。

## 尾焰根因和修复

历史 P12 日志复现了 `formalTauReady=0 coreVisible=0 haloVisible=0`。数据未就绪时必须保持 fail-closed，不能替换为 `tau=1`。数据与初始化就绪后，实际外置 shader 仍存在第二个问题：正式 SI `u_plume_gray` 被 `clamp(...,0,1)`，随后又叠加 legacy fog/contrast，并让 alpha 参与两次能量衰减。

P14 的正式分支：

- 只做 `max(0, u_plume_gray)` 的数值安全保护，保留 SI 量级；
- alpha 由 profile opacity 与密度权重转换一次；
- 输出 straight-alpha RGB，不预乘；
- 不在 sprite 内再次混合雾、对比度或显示 gray；
- legacy 显示分支仍显式隔离，不能进入正式 raw SI 域。

带符号 raw 验收：固定 ROI `On-Off mean=+0.067446793258 W/(m² sr µm)`；728184 个正样本、0 个负样本、最大 `+0.3505859375`。这证明该受控热源对传感器 raw 的实际贡献为正；它不把通用民用尾气参数声称为某装备实测光谱。

## CPU/GPU 对照

CPU 参考测试通过四个方向性检查：热发射增加、冷吸收烟降低、白热显示保持正视觉极性、黑热显示反转视觉极性。GPU 证据来自实际 priority=100 sprite 路径同一帧 On/Off 的 RGBA16F→PFM 读回，而不是屏幕截图差。

证据：

- `logs/p14/verification/p14_composition_reference_test.log`
- `logs/p14/plume_signed_raw_qc_final4/plume_signed_raw_qc.json`
- `logs/p14/plume_signed_raw_qc_final4/on_minus_off_signed_raw.png`
- `logs/p14/runs/p14_final4_plume_truck_mwir_on_raw/`
- `logs/p14/runs/p14_final4_plume_truck_mwir_off_raw/`

## 标定边界

材料、温度、发射率、反射率、天空/地面/云有效温度均为通用工程假设；未做设备、场地和传感器联合标定。状态保持 `NOT_VERIFIED_CALIBRATION`。

