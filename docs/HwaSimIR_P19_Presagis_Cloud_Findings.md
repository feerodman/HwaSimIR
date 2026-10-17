# P19：Presagis 云实现核查与优化边界

2026-10-17，依据用户指定的本机 Ondulus IR 22.0、Vega Prime 22.0 安装目录。已读官方随包文档、公开头文件、示例配置；没有运行 Presagis 性能基准，没有把 DLL 内部算法当作已知事实。

## 结论

降低云附件分辨率存在画质风险，不能作为已解决方案。本轮同轨迹双路实测证明能提高帧率，但仍有秒级积压；低分辨率方案出现目标附近误差，补算候选又出现大范围合成回归，全部未接受。板端及三个渲染源码文件恢复稳定 DDS24，实验代码、构建和媒体保留。

Presagis 标准云路径提供的主要启发是改变云的表示与工作量，而非必然降低最终分辨率。本项目已使用同源云纹理，但绘制算法不同。四张 cloud3d_001/002/014/020.png 经 SHA256 核对，与 Vega Prime 安装目录相同；重复更换这些素材不会消除当前体积积分的开销。

## 安装资料直接支持的事实

| 部分 | 已确认实现 | 本机依据 |
|---|---|---|
| 标准 3D 云 | 由多张有透明度的纹理面片 puffs 组成，面片朝向观察者；通过纹理、顶点颜色、方向光和光穿透因子形成体积外观。并非此类标准云每像素必须进行多步体积射线积分。 | [vpEnvCloud3D](D:/Presagis/Suite22/Vega_Prime_22_0/docs/help/vpEnvCloud3D.html)、[3D clouds](D:/Presagis/Suite22/Vega_Prime_22_0/docs/help/3D_clouds.html) |
| 3D 云层 | 用少量云参考生成云实例、共用纹理数组，设置可见范围和边界淡出；文档建议大量相同云使用云层管理。 | [vpEnvCloudLayer3D](D:/Presagis/Suite22/Vega_Prime_22_0/docs/help/vpEnvCloudLayer3D.html) |
| 提交与资源 | CullRecord 使用交错顶点数据；云层头文件提示运行时分配大缓冲会掉帧，性能敏感时使用缓冲池。不能据此宣称每层固定只有一次 draw call。 | [CullRecord.h](D:/Presagis/Suite22/Vega_Prime_22_0/include/vegaprime/vpEnvCloud3DCullRecord.h:32)、[CloudLayer3D.h](D:/Presagis/Suite22/Vega_Prime_22_0/include/vegaprime/vpEnvCloudLayer3D.h:85) |
| 遮挡与穿云 | 不透明物体先绘制，透明云由远到近排序；标准云默认深度测试开、深度写关闭。接近/进入云时还使用透明度与能见度过渡。 | vpEnvCloud3D 文档 |
| 2D 层云 | 以观察者为中心的弯曲网格与纹理覆盖；云层上下表面、覆盖率和入云能见度分别处理。 | [vpEnvCloudLayer2D](D:/Presagis/Suite22/Vega_Prime_22_0/docs/help/vpEnvCloudLayer2D.html) |
| Ondulus 云示例 | 提供 2DLayer、3DLayer、SilverLining 三种示例；3DLayer ACF 引用 Vega Prime EnvCloudLayer3D/EnvCloud3D，示例参考云配置 20 puffs、3 张纹理。 | [Clouds_3DLayer.acf](D:/Presagis/Suite22/Ondulus_IR_22_0/appdata/ondulus_ir_viewer/Clouds_3DLayer.acf:477)、[Sample descriptions](D:/Presagis/Suite22/Ondulus_IR_22_0/docs/help/Sample_descriptions.html) |
| 红外处理边界 | Radiative Transfer 文档描述发射、反射与大气传输；PBPipeline 接口定义透明几何的深度、距离、材质、不透明度以及环境衰减等输入。 | [Radiative Transfer](D:/Presagis/Suite22/Ondulus_IR_22_0/docs/help/Radiative_Transfer_system.html)、[PBPipeline.h](D:/Presagis/Suite22/Ondulus_IR_22_0/include/Ondulus/PB/PBPipeline.h:160) |

以上不足以还原 Ondulus 所有云着色器、精确温度/辐射公式或 SilverLining 的闭源实现，也没有证明其渲染分辨率策略。不能把标准面片云的说明推广为全部 Presagis 云算法。

## 对当前 HwaSimIR 的意义

当前 P17 云把同源透明纹理重建为 96³ 密度场，物理 SI 路径与白色外观路径分别积分。重负载片段日志经常只有一个可见体积云，却覆盖整个 800×800 视口；物理路径当时约8步，外观路径24步。纹理每帧加载计数为0，已有世界网格、视锥筛选和预建纹理。因而不能把“增加缓存、增加剔除、减少很多云实例”当成尚未验证的新根因；目前主要证据指向大面积像素积分和GPU完成等待。

可借鉴的方向，尚非已通过的修复：

1. **保持800×800与完整目标遮挡，减少云的重复积分。** 检查SI与外观层能共享的射线、密度及光照计算，保持各自光学意义；原8步与24步的LOD并不相同，不能直接复用样本后宣称数值等价。
2. **预积分或按视角缓存云的光学结果。** 复用稳定云形的计算，运动、视角、天气或光学参数改变时失效更新；目标与云相交及近距离穿云保留实时计算。此项是对本项目的推导，并非已从上述资料确认的Presagis内部算法。需要专门验证细节、连续运动、失效条件和遮挡。
3. **云层/面片表示作为独立对照。** 可以像标准Vega云一样用云团面片降低积分成本，但会改变近距离视差、轮廓和穿云行为。不能直接替换当前已认可外观，也不能恢复旧的周期billboard缺陷。保持稳定世界位置、非周期布局、软过渡只是必要条件，仍需视觉验收。
4. **保留已有预分配与剔除，按证据优化批处理。** 当前主要是一朵大云占满屏幕，单纯合并提交预计不足以补足20FPS缺口，需实测。

## 本轮测量与撤回

相同原始数据offset3000、600帧/路、60Hz输入，两路raw逐帧核对并保存；不是完整3600帧性能验收。

| 路径 | 精跟FPS | STOP排空ms | 最大积压帧 | 图像结论 |
|---|---:|---:|---:|---|
| 稳定24原实现 | 39.71 | 5156 | 212 | 对照 |
| 候选25，原分辨率开关默认 | 39.06 | 5410 | 218 | 对照差异最大3灰度，未提速 |
| 候选25，云长宽各减半 | 46.82 | 2865 | 138 | 目标附近最大误差96灰度，不接受 |
| 候选25，云长宽各减至1/4 | 50.45 | 1944 | 100 | 目标附近最大误差95灰度，不接受 |
| 候选26，加全分辨率目标区域补算 | 48.32 | 2469 | 121 | 合成回归，整帧MAE21.91，不接受 |

全部候选功能收发完成并不表示性能或视觉通过。候选26合成回归根因未完成定位；不把它作为“全分辨率补算已修好”的证据。没有降低60FPS/80ms目标，没有以丢帧、缩放最终输出或重复末帧消除积压。

证据：[机器可读汇总](../logs/p19/presagis_cloud_review/evidence.json)，[实验源码与撤回回执](../logs/p19/resumed_cloud/experiments26/withdrawal.json)，[半分辨率目标局部对照](../logs/p19/runs/cloud25_half_raw/max_error_crop.png)，各用例的output.mp4、received.gray8、performance.json与visual_raw_comparison.json。当前性能仍为UNRESOLVED，不宣称P19全部完成。
