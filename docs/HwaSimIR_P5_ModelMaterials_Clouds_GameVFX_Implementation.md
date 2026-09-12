# P5 通用资产、天气云与游戏喷流实施记录

本轮按飞机游戏画面实施。保留实际文件名、既有机体材料、热区和控制协议；未重制 f35 贴图。测试喷口是生成的普通圆筒，喷流图集是解析函数生成的美术遮罩，没有设备特征拟合。数据与截图位于 `logs/p5`，可复验入口为 `tools/p5_run_case.ps1`。

## 1. 资产到 GPU 的对应关系

实际入口为 `LoadPlatformAssetNode` → `IRSceneMaterialMapper::bindPlatformNode` → 命名采样器 `u_material_id_texture` → 8 槽材料数组 → 正式 shader 的材料查找。材料数据库优先读取 `Config/Materials`。底色纹理与编号纹理分开绑定；底色的亮暗不再决定材料编号。

| 资产目录 | 实际模型 / 底色 / 编号纹理 | XML 编号与材料 | 编号纹理占比 |
|---|---|---|---|
| f22 | `f22.obj` / `f22.rgb` / `f22_mat.tif` | 1 铁、128 涂层、255 铝 | 57.907% / 14.333% / 27.760% |
| aim120 | `AIM120.obj` / `aim120.jpg` / `aim120_mat.tif` | 1 铝、85 涂层、169 玻璃、255 铁 | 66.304% / 2.895% / 0% / 30.801% |
| aim9x | `aim9x.obj` / `TX_AIM9X_Diffuse.png` / `TX_AIM9X_Diffuse_mat.tif` | 1 铝、85 铁、169 涂层、255 玻璃 | 35.114% / 3.010% / 58.085% / 3.791% |

名称分别对应 `BM_METAL-ALUMINIUM`、`BM_METAL-IRON`、`BM_PAINT`、`BM_GLASS`。XML 位于同名 TIFF 后附 `.xml` 的文件中；有 Surface_Substrate 时使用外表面材料。以上百分比是整张编号纹理的占比，不是相机中可见表面的面积或像素占比。

三个编号纹理均为单通道 8 bit，分辨率依次为 1024²、2048²、4096²；未知编号占比为 0，OBJ 未发现缺失 UV 的面角或零长度法线。文件绝对路径、SHA-256、包围盒及顶点/面数见 `logs/p5/assets/audit.json`。没有凭这些统计宣称模型拓扑或材质物性经过现实标定。

导入时没有额外缩放或旋转，诊断节点维持单位缩放与原始局部坐标。OBJ 的 X/Y/Z 包围范围分别为：f22 `±6.749 / ±9.3007 / ±1.998`；aim120 `±0.3102 / ±1.8221 / ±0.3102`；aim9x `[-0.1578,0.1579] / [-1.5086,1.5085] / ±0.1578`。长轴为局部 Y，实际表面朝向以法线图检查。原有 F22 资源条目的 `displayName` 误写成 `F35`，本轮同时更正为 `F22`；类型、OBJ、编号纹理和 XML 仍为原来的 f22 路径，没有替换或重制 f35 资产。

板端 Assimp 对 aim9x 原 OBJ 仍报告两次非简单多边形三角化警告，并完成 7,922 个三角形导入。源文件含带桥接重复顶点的多边形；本轮没有凭显示截图重构其拓扑。此项作为已知资产限制保留，不能将本轮材料绑定通过写成原模型拓扑全面通过。

当前各有 3、4、4 个 XML 槽，不发生 8 槽截断。aim120 的玻璃条目没有被当前编号纹理使用，不能据此认为玻璃已在画面中生效。绑定器现在拒绝超出 8 槽、重复编号和超出 0–255 的编号；不再静默截取前 8 项。编号纹理明确使用 luminance 线性格式、nearest 采样，不使用 mip 插值，匹配容差为半个 8 bit 编号步长。命名采样器消除了依赖 TextureStage 排序恰好成为 `p3d_Texture1` 的不确定性。

