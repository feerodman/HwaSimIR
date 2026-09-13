# P6B 实施与验收

本页随实机回归更新。基线为 `e18927e8e68cb17ea0a898efff8773301ae704fb`，实施前已有的三个未跟踪文件已存入 `logs/p6b/baseline/preexisting_untracked.zip`，源码/配置哈希见 `logs/p6b/baseline/source.json`。原有 `sensor_lab.py`、`synthetic_visible.json` 保持原字节。本轮未提交 Git，不覆盖用户已有修改。

最终已将 INI 的天气选择设为 `AppearancePreset=GameWorld`，新素材进入正常运行；两个波段 JSON 的 `DefaultPreset=Legacy` 均保持原值。正常验收没有设置 `P6Scene/P6ExistingTargets`，没有注入 35° 相机；发送器使用现有协议输入，800×800 与 200 μrad 像元角对应约 9.167° 视场。验收相机从生成的公共云世界中选取视角，没有改动云描述来凑双云。双云是同屏预算，世界仍有更多候选。

## 生效范围

| 项目 | 正常运行 | 测试入口或限制 |
|---|---|---|
| 新云素材、云形、世界坐标 | `IRWorldCloudStreaming` 生成候选；Weather 预设选择外观；同屏预算 2 | 固定 P6 布景仍独立保留，不作为正常世界证据 |
| 公共天气世界 | 固定 WGS84 ENU 原点、网格、种子与显式模板清单 | 区域切平面天气，非全球曲面/地形 AGL 云层；静态云，不实现风场动画 |
| 显示 | 两个独立波段 JSON；原默认 Legacy 不变；Game/Auto 显式选择 | PFM 全帧读取仅显式诊断；不是有物理谱单位的辐亮度 |
| 自动统计 | GPU 64×64 分层像素抽样，5 Hz 更新；单次 gamma、全画面统一映射 | 抽样百分位不保证等于任意全图百分位；非业务目标框增强 |
| 普通平板、灰阶/球/高亮测试片 | 无默认注入 | 普通测试入口，只核验深度与显示；不改变业务温度或相机协议 |
| 传感器实验 | 独立 Python CLI，不链接渲染业务模块 | 全部人工光谱和参数；不等于真实器件标定 |
| 已有界面 FPS、合并数据页、喷口挂接 | 保留 P6A 实现；正常世界＋现有资产尾焰已做 60 秒与 10 分钟验证 | 另有固定 P6 场景 UI 缩窗回归，单独标记 |

## 云世界与素材

公共原点为纬度 40°、经度 116°、椭球高度 0 m，网格 2500 m，种子 12345。世界网格与各进程首条平台输入无关；首条实际输入只确定本进程渲染 ENU 坐标，再做世界/局部双向变换。位置和半径在 CPU 描述中为双精度。`World.Animation=Static` 明确云状态不依赖操作系统时间；两个实例不需要校时。云层高度是公共切平面 Z，不等于任意地形上空的海拔或离地高度。

`Config/Weather/world_cloud_game.json` 管几何/外观，INI 管预设选择、加载/卸载与可见预算。`IRWorldCloudStreaming` 原有稳定描述决定 cloudId、位置、尺寸、密度、旋转和模板索引。资源清单使用明确 T0–T3，顺序与键属于预设身份；不按目录遍历、可见列表或池槽选材。模板清单和源图 SHA-256 写入派生清单及部署清单；运行只读取明确文件并检查尺寸/内容校验值，不恢复全目录扫描。

使用仓库 `cloud3d_001/002/014/020.png` 的实际 alpha 轮廓和灰度，离线构建四个 96³ 艺术密度体（每个 RGBA 3,538,944 字节）。去掉旧构建中的周期深度波瓣，用固定种子低频三维噪声形成不规则内部层次；不是把二维图片误当实测体素。旧构建同样缓存为 `_p6a.rgba`，供相同世界/相机下对照。普通启动不运行 Python 构建。

片云覆盖 30 km、纹理周期 9 km、平面高度 2500 m；单个局部云 XY 半径范围 350–1000 m、Z 半径 180–500 m，互相独立。片云采 alpha，mipmap 线性过滤，掠视角平滑淡出；世界 UV 固定，局部坐标原点变化不平移纹理。局部云保持原生场景深度附件与预算剔除，使用有限边界内中点积分，近/中/远 10/8/8 步。背景上下半球在世界方向的地平带平滑过渡，片云、体积云、背景在线性域合成后统一显示。