| 字段 | 实际作用与限制 |
|---|---|
| XML 编号、UV、ID 纹理 | 决定每个片元查到哪个数组槽；已做实际像素 A/B |
| NIRReflectance / MWIRReflectance | `MaterialBandOptics` 优先；生产表当前为空的项明确回退，不把空值当测量值 |
| Solar Absorptivity | NIR 回退 `1-吸收率`，也参与已有太阳加热模型 |
| Thermal Emissivity | 热辐射及 MWIR 反射回退；经过现有温度、波段和显示链影响像素 |
| Density、Specific Heat、Thickness、Conductivity | 通过既有热惯性状态间接作用，不是直接的颜色旋钮；比热由 w-sec/gm/K 乘 1000 转成 J/kg/K |
| Transmissivity | 参与光学回退的能量剩余项；没有因此实现穿透模型或折射 |
| Roughness | 已上传但当前正式 shader 未实现基于它的粗糙度高光；不能称为已验证 PBR 效果 |

独立人工平板的 TEST_A NIR 反射率 0.2→0.4，接收区灰度中位数 49→100，TEST_B 控制区保持 151。三个真实资产也使用独立的 `Config/Tests/P5/AssetBandOptics_A/B.csv`，只覆盖 `BM_METAL-IRON`，正式材料表没有被改成测试值。最终统计见 `logs/p5/evidence.json`。诊断视图包括底色、UV、法线、编号、原有 NIR/MWIR 和 A/B。相机明示使用 35°、near=0.1 m，仅移动相机适配包围盒，模型没有缩放。

| 资产 | Windows 选中像素 | Mali 选中像素 | 两端编码前 RGB8 中位数 A→B | ROI 内非选中区误改 |
|---|---:|---:|---:|---:|
| f22 | 12150 | 12145 | 51→102 | 0 |
| aim120 | 1893 | 1892 | 51→102 | 0 |
| aim9x | 241 | 239 | 51→102 | 0 |

少量覆盖像素差来自两端光栅化边界；不是参数值差。`rk_material_pixels.png` 为 Mali→MPP→DDS 接收对照，`gpu_rgb8.png` 是 Mali 编码前颜色纹理读出，仍是 RGB8。正式 `MaterialDatabase.csv` 和 `MaterialBandOptics.csv` 与根目录 `materials` 副本的 SHA-256 分别一致，本轮均未改动。

初始 NIR 预览是协议时间对应夜间，日志为 `night_boundary_zero_natural`，模型很暗；这是光照条件，不是槽位失效。复验使用发送器已有 `--utc-hour=6.0` 设置仿真时间，不改 Windows、虚拟机或板卡系统时钟。

## 2. 云为何不明显，以及实际修复

发现两个可复现的实现问题：

1. INIT 阶段地理原点尚未有效时已创建云。首帧建立地理参考后，相同 cloudId 的缓存只更新距离，旧高度仍保留。现更新同一世界网格描述及节点 Z，保留编号、种子、形状和淡入状态；`World3DCloudRebase` 记录旧/新高度。移动加载中心不移动整朵世界云。
2. P4 改为原生深度附件后，体积 shader 仍声明未绑定的 `u_scene_depth_texture`。只有体积云真正进入绘制路径时才出现缺失输入错误。已删除这段未启用的深度采样分支，继续用共享原生深度测试代理表面；没有新增深度纹理附件、读写反馈或拷贝。

收尾的组合测试进一步发现：关闭局部体积云而启用 gamma=2 时，旧独立 EGL 原始缓冲分支允许格式退化成 RGB565，复制到半浮点纹理时产生 GL 0x502。现将所有离屏浮点后处理缓冲都建立在先创建的最终颜色输出/GSG 上，并要求与已验证体积云路径相同的 FBO 格式；仍使用原生深度附件。`rk_release_sheet` 与 `rk_release_gamma2_blend` 实机复验均报告 `actualFloat=1 actualRgbBits=(16,16,16)`，无 GL 错误，常量合成 RGB8 为 [180,180,180]。失败的旧 `rk_cloud_sheet_only` 目录保留，不计入通过或画质证据。

另外将只检查云中心点的裁剪改为保守球与视锥平面测试，避免云中心在画面外但云体覆盖画面时被整朵剔除。加载中心支持 `Camera`；`TrackedTarget` 继续使用完整协议键，目标缺失时回退相机。测试场景显式指定相机加载中心，云仍来自确定性世界网格。

低频日志区分 weather/cloudEnable、候选数、对象池活动数、视锥剔除、零密度/透明度剔除和可见数量预算。`visibleCloudVolumes` 明示只是 selected_for_render，不能单独作为实际像素证据。最大投影直径是估计值，按 800 像素画幅，入云时上限按满画幅估算。实际显示通过开/关体积云接收图、普通遮挡物和视频确认。

调试中曾增加绘制回调计数，隔离异常后移除了该辅助回调；最终不依赖回调计数声称画质通过。最终仍需区分：没有候选、生成但未入视野、深度遮挡、已经合成但对比度低。252 K 等现有天气参数在公共 MWIR 显示范围内可能呈暗云，不把“云必须是白色”当数值标准。

## 3. 尺度、纹理与合成

| 控制 | 正式默认 | 含义 |
|---|---:|---|
| Stage7Weather.CloudWorldSizeM | 30000 m | 远场片状云覆盖边长 |
| CloudTileSizeM | 5000 m | 世界网格吸附步长 |
| **CloudTextureWorldSizeM** | 5000 m | 新增独立纹理重复世界尺度；不改变覆盖边长 |
| CloudBaseAltitudeM / CloudThicknessM | 2500 / 800 m | 片状云层高度与层间厚度参数 |
| Stage7VolumetricCloud.Min/MaxCloudAltitudeM | 2200 / 4200 m | 局部云相对地面参考的高度范围 |
| Min/MaxRadiusXYM、Min/MaxRadiusZM | 见实际 INI | 单朵云的横向/纵向半径，与覆盖边长分开 |
| DensityTextureSize / RaymarchSteps | 见实际 INI | 密度空间采样与沿视线采样质量；不是云尺寸 |

最少修改：远景重复大小用 CloudTextureWorldSizeM；单朵大小用半径范围；覆盖范围仍用 CloudWorldSizeM。`p5_run_case.ps1 -SmallClouds` 是显式的小尺度游戏测试预设：XY 半径 160–300 m、Z 半径 90–160 m、纹理重复 1800 m、光学厚度美术倍率 3。高度与种子不改；正式默认不被该预设覆盖。

本机实际查看了 `ondulus ir 红外图片示例/中波-3d云.png` 与三个 Weather 原图及通道预览。参考图只对照块/层结构、边缘与疏密，没有复制为项目输出，也没有作为数值真值。实际文件：

| 纹理 | 格式、范围 | 当前用途 |
|---|---|---|
| cloud_scattered.png | LA 512²；L=255，A=0–255 | Cloudy 片状云用 alpha；白色 L 本身不含云形 |
| cloud_overcast.png | LA 128²；L=102–181，A=201–255 | Overcast 使用配置指定的亮度遮罩；不能统一按 alpha 阈值处理 |
| cloud_cumulus.png | LA 512²；L=122–245，A=0–255 | 纹理表中存在，但当前天气 profile 没有选择它；局部体积云使用程序生成的 3D R8 密度纹理 |

原图及配置未被替换，遮罩预览在 `logs/p5/assets`。3D 密度目前为四个共享 32³ 模板；没有把参考图片冒充密度体数据。

`Stage7CloudLinearValue` 使片状云与体积云采用背景已有的共同 MWIR 映射。公共场景量是经过相同显示范围缩放、限制的无量纲线性值，RGB16F 只是存储格式，并非原始物理量。云源值随后采用同一线性天气对比度/雾处理；透明 source-over 在线性域合成。gamma 从局部写入处移到 Stage6 最终通道统一应用一次，再做极性。gamma 非 1 时禁止误走 no-op bypass。

恒定层验证：底层 0.2，前层 0.8、alpha=0.5，编码前 RGB8=[127,127,127]；gamma=2 后=[180,180,180]。符合 0.5 与 sqrt(0.5) 的 8 bit 量化。DDS/H.264 接收图有约 1–4 级有损编码/色彩转换差异，接收 PNG 不冒充浮点辐射数据。