条带定位：旧周期深度构建和低步数相位共同产生结构条纹；第一次尝试的屏幕梯度抖动又引入斜向颗粒，已拒绝。最终采用平滑非周期密度和中点采样。编码前 RGB8 已可观察这些差异，因此不能全部归因 MPP；同帧 DDS 解码另测量化差异。64 步仅作显式参考，8 步中景对参考的预编码 MAE 为 0.675 灰阶、P99 为 5（旧的 deploy2 测量，最终图另列）。低步数仍是性能/层次折中，局部体积也没有实现完整多重散射或地形阴影。

## 一致性证据

* 独立描述检查使用实际 `IRWorldCloudStreaming`：同配置重复、改变局部原点、逆序加载、跨格返回、改变种子。Windows/aarch64 共享 37 个描述，位置最大差约 `4.55e-13 m`，ID/模板/哈希/静态状态一致。
* 两个真实 Windows 进程，DDS 身份 2001/2 与 2002/2，第二进程晚启动 6 秒且相机位置不同：8 个共享 cloudId 的全部公共字段完全一致；实际收到 480/320 帧，输出守恒。
* 正常协议相机跨网格返回：40 个不同 cloudId，55 次激活，重复描述逐项不变；与起始固定相机共享的 8 个云一致。
* Windows 与板卡正常渲染日志共享的 8 个描述，最大数值差约 `1.14e-13`。压缩图不作逐字节跨平台比较。

可复算文件：`logs/p6b/world_cross_platform.json`、`world_two_release_instances.json`、`world_return_actual.json`、`world_render_release_windows_arm.json`。双实例测试还修复了 DataDrivenTestQT 对其他平台初始化应答不加过滤的问题及初始化平台编号硬编码；没有更改消息结构或控制协议。

## 显示统计定义与验证

`HwaSimIR.Display.Presets.Auto.Statistics` 的 `Method/Size/UpdateHz/LowPercentile/HighPercentile/SmoothingAlpha` 按明确 JSON 路径解析、存储至 profile、应用到渲染器统计与平滑。JSON Size 范围 16–128；实际默认 64。旧 INI/环境覆写仍兼容，日志给出唯一最终来源；已有 INI 统计重复值被注释，避免掩盖 JSON。有效接口 800×800 优先，profile 中 640×480 不覆盖有效接口。

样本为 `floor((i+0.5)*W/N), floor((j+0.5)*H/N)` 的原场景像素，用 `texelFetch` 读取，无平均、无 mipmap、无 UI。64² 共 4096 点，对公共显示域中裁剪后的强度构建 256 桶直方图，取 2%/98% 分位；5 Hz 更新并平滑，下一帧应用统一映射。统计 GPU pass 也只在到期时启用。两组同帧完整 PFM/样本 PFM 实测逐样本误差为 0，普通图表与双云样本/全图分位相同；这只是这些输入上的结果，不是所有图的等价保证。

显示 shader 保留线性高亮直到 AGC，之后裁剪、gamma 一次，再按极性翻转。完整浮点 PFM 导出和旧全图统计比较都必须显式开启。PFM 标为公共缩放线性值，不能作为独立传感器原型的绝对光谱输入。灰阶、平板、球和小高亮片用相同设置记录线性值、预编码 RGB8、实际 DDS RGB8；数值与耗时结果由 `tools/p6b_evidence.py` 汇总。

本次 MPP 路径将 RGB24 按有限范围转换为 NV12（Y 约为 16–235），实际 Windows DDS/FFmpeg 解码与编码前 RGB8 已逐帧比较。当前 H.264 未声明完整 VUI 色彩范围字段，因此结果只覆盖本次 RGB24→MPP→实际接收器链路，不推及 Gray8 直接输入或任意第三方解码器。

## 本机资料与独立实验

完整 42 项路径、SHA-256、单位、适用性和读/实验/分发状态见 [本机资料核验](HwaSimIR_Sensor_Resources_Readiness.md) 及同名 JSON。商业软件目录只读，没有复制资源到仓库或板卡，没有上传；示例许可不被推断成相邻纹理/手册的再分发权。已实际打开本机云图示和项目 Weather alpha，参照图只对照结构。

独立入口：[sensor_cli.py](../experiments/ordinary_sensor_lab/sensor_cli.py)，说明 [README](../experiments/ordinary_sensor_lab/README.md)。新增严格 JSON/CSV 曲线输入和来源/哈希记录；显式波长/响应/曝光/像元单位，拒绝双计 QE/透过率、外推、相对响应冒充绝对效率。实际执行 28 项参考测试，常量人工谱的解析电子数 `1038.286542055684`，计算值 `1038.2865420556839`；同时验证曝光线性、暗电荷、Poisson/Gaussian 噪声、满阱、ADC 与舍入。结果 `logs/p6b/sensor_lab/reference_results.json` 和 `synthetic_csv.json`。这属于人工模型验证；器件实测 SRF/QE、光学透过、噪声温度依赖等仍缺失。