片状云与局部云共存。把原有“有任意体积云就整体压暗片状云”的系数替换为局部覆盖淡出：云层高度穿过局部云体时，仅其 XY 投影范围按局部淡入权重减弱片层，最多四个覆盖区。未长期关闭任何一种云以通过验收。当前只精确到代理表面的原生深度测试，不宣称沿整条体积光线精确遮挡，也没有实现全地形阴影。

## 4. 游戏喷流

旧表示由两张相交平面构成，端面/斜角暴露平面轮廓。新的每层固定 32 个软边精灵在 GPU 展开，发射轴与中心始终在模型局部 -Y；每个精灵面向视线，整个喷流不会随相机旋转。共享 256×128 的两个遮罩单元，分别用于发光/烟气；图集由 `p5_generate_sprite_atlas.py` 可重复生成。

使用预分配几何池，每层一次批量绘制；没有逐帧纹理加载或粒子节点分配。生命周期用有效协议时间相对首帧推进，修复“把绝对毫秒时间转成 float 后失去小数导致 alpha 为零”。生产可见层共享最多 256 个有效粒子的预算，单层 8–32 连续 LOD，分数粒子权重及光学厚度归一化缓和 LOD 亮度跳变。32 个池顶点组仍会提交，预算减少的是有效粒子/片元，不把它说成几何 draw call 数减少。

发光和烟气均使用有界 source-over，烟气另有吸收/源色权重，不靠无限加法混合产生白饱和。保留深度测试、关闭透明深度写入。没有改机体材料、热区或协议去掩盖几何问题。`P5LegacyVisuals=1` 只用于受控旧交叉片/旧云曲线对比，不能作为完整 P4 旧二进制的性能基线。

## 5. 部署与运行验收

### 5.1 同步与长测

以下均为最终 ELF 在 RK3588 上经正常 `./run_precise.sh` 启动，Mali 离屏 800×800 → MPP H.264 → DDS → Windows FFmpeg 解码。发送器明确使用 `--sim-mode=1`，OrderedQueue，每个有效输入只生成一份 H.264，未缩小图像、复制旧帧或跳过有效输入。采用明确标注的小云游戏预设、Cloudy、普通喷口、侧视诊断相机、gamma=1，Stage6Diagnostics=false。它们证明这一配置的运行能力，不是所有云尺寸或视角的最坏情况性能保证。

| case | 名义频率 / 输入时段 | 有效输入 | GPU 采集 | DDS 发布 / Windows 接收 | 元数据 / 标注 | 最终队列 / 旧帧 |
|---|---|---:|---:|---:|---:|---:|
| rk_release_sync20 | 20 Hz / 10 s | 199 | 199 | 199 / 199 | 199 / 199 | 0 / 0 |
| rk_release_sync30 | 30 Hz / 10 s | 299 | 299 | 299 / 299 | 299 / 299 | 0 / 0 |
| rk_release_sync60 | 60 Hz / 10 s | 598 | 598 | 598 / 598 | 598 / 598 | 0 / 0 |
| rk_release_60s | 60 Hz / 60 s | 3599 | 3599 | 3599 / 3599 | 3599 / 3599 | 0 / 0 |
| rk_release_600s | 60 Hz / 600 s | 36000 | 36000 | 36000 / 36000 | 36000 / 36000 | 0 / 0 |

按同一有效回合统计，时间窗口边界与 Windows 发送器调度会使输入数不恰好等于名义频率乘时长；本表核对本回合接受的有效输入，不将少于名义乘积直接算作渲染丢帧，也不宣称测得独立的发送器入 DDS 前总数。上述各组 `inputMinusCaptured=0`、`staleFramePublished=0`，视频写入/读取错误、MPP/H.264 解码错误、覆写、溢出和最终 metadata mismatch 均为 0。启动阶段输入队列峰值最多 11 帧，随后恢复；未出现持续积压，最终全部排空。`sourceSeqLag` 采样为 0。DDS 分离的三种消息在途中可能瞬时 pending=1–2，最终全部匹配，不能把消息到达先后差当作丢帧。

10 分钟的 300 个稳定统计窗口平均输入 60.003 Hz、绘制 60.003 Hz，绘制窗口范围 58.864–61.208 Hz。该统计一致排除首/尾跨 INIT/STOP 的窗口，完整日志及包含边界的均值均保留在 `evidence.json`；不是逐帧 P95。RSS 在同一进程运行 93 s / 470 s 时为 370,100 / 371,944 KiB，温度约 41.6–43.5°C；两次快照不足以宣称无限期没有内存泄漏。

每组均有 `visibleCloudVolumes=1`、7 次积分步，候选 10、活动 8。这条日志本身只代表选择。受控的 `rk_accept60` / `rk_volumeoff` 接收图排除喷流条带后，在 43,400 像素 ROI 中有 6,136 像素变化超过 3 灰阶，平均绝对差 2.289；这组比较使用第 5.2 节注明的同一旧版本，证明局部体积云实际参与成像。最终版本的无标注接收图及视频也已逐项检查云体与喷流可见性。差值包含局部片层淡出的变化，不冒充纯体积辐射或独立 GPU 计时。

暂停测试 `rk_pause` 在第 90 个输入后暂停 3 s，恢复时仍是 90，`catchUpBurst=0`；最终 269 输入 / 269 接收，未在无新输入时持续输出旧帧。特效时间基准为协议毫秒时间减去本回合首帧时间：第 120 帧在 20/30/60 Hz 分别约 5.960 / 3.965 / 1.972 s，符合各自采样间隔，没有把帧数当作秒。

同进程的 RESET→INIT→START→STOP 连续两次使用回合编号 1，暴露了 Windows 接收器仅按回合数字变化清理关联集合的问题：旧版本第二轮把新序号当作重复，虽收到 158 帧仍报 mismatch=158。现每个有效 INIT 原子清理视频/元数据/标注集合和本回合序号，同时保留累计传输计数；不改 DDS 格式或生产端拒旧帧机制。`rk_weather_reinit_fixed` 两次各 79 帧、累计 158 帧，第二轮 pending/mismatch 全为 0。无有效业务目标时第一轮 Cloudy 可见一个体积云，第二轮 Clear 候选及可见数变成 0，对象池随后退活；动画新回合的首帧 elapsedSec 恢复为 0。初次失败目录 `rk_weather_reinit` 不算通过。

Windows 接收器此修复后的 SHA-256 为 `cccd3fc51f07be6030c60f1ec47e26f5df5b07ed7117356bb92484c7f8eccd84`，在 `HwaSim_IR_VideoDisplay/x64/Release` 实际使用。之前的单回合长测接收器 SHA 为 `c76c1c08854f26d7608b0303e81c516ed3ae74fd093397c88e8b7c60ec5874d5`；最终 `rk_release_*` 全部使用更新后的接收器。

### 5.2 耗时对照与口径

前三行同为 60 Hz、800×800、10 s、小云预设、侧视，使用修复浮点缓冲分支前的同一 `12f23043…` ELF 做受控对照；末行是最终 `f18a9e91…` ELF 的 600 s 长测，单位 ms：

| 表示 / case | 场景更新 | IR 更新 | 绘制调用 | 回读 | 喷流 CPU 更新 |
|---|---:|---:|---:|---:|---:|
| 旧交叉片和旧云显示 / rk_legacy | 0.413 | 0.901 | 13.916 | 1.252 | 0.194 |
| 新软精灵和公共云显示 / rk_accept60 | 0.420 | 0.898 | 13.839 | 1.251 | 0.199 |
| 新软精灵、关闭局部体积云 / rk_volumeoff | 0.417 | 0.790 | 12.785 | 1.257 | 0.191 |
| 最终新表示 / rk_release_600s | 0.476 | 0.748 | 13.525 | 1.256 | 0.092 |

这些是现有 `[Perf]` 的稳定窗口平均值，绘制调用可能包含驱动等待，不是独立 GPU timestamp query，不能把各列直接累加成端到端时间。小云开启比关闭约多 1.054 ms 的绘制调用时间；新旧表示约差 −0.078 ms，处于运行波动量级，不能宣称明显提速。旧表示是同一个新 ELF 的受控 `P5LegacyVisuals=1` 分支，保留当前原生深度和同步修复；没有冒充整个旧 P4 二进制的历史基准。