## 实机版本、回归及图像

代码正确性：Windows MSVC 构建与 aarch64 交叉构建通过；共享世界描述、JSON 路径读取、显示数值与原生深度分别验证。所有通过项都同时检查视频、元数据、标注、队列清空和旧帧拒发；保留早期失败日志，没有混入通过项。

以下主要为 MWIR、原尺寸 800×800。FPS 是日志区间平均值再取均值，省略首末活动窗口，不是 GPU 查询或逐帧 P95。暂停/重新初始化项包含主动空闲，故不拿其平均 FPS 评吞吐。冷启动及诊断窗口会低于 60，未宣称每帧硬实时截止期。

| 实测场景 | 接收对应输入数 | 帧率均值 | 输入/视频/元数据/标注守恒 |
|---|---:|---:|---|
| 正常世界＋资产尾焰＋双云，60 秒 | 3600 | 59.992 | 通过 |
| Auto 双云，无 PFM，60 秒 | 3600 | 59.992 | 通过 |
| 正常世界＋资产尾焰＋双云，600 秒，INI 加载中心 | 36000 | 60.002 | 通过 |
| 分离视角，8 秒 | 241 | 30.135 | 通过 |
| 横移/进出视野，24 秒 | 721 | 30.031 | 通过 |
| 云前普通平板遮挡 | 181 | 29.997 | 通过 |
| 20 Hz 暂停＋重初始化 | 100+121 | — | 通过 |
| 30 Hz 暂停＋重初始化 | 150+180 | — | 通过 |
| 60 Hz 暂停＋重初始化 | 301+361 | — | 通过 |
| 默认 MWIR / 默认 Weather，8 秒，显式 PFM 诊断 | 481 | 56.966 | 通过 |
| 默认 NVG / 默认 Weather，6 秒，显式 PFM 诊断 | 180 | 30.182 | 通过 |

加载/剔除日志中的 visible 表示提交绘制选择，并不等于像素贡献。最终版本逐云隐藏的控制对照（cloudId 和其他云保持不变）为：

* near：编码前至少变化 2 灰阶的像素 203098，占 31.73%；DDS 原图也有相应变化。
* far：编码前至少变化 2 灰阶的像素 214158，占 33.46%；DDS 原图也有相应变化。

固定映射图表 Legacy/Game/Black 的反向步数均为 0，预编码预测 MAE 约 0.19–0.30 灰阶。小高亮片线性输入前后完全相同（1.5，1600 像素），编码前从 149 变为 182，非高亮像素差为 0；DDS 均值约从 148.98 变为 181。详见 `hdr_order_comparison.json`。

同一 20 Hz 双云输入、相同低频统计策略、无完整 PFM 诊断：一次抽样读取/转换/直方图平均 **0.516 ms**（4096 点，23 个去重更新）；旧全图参考平均 **56.768 ms**（640000 点，25 个更新）。这是每次统计调用耗时，GPU 抽样绘制计入渲染耗时，不能与每帧摊销数混称。`auto_statistics_update_cost.json` 保存原始摘要；最终 Auto 60 Hz 无 PFM 测试是实时使用资格依据，默认仍是 Legacy。

图像效果：原有周期层纹减轻，云轮廓更加不规则；Auto 下最明显的片云细条通过掠视淡化减轻。固定区域的 9 行低通残差均值由 1.958 降到 0.621 灰阶，P99 由 17.00 降到 2.67；该指标仅辅助定位片云细线，不是通用画质分数。有限步数体积仍偏软，近景内层不是完整散射模拟。俯视分离场景中片云保留，局部云与片云共存；云前平板遮挡经过实际接收图核验。

部署后的默认图也单独交付：Legacy MWIR 画面明显偏暗，NVG 的云与背景对比偏低，不能称为理想画质。依照本轮要求没有暗中更换默认显示，Game/Auto 图明确标为主动选择的预设。两组默认启动短测包含完整 PFM 诊断和冷启动，不能据其均值推断稳态 60 Hz 性能。

四云单独结果：正常同一视角把预算改为 4 仍只选到 2 朵，不能据此宣布正常世界四云通过。另一个固定 P6 测试入口短测实际选择 4 朵，约 59.91 FPS、10 秒输出守恒，但不是正常世界四云长测与画质验收。本轮发布双云预算，不发布四云质量保证。

失败与边界：早期屏幕相位造成斜向颗粒，已拒绝；一次 GLES 矩阵精度不匹配使该组图形验收失败，已修复；UV 高精度单独未消除片云细线，真正有效的后续改动是掠视淡化。原有资产导入的两条三角化告警仍存在，未通过重做资产或改变材料掩盖。没有重新引入 GL 0x506 路径。