最终 10 分钟内 303 个稀疏 `[VideoOutputPerf]` 帧样本：BGR→NV12 平均 9.232、最大 29.539；MPP 平均 1.872、最大 2.974；DDS 发布入队调用平均 0.0156、最大 0.051；输出线程总时间平均 11.497、最大 38.964。DDS writer 的真正写入统计另见 `[DdsVideoTiming]`，入队调用不是网络传输全耗时。未采集所有帧的分位数，故不提供 P95。板卡和 Windows 系统时钟不同，跨主机延迟保持不可用（接收日志为 −1），没有改系统时钟或把 0 ms 当测量值。

质量成本明确保留：最多一个可见局部云，四个共享 32³ 密度模板，测试时 7 步积分；普通喷口两个批次共 64 个池精灵，有透明重叠。生产层另有 8–32 LOD 和 256 有效粒子总预算。没有 GPU overdraw 查询，不能将代理包围范围当实际片元数；小云最大投影直径估计 349.740 px，大云预设达到 800 px 上限且使用 10 步。大云的广泛低对比度覆盖与小云的局部轮廓在 `rk_views.png` 中分别交付。

### 5.3 部署身份与启动

原子部署工具把 ELF、完整 Config、启动脚本与性能策略一起暂存、SHA-256 校验、停止目标进程后切换，并保留可回滚版本；补充了机器专用 NetworkConfig 的保留和 GameVFX 三个必需文件检查。正常启动仍执行 P4 快速校验，不恢复完整 5.72 GB 扫描。没有改板卡时钟、许可证或图形认证，也没有下载板端依赖。

首次为完整配置传输；最终同内容配置通过整树 SHA 校验后建立不可变硬链接快照，再将 ELF、完整 Config 快照和启动工具一起切换。版本文件采用替换后 rename，避免改到共享 inode。不是仅更新 ELF；未来也应通过此部署器替换配置，不能原地修改共享回滚快照。保留板端专用网络配置及回滚版本 `Config.before_20260912-050551`。清单含 7,580 个条目，完整配置约 5.72 GB；最终部署日志及清单位于 `logs/p5/deploy_gamma_fix`。

```text
sourceIdentity = 4d9b3100302a28134c35d5b32d94acfa887fdfd4-dirty
ELF SHA-256 = f18a9e91d0af5f5ecfe222966c4cff1b805d2e9864145fb057fd3d0c541d845f
Build ID = dbcb03a20a118e4202d2c99d117f342bbcaed535
Runtime INI SHA-256 = 1a13d9214da70f82b626902fc9c23a351ec5a3b14538c055247f81b5e5e45dee
Config manifest SHA-256 = 614745354b91065905a683f8dc6eb6c38ec239f9476089bea40fdf5148f3ec7b
run_precise.sh SHA-256 = fb3fefa082027db8bb238b0420940a67db2f30862d4e34078777530087a5caad
performance tool SHA-256 = 6f820002923d5aeb0c4b4af4573c3812e7b9ca7178d5494e71ec51a1acff36b2
```

最终各组正常启动均记录 `RunIntegrity mode=quick result=PASS fullConfigHashScan=0`。`rk_release_sync60` 的 preflight 为 47.390 ms，进程资源就绪 212.820 ms；首个业务视频在启动后 19.795 s 才出现，其中包括测试器延后启动、DDS 建链及 INIT 等待，不能将它宣传成 213 ms 首帧。sysfs 频率设定的即时读回偶发滞后，性能脚本新增最多 10×20 ms 的有限重试，最终仍要求精确读回值，不放宽频率要求。

Windows 主程序/发送器、接收器和 aarch64 构建通过；`p4_mwir_band_radiance_qc.ps1`、`w15_world_cloud_model_check.ps1`、`stage4_hotspot_check.ps1 -Strict` 通过。GLES 首次实绘暴露的两个跨 shader 阶段精度声明不一致已修复，最终板端无 shader 链接错误、未绑定深度 sampler 错误或 GL 0x506。历史失败目录（如 `rk_sync20` 的旧 shader、早期 CPU 读回失败）保留供追溯，不计入本节通过案例。

`p5_acceptance_check.py` 最终对 24 个实测板端 case（含以上最终回归、材料/视角对照及连续 INIT）输出 PASS，同时检查材料 GPU A/B 与 gamma 常量结果。长测结束后再次读取板端 ELF、配置清单、Runtime INI、启动器及性能脚本 SHA，均与本节一致，回滚目录存在；见 `logs/p5/deploy_gamma_fix/post_test_identity.txt`。本地 5.72 GB 临时部署副本已清理，哈希清单、原始日志和板端回滚仍保留。

## 6. 下一阶段普通相机实验的数据盘点

现有 `default_NVG.json`、`default_MWIR.json` 提供波段与显示/MTF/噪声设置，NIR 为 0.7–1.1 µm，MWIR 为 3–5 µm。部分 QE/MTF 路径指向外部 `PRESAGIS_ONDULUS_IR_22_0` 数据目录，路径字符串不能算本项目已有实测曲线；本地 Config 未发现可直接使用且来源明确的 SRF/QE 曲线。JSON 中 DisplayGamma=2.2 也不能直接当作当前 C++ 运行时有效 gamma，实际采用 Runtime INI/环境覆盖及日志。

| 实验阶段 | 需要的数据与单位 | 本轮结论 |
|---|---|---|
| 显示/曝光范围、MTF、噪声分离 | 灰阶/棋盘、有效配置、MTF 空间频率单位，噪声所在域 | 现有开关与测试设施可复用；未同时扩展探测器模型 |
| SRF | wavelength（µm/nm 明确）、无量纲响应、归一化/绝对标定、来源、采样间隔 | 当前矩形波段是明确近似；缺可追溯完整谱响应 |
| 避免重复权重 | SRF 是否含 QE、光学透过、滤光片及电子转换 | 必须随曲线说明，现有外部引用不足以判断 |
| QE / 光子到电子 | QE(λ)，0–1，是否已包含在 SRF | 缺本地可信曲线 |
| 曝光与通量 | 曝光秒、像元尺寸、有效面积、f/#、光学透过与视场约定 | 需要完整普通相机实验定义 |
| 电荷与数字化 | 读出噪声 e− RMS、暗电流 e−/s/像元及温度、满阱 e−、ADC bit、e−/ADU、黑电平 | 缺一套单位和来源一致的数据 |

单个波段平均值已经丢失的光谱细节不能靠末端乘一条 SRF 恢复。后续若需要真实谱加权，应从原始谱或可信分谱重新积分，另立任务；本轮没有实现完整探测器或全地形阴影。

## 7. 图像交付、复验与分项结论

以下为本项目实际渲染输出；参考图没有进入交付画面。每个 case 目录保留 `request.json`、渲染日志、发送器/接收器日志、无标注 `received.png`、实际 DDS Annex-B `received.h264` 及直接封装的 `received.mp4`。视频按测试名义频率封装，不补帧；暂停期间没有码流帧，短视频不是跨主机延迟或停顿时长测量工具。`video_probe.json` 记录尺寸、帧数和时长。常规 60 Hz 短视频为 800×800、360 帧、6 s；4 s 诊断视频只含其实际收到的帧。