无标注实际接收图及视频集中在 `logs/p6b/delivery/`，`manifest.json` 记录来源、操作及 SHA-256。独立原图为收到的像素原样复制；比较图只拼接并在图外标说明。MP4 独立视频从实际 DDS H.264 无损封装；并排比较视频单独标为重编码。

* [正常世界 Game 显示原图](../logs/p6b/delivery/normal_world_asset.png) / [尾焰与双云短视频](../logs/p6b/delivery/normal_world_asset.mp4)
* [Auto 原图](../logs/p6b/delivery/normal_world_auto.png) / [24 秒相机横移与进出视野](../logs/p6b/delivery/normal_world_pan.mp4)
* [片云细条改前/改后](../logs/p6b/delivery/sheet_before_after.png) / [同显示设置对照视频](../logs/p6b/delivery/sheet_before_after.mp4)
* [旧公式参考/新云算法](../logs/p6b/delivery/cloud_algorithm_comparison.png) / [算法对照视频](../logs/p6b/delivery/cloud_algorithm_comparison.mp4)
* [普通平板遮挡](../logs/p6b/delivery/ordinary_plate_occlusion.png) / [分离视角](../logs/p6b/delivery/separated_world_view.png)
* [高亮映射顺序对照](../logs/p6b/delivery/display_highlight_order.png)
* [部署后默认 MWIR 原图](../logs/p6b/delivery/deployed_default_mwir.png) / [实际接收视频](../logs/p6b/delivery/deployed_default_mwir.mp4)；[默认 NVG 原图](../logs/p6b/delivery/deployed_default_nvg.png) / [实际接收视频](../logs/p6b/delivery/deployed_default_nvg.mp4)

真实两实例最终构建对照见 `world_two_release_instances.json`；最终 Windows/aarch64 正常渲染对照见 `world_render_release_windows_arm.json`，8 个共享描述最大数值差 `1.14e-13`。全部计数、耗时和编解码差异见 `logs/p6b/evidence.json`。

## 部署与回滚

沿用 `tools/rk3588_deploy_atomic.ps1 -ReuseVerifiedFiles`：先校验当前完整 Config，再用硬链接快照创建候选版本；变化文件以新文件加 rename 替换，校验全 Config/ELF/启动器/性能脚本后停进程切换，并再次核对旧回滚快照。派生缓存、JSON、INI、Weather/GameVFX、图集全部包含，shader 在本轮 ELF 内编译；不是只拷 ELF。配置总量约 5.75 GB，不改板卡时钟、网络/许可证、DDS/OrderedQueue/Mali/MPP/P3 策略。启动器仍只校验明确资源清单。

每次 `logs/p6b/deploy*/` 保存完整 manifest、版本、备份位置和校验日志。回滚必须成套恢复对应时间戳的 Config、ELF、run_precise.sh 与性能脚本；硬链接快照不可原地编辑。最终日志为 [deploy_final.run.log](../logs/p6b/deploy_final.run.log)，部署检查、切换后检查与原回滚快照检查均通过。

| 最终发布项 | SHA-256 / 标识 |
|---|---|
| ELF | `f02b7d05e1c81c9f5b55601f7162223369419356d5a18f622d10d7e4de1e3cda` |
| ELF Build ID | `854fa8f86dd99b00a150cbe9ad93756084ff7636` |
| 完整 Config manifest | `7e84ae2c0432143772311ac0f43b6271101ed93fe418ac792a93d71e842d2c0d` |
| HwaSimIRRuntime.ini | `a3c4a2255240629755c8d58a41a3eade6a9349cd32b6215707f34ddcd168ccda` |
| 部署版本 | `e18927e8e68cb17ea0a898efff8773301ae704fb-dirty`；逐文件源码哈希见 `logs/p6b/source_final.json` |
| 配套回滚快照 | `/userdata/HwaSimIR/Config.before_20260913-061122` 及同时间戳 ELF/启动器/性能脚本 |

最终 10 分钟使用同一 ELF、同一 Weather 资源与 Game 固定显示，随后只将已验证的天气选择提升为 INI 默认并再次成套部署。长测 36000 个输入/视频/元数据/标注全部守恒，最终队列与旧帧发布均为 0，平均 60.002 FPS。渲染日志均值 15.612 ms，其中 RGB8 读回 1.254 ms；另一个输出线程的抽样均值为 NV12 转换 9.067 ms、MPP 编码 1.835 ms、DDS 发布 0.013 ms，线程并行工作，不能将这些数与渲染耗时简单相加。

验收仍有边界：近景体积层次偏软；本轮云前普通物体深度遮挡通过，但没有实现对云体内部不透明物体的完整光线积分截断。四云未取得正常世界质量档资格。独立传感器实验只证明人工模型与计算流程，不代表真实器件标定。