| 内容 | 可打开的证据 |
|---|---|
| 最终 RK3588→MPP→DDS 无标注图、6 s 短视频 | [接收图](../logs/p5/rk_release_600s/received.png)、[短视频](../logs/p5/rk_release_600s/received.mp4) |
| 同一类实际接收图加诊断框 | [云、软精灵、普通喷口、片状云标注](../logs/p5/rk_mixed_annotated.png)；标注框是解释用近似范围，未代替原图 |
| 改前/改后与局部体积云关闭 | [RK 对照图](../logs/p5/rk_comparison.png)、[旧表示短视频](../logs/p5/rk_legacy/received.mp4)、[新表示短视频](../logs/p5/rk_accept60/received.mp4) |
| 远场片状云单独 / 局部体积云单独 | [片状云图](../logs/p5/rk_release_sheet/received.png)、[片状云视频](../logs/p5/rk_release_sheet/received.mp4)、[体积云图](../logs/p5/rk_cloud_volume_only/received.png)、[体积云视频](../logs/p5/rk_cloud_volume_only/received.mp4) |
| 大/小预设、前/后遮挡、端面 | [RK 多视角对照](../logs/p5/rk_views.png) |
| 云下/旁/上、视差的补充视图 | [Windows 相机位置对照](../logs/p5/cloud_views.png)、[移动视差视频](../logs/p5/win_cloud_parallax/received.mp4) |
| 旧交叉片与新软精灵的端面/侧面/斜角 | [Windows 几何对照](../logs/p5/plume_comparison.png)、[近/中/远和遮挡](../logs/p5/plume_views.png)、[RK 端面视频](../logs/p5/rk_plume_end/received.mp4) |
| f22 普通底色、UV、法线、编号、NIR/MWIR | [f22 图集](../logs/p5/f22_views.png) |
| aim120 对应八视图 | [aim120 图集](../logs/p5/aim120_views.png) |
| aim9x 对应八视图 | [aim9x 图集](../logs/p5/aim9x_views.png) |
| 三个资产的实际 Mali 材料 A/B | [RK 材料像素对照](../logs/p5/rk_material_pixels.png) |
| 机器可读计数、数值结果及版本 | [验收清单](../logs/p5/acceptance_suite.json)、[完整统计](../logs/p5/evidence.json)、[资产哈希与占比](../logs/p5/assets/audit.json) |

图像结论：材料区域查找和 A/B 生效通过；旧交叉片在严格端面退化为几乎不可见、斜视出现硬平面轮廓，新精灵端面保持软圆形，普通遮挡物可挡住它。没有把严格端面截图误称为已经拍到清晰的字母 X；交叉几何及视角相关伪影由对照证明。单独喷口侧视测试因相机以喷口为中心，尾部靠近画幅左边界；完整尾迹以 45 m 距离的混合场景侧视图检查。此构图限制不通过缩放模型掩盖。

云可见性、世界位置、前后遮挡、共存与公共合成通过。艺术质量仍有限：低步数/低分辨率密度的云团呈柔软斑块，内部层次较简单，部分角度有积分分层；远场遮罩保留较细碎的纹理结构，当前公共 MWIR 范围中的冷云偏暗。相机与背景还有明显的地平线分界。这些画面已实际交付供确认，不能宣称已达到参考软件的艺术质量，也不通过统一抬白掩盖公共显示关系。

| 验收层面 | 结果与边界 |
|---|---|
| 代码/数值 | 材料绑定、GPU A/B、线性混合、单次 gamma、云坐标与缓冲兼容、软精灵固定轴、INIT 关联重置通过；aim9x 原始非简单多边形警告仍单列 |
| 实机运行 | 最终版本的同步频率、含云/特效长测及原子部署按本报告第 5 节和 JSON 核对；不拿初期失败目录或无局部体积云的案例代替 |
| 图像效果 | 材料定位、端面伪影、普通遮挡与云显示已验证；云的艺术层次/对比度仍需结合交付图确认，没有以 FPS 代替画质评价 |

诊断相机明确是 35°，不是原业务传感器视场；普通喷口及云测试物是额外中性几何。数据协议和原业务标注未扩展，因此测试物不在业务目标标注列表中，画质截图关闭了业务框。测试不代表所有目标同时可见、所有大云尺寸下的最坏性能。

复验命令（SSH 密码由调用环境提供，脚本不保存口令；各 case 串行运行）：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/p5_release_check.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/p5_visual_matrix.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/p5_visual_matrix.ps1 -AssetPixels
F:\Programs\anaconda3\python.exe tools/p5_evidence.py
F:\Programs\anaconda3\python.exe tools/p5_package_evidence.py
F:\Programs\anaconda3\python.exe tools/p5_acceptance_check.py
```

正常板端启动仍为 `cd /userdata/HwaSimIR && ./run_precise.sh`。P5 诊断相机和人工材料仅由环境变量/测试入口显式启用，普通启动不进入这些测试场景。本轮未增加完整探测器、全地形阴影或其他波段任务。
