# HwaSimIR P16 收口报告

日期：2026-09-21  
实际 HEAD：`f867c72e0cc3a2c2b4690f041108e2f4eadbc40d`  
P15 实施基线：`633048417108dfc20958324b683b467d7a54cc40`（仅作历史基线，本轮没有据此回退）  
状态：生产代码、外置 shader 和 Weather 资源已构建并部署；受控生产渲染证据通过；用户普通入口复验仍保持开放；真实标定保持 `NOT_VERIFIED_CALIBRATION`

## 1. 先给结论

| 项目 | 状态 | 结论 |
| --- | --- | --- |
| Windows x64 构建 | PASS | `D:\HwaSimIR\HwaSim_IR\Bin\HwaSim_IR.exe` 最终重建通过，SHA-256 `9fef9d55fdd1e0743d8f0450b020e402b18da4565d5f6b700bd94dbfd210b1bc` |
| RK3588 构建与部署 | PASS | `/userdata/HwaSimIR/HwaSim_IR` SHA-256 `3d8d97ded05014a11e78983770bd4dacfb0136ded7156e38cefcac4672c40bde`，Build ID `bea3b43a2c51c892735edf44d35359bf40a6d814` |
| 尾焰实际 draw/合成 | PASS（受控生产路径） | 找到同深度透明层排序不稳定这一实际根因；改为 halo `fixed/70`、core `fixed/71`，热核心 signed raw 为正，冷烟负贡献保留 |
| 用户普通入口尾焰反馈 | **REOPENED / 需现场复验** | 本轮证据使用实际生产 renderer/shader，但使用通用喷口受控 fixture；不能代替用户正在使用的具体普通入口验收 |
| 云/雨/雪实际 draw | PASS（受控生产路径） | 云使用 Weather profile 纹理；雨雪 Batch 区域、时间精度、纹理采样和局部源函数已修；同一 ELF 下均有连续 DDS MP4 和 raw 差分 |
| 用户普通入口天气反馈 | **REOPENED / 部分改善** | 雪在固定映射中可辨；雨单帧仍较淡但连续 raw 有移动条纹；原 `1.txt` 云有真实纹理贡献但普通映射对比度仍低，尚无当前用户入口签收 |
| 模型纹理闪烁 | MITIGATED / 需普通入口确认 | 生产模型加载加入颜色纹理 mipmap+三线性+各向异性，材质 ID 使用类别 mode mip；受控运动测试 temporal MAD 降低 11.18%，但既有普通媒体没有能排除运动/编码的精确复现段 |
| 模型/轨迹原件保护 | PASS | `Config/TargetLib/models` 无 diff；原 `DataDrivenTestQT/1.txt` SHA-256 仍为 `f2c3db00...dd7901` |
| P16 坐标标注 | PASS | 屏幕唯一入口输出 `编号(x,y)`，坐标等于原记录；16 组布局及隐藏/恢复/重排/旋转/重建/边角/位数变化通过 |
| P15 普通 UI 短回归 | PASS | 默认可见、选择民用 `0x55`、INIT 冻结、RESET 解锁、关闭重开通过；DataDrivenTestQT 源码本轮无改动 |
| 最终媒体可解码 | PASS | 8 个 effects7 生产案例全部 H.264、800×800，解码帧数逐项等于已接受输入行数；所有案例绑定同一板端 ELF |
| 独立图形样例 | PASS（仅辅助） | 一般 RGB 粒子/云雨雪与重叠层排序样例通过；不用于关闭生产天气、尾焰或红外标定问题 |
| 真实标定 | `NOT_VERIFIED_CALIBRATION` | 本轮没有把通用热源、纹理或类别采样结果写成具体装备真实红外特征 |

本轮没有扩大 MODTRAN 数据域，没有修改首有效位置、协议、队列、热模型或生产跟踪链，没有改 `Latest`、分辨率和 DDS 身份，也没有 commit/push。

## 2. 实际程序与资源身份

### 2.1 程序

| 角色 | 路径 | SHA-256 / Build ID |
| --- | --- | --- |
| Windows 常用构建 | `D:\HwaSimIR\HwaSim_IR\Bin\HwaSim_IR.exe` | `9fef9d55...210b1bc` |
| RK3588 最终部署 | `/userdata/HwaSimIR/HwaSim_IR` | `3d8d97de...2c40bde` / `bea3b43a...6d814` |
| DDS 发送端 | `D:\HwaSimIR\deliverables\HwaSimIR_P15\DataDrivenTestQT\DataDrivenTestQT.exe` | `4d123892...9c6dde` |
| 视频接收端 | `D:\HwaSimIR\HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe` | `731a05d9...b97b6` |
| 独立 RGB 样例 | `D:\HwaSimIR\samples\P16IndependentGraphics\bin\P16IndependentGraphics.exe` | `35034859...a4a09` |

板端实际运行配置 SHA-256 为 `3a56f9f6af7034beae7fe12f8c4e32d9a55df9e377d98e3d343fc579fe9df07f`；部署 manifest SHA-256 为 `c8c6573fd57fb6261fc8a011c06167e5aa75dfc56636e50884736cb5914e66b7`。形式 LUT 与 coverage manifest 分别为 `48432459...c7cd7`、`f5d27c30...c5d7b`。最终回执见 `logs/p16/build/effects7_deployment/deployment_receipt.json`。

### 2.2 Weather 资源绑定

本机和板端逐文件 SHA-256 一致：

| 资源 | SHA-256 | 实际用途 |
| --- | --- | --- |
| `Config/Weather/weather_textures.json` | `e2127622...4345a3` | 云、雨、雪键到 Weather 文件的唯一索引 |
| `Textures/rain.rgba` | `42938413...aee20` | Rain Batch alpha/coverage |
| `Textures/snow.rgba` | `133f7244...d1cc6` | Snow Batch alpha/coverage |
| `Textures/cloud_scattered.png` | `61f6c028...376f0d` | Cloudy profile；原 `1.txt` 最终案例日志确认实际加载 |
| `Textures/cloud_overcast.png` | `8f654ee3...87841` | Snow/overcast profile |
| `Textures/smoke.png` | `1ae51f8d...771ea` | 尾焰 halo 的空间 coverage；RGB 不参与辐射 |

最终外置 shader 也在板端逐项复核：`sprite.vert` `d702c032...499c`、`sprite.frag` `c0a4a4ac...b25d`、`precipitation.vert` `8ed949c0...2f0a`。这避免了“只换 ELF、仍读取旧 shader”的情况。

## 3. 尾焰修复

### 3.1 根因

当前 `sprite.frag` 在本轮开始前已经有独立 SI 分支，因此没有把旧的 0–1 截断再次误报为当前根因。实际问题位于 draw 合成顺序：核心和 halo 同深度放在 Panda 的 `transparent` bin，距离排序不能保证两个层的 5/6 sort 值按预期生效，面积更大的冷 halo 可能最后覆盖热核心，形成“全黑尾焰”。

修复后保持深度测试开、深度写关和 straight alpha：

- halo：`fixed/70`，先画冷吸收介质；
- core：`fixed/71`，再画热发射核心；
- 正式日志固定输出 `drawOrder=fixed_halo70_then_core71`；
- `radiusTail` 现在真实参与中心扩展和 quad 尺寸，不再把长尾压成根部宽度；
- halo 从 `Weather/Textures/smoke.png` 只取 alpha 作为空间遮罩；
- 没有改为 `tau=1`，没有整体加热、强制白色或提高显示增益。

### 3.2 signed raw 证据

最终 MWIR 通用喷口 On/Off 使用同一 effects7 ELF、相同输入和时间线。seq 900 的几何固定核心 ROI：

- signed mean：`+0.1306851959 W/(m² sr µm)`；
- signed sum：`+2509.15576`；
- 最大值：`+0.2978515625`；
- 正/负能量绝对值比：`5.68199`。

宽 ROI signed mean 为 `-0.0354719`，原因是更大面积的冷烟吸收超过紧凑热核心；这不是绘制失败，也没有用绝对差冒充“变亮”。红/蓝 signed 图见 `logs/p16/production_effects/raw_qc/plume_mwir_effects7/seq900_first_minus_second_signed.png`。

受控视频：

- On：`logs/p16/production_effects/runs/p16_final_mwir_plume_on_effects7/recording/round_001_20260921_201542_194/output.mp4`
- Off：`logs/p16/production_effects/runs/p16_final_mwir_plume_off_effects7/recording/round_001_20260921_201715_569/output.mp4`

这关闭了“生产 sprite 路径是否实际 draw、热核心是否有正辐射贡献”的工程问题；由于案例是通用喷口受控 fixture，`P15-ORDINARY-PLUME` 仍需从用户正在使用的普通入口复验后才能关闭。

## 4. 云雨雪修复

### 4.1 实施内容

- 取消普通天气被 CustomerDemo 外观强制成单一 scattered sheet 的行为；普通路径优先使用 `weatherState.cloudTexturePath`，仅缺失时才 fallback。
- 普通世界云保持 `u_game_sheet.x=0`；P6 艺术 fixture 才能使用显式 sheet 分支。
- 云 UV 不再在 shader 采样前 `fract()`；repeat sampler 负责环绕，保留 mip 导数，减少瓦片边界闪烁。
- 云、雨、雪纹理生成 mipmap；云使用三线性+anisotropy 4，雨雪使用三线性+clamp。
- 修复 Stage7 volume region 仅由可见云激活的问题：无云但有雨雪时也实际提交 Batch draw。
- 协议时间先减本世代首个有效实时时间再传 float；`sourceSeq=0` 的 INIT 占位不提交时间原点。最终日志 seq 2 的 `elapsedSec=0.0166669`，不再因约 `1.8e9 s` epoch 转 float 而丢失小数相位。
- 0.72 像素雨丝横向 coverage floor 只避免亚像素漏采样，不改辐射/alpha 增益。
- 雨使用当前环境温度，雪限制不高于 273.15 K，通过 Planck 计算 SI 源函数；纹理仍只提供 alpha/coverage。

### 4.2 同一 ELF 结果

| 案例 | 输入/帧 | raw 结果 | 固定映射观察 |
| --- | --- | --- | --- |
| MWIR Rain | 民用地面 fixture，1800/1800 | seq 900 ROI mean `+8.9753e-05`，5976 个正样本 | 连续运动中有细雨丝；单帧仍较淡 |
| MWIR Snow | 民用地面 fixture，1800/1800 | ROI mean `-3.4883e-04`，16191 个负样本 | 冷吸收雪粒可辨 |
| SWIR Snow | 民用地面 fixture，1800/1800 | ROI mean `-2.16006e-03`，16815 个负样本 | 冷雪斑直接可辨 |
| MWIR Cloudy | 未修改原 `1.txt`，4318/4318 | seq 900 ROI mean `-8.2733e-04`，正 212577/负 922851 | 普通映射有低对比纹理，signed 图清楚；日志 `visibleCloudVolumes=2` |

原 `1.txt` 案例保留头 659 行 `ViewValid=0`，因此联系图第一格黑屏是预期输入语义；第 660 行后才进入有效画面。云案例日志确认实际路径为 `Config/Weather/Textures/cloud_scattered.png`、coverage `0.595`，有效视线阶段 `activeCloudVolumes=8`、`visibleCloudVolumes=2`。

视频入口：

- MWIR 雨：`logs/p16/production_effects/runs/p16_final_mwir_rain_effects7/recording/round_001_20260921_202106_256/output.mp4`
- MWIR 雪：`logs/p16/production_effects/runs/p16_final_mwir_snow_effects7/recording/round_001_20260921_202240_071/output.mp4`
- SWIR 雪：`logs/p16/production_effects/runs/p16_final_swir_snow_effects7/recording/round_001_20260921_202412_826/output.mp4`
- MWIR 原轨迹云：`logs/p16/production_effects/runs/p16_final_mwir_cloud_original_effects7/recording/round_001_20260921_202548_361/output.mp4`
- 完整索引：`logs/p16/production_effects/media_index.json`

这些结果证明纹理绑定、Batch/云 draw 和 signed 像素贡献已恢复；雨和原轨迹云在普通固定映射中的对比度仍较低，加上案例不是用户当前窗口入口，因此 `P15-ORDINARY-WEATHER` 保持 `REOPENED`，不拿 raw、旧 PASS 或独立样例代替用户验收。

## 5. `models` 闪烁处理

### 5.1 只读历史定位

`logs/p16/flicker_audit/p16_flicker_audit.json` 只读核对了 P15 普通 MWIR 与 P14 两波段原轨迹媒体。P15 目标最大仅 9×9 像素；P14 的可疑帧仍混有真实运动、子像素采样和 H.264，且没有同帧编码前 raw，因此结论保持：

- `reproductionStatus=NO_REPRODUCER`；
- `rootCauseStatus=NOT_LOCATED`；
- Qt paint 长尾不是已确认根因；
- 没有把不同运行、不同时间或不同配置拼成“同帧”证据。

### 5.2 通用生产采样修复

在实际 `LoadPlatformAssetNode`/`IRSceneMaterialMapper` 路径实施：

- 颜色纹理：生成 RAM mip 链，min=`linear_mipmap_linear`、mag=`linear`、anisotropy=4；
- 材质 ID：不能线性混合编号，使用稳定 2×2 众数归约构建 R8 类别 mip；相同票数取原有最低 ID，min=`nearest_mipmap_nearest`、mag=`nearest`；
- 日志输出 `[ModelTextureSampling]` 和 `[MaterialIdSampling]`，F35/F22、AIM120、AIM9X 等实际加载均已看到；
- 没有修改、重导出、隐藏或降分辨率处理 `Config/TargetLib/models` 内任何模型/纹理。

受控 F22 生产 renderer 运动测试均为 800×800、60 fps、360 解码帧。颜色纹理 temporal foreground MAD 从 `0.01901123` 降到 `0.01688554`，降低 `11.181%`；两个材质 ID 控制组修改前后解码序列完全相同。证据见 `logs/p16/production_effects/model_flicker/comparison.json` 和 `parallax_texture_before_after.png`。

该结果证明通用采样改动有效，但不等于用户所述普通入口闪烁已经完整复现和关闭；该反馈保持 `OPEN / NEEDS_ORDINARY_REPRODUCER`。

## 6. P16 坐标标注和 UI 回归

生产唯一关重部位文字入口现在直接使用记录中的 `displayIndex`、`point.x`、`point.y`，格式例如 `1(320,240)`。布局 key 不包含坐标文本；标签避让只移动文本框，不修改文字内原图像坐标。原名称、ID、坐标、可见性、协议字段和目标关联未改。

`tools/p10_label_test.cpp` 完成 16 组分辨率/密集布局，以及坐标更新、2 隐藏/恢复、容器反序、90° 投影旋转、Overlay 重建、四角和数字位数变化。结果：`format_index_xy=1 coordinate_match=1 stable_keys=1 unchangedRecords=1 allLabelsRetained=1 PASS`。入口：

- 截图：`logs/p16/annotation/p16_number_coordinates_1_2_3.png`
- 四角：`logs/p16/annotation/p16_number_coordinates_corners.png`
- 请求/布局/原坐标对照：`logs/p16/annotation/p16_coordinate_label_alignment.csv`
- 连续真实 DDS 短片：`logs/p16/annotation/p16_coordinate_labels_dds_clip.mp4`，800×800、476 帧、7.983 s、SHA-256 `6b7b62fd...426354`

P16 没有改 DataDrivenTestQT。最终最小 UI 回归见 `logs/p16/ui_smoke/p16_ui_smoke_final.json`：默认无需展开预览即可看到控件，选择 `0x55`、INIT 冻结、RESET 解锁、关闭重开均通过。第一次用构建目录配置调用时因推导到不存在的 LUT manifest 而失败；纠正为 `DataDrivenTestQT/NetworkConfig.ini` 后通过，失败没有删除或改门槛掩盖。

## 7. 独立普通图形样例

独立样例位于 `samples/P16IndependentGraphics`，只使用 Panda3D 和自建棋盘/简单几何，不链接或导入 HwaSimIR 红外、跟踪模块，不读取业务轨迹，也不共享生产 shader/配置。

- 7 个单项/少量组合效果案例均可解码且相对 Off 基线可辨；
- 纹理静止基线 mean consecutive delta 为 0；
- 人工让两个层每帧交换 sort 的 before 为 `19.4361`，固定稳定 sort 的 after 为 `0`；
- 结果只说明一般 RGB 粒子和透明层行为，不关闭生产天气、尾焰或标定问题。

运行说明：`samples/P16IndependentGraphics/README.md`；媒体索引：`logs/p16/independent_sample/media_index.json`。

## 8. 媒体、时间和身份

最终生产媒体索引 `logs/p16/production_effects/media_index.json` 的 SHA-256 为 `6085e23d...b4b3a`，逐案例保存：caseId、程序/配置身份、输入哈希、分辨率、帧数、时间基准、开关事件、MP4/H.264/截图/日志/received/帧身份哈希和观察结论。

时间关系：

1. 源时间来自已接受 DDS 输入行；
2. 仿真时间沿用源时间，粒子相位只把本世代首有效样本作为浮点相对原点；
3. 发送节拍不改写源时间，也不跳过已接受输入凑帧率；
4. 视频 PTS/到达时间由接收端 `frame_index.jsonl` 独立记录；
5. STOP、DDS 排空、末帧和录像完成状态保存在各案例 `recording_status.json`/`case_result.json`。

8 个最终案例全部来自同一 `effects7` ELF，H.264 800×800：6 个 1800/1800 帧，两个原 `1.txt` 案例 4318/4318 帧。`allMp4DecodableWithExpectedFrames=true`，没有复制旧帧或拿少量 PNG 拼视频。

## 9. 修改文件

生产源码/外置资源：

- `HwaSim_IR/Bin/Config/GameVFX/sprite.frag`
- `HwaSim_IR/Bin/Config/GameVFX/sprite.vert`
- `HwaSim_IR/Bin/Config/Weather/precipitation.vert`
- `HwaSim_IR/HwaSim_IR/Annotation/AnnotationOverlay.cpp`
- `HwaSim_IR/HwaSim_IR/HwaSimIR.cpp`
- `HwaSim_IR/HwaSim_IR/HwaSimIR.h`
- `HwaSim_IR/HwaSim_IR/IR/IRGameSpriteBatch.h`
- `HwaSim_IR/HwaSim_IR/IR/IRPrecipitationBatch.h`
- `HwaSim_IR/HwaSim_IR/IR/IRSceneMaterialMapper.cpp`
- `HwaSim_IR/HwaSim_IR/IR/P5GraphicsTest.inl`

测试/证据工具：

- `tools/p10_label_test.cpp`
- `tools/p13_original_dds_case.ps1`
- `tools/p14_original_dds_case.ps1`
- `tools/p16_effects_raw_qc.py`
- `tools/p16_flicker_audit.py`
- `tools/p16_production_flicker_compare.py`
- `tools/p16_build_production_media_index.py`
- `tools/p16_analyze_independent_sample.py`
- `tools/p16_build_independent_sample.cmd`
- `tools/p16_run_independent_sample.ps1`
- `samples/P16IndependentGraphics/*`

用户已有未跟踪文件 `main.obj` 未删除或修改。`models`、原 `1.txt`、P14/P15 历史报告和旧媒体均保持原位。

## 10. 构建、检查与回滚

最终检查：

- `HwaSim_IR.vcxproj` Release/x64：PASS；
- AArch64 构建/部署：PASS；
- `tools/stage5_plume_check.ps1 -Strict`：PASS；
- `tools/stage7_weather_check.ps1 -Strict`：PASS；
- `git diff --check`：PASS（仅 CRLF 提示）；
- P16 标注：PASS；
- P15 UI 最小回归：PASS；
- 8/8 最终 MP4 解码帧数匹配：PASS。

回滚入口：

- Windows P16 前 EXE：`logs/p16/backups/pre_p16/HwaSim_IR.exe`，SHA-256 `34c25cda...50bb29c`；
- 板端 P16 最终部署前 ELF：`/userdata/HwaSimIR/HwaSim_IR.before_20260921-201312`；
- 板端 Config 备份：`/userdata/HwaSimIR/Config.before_20260921-201312`；
- 板端回滚时应先正常停止 renderer，再原子替换 ELF/Config，随后重跑普通启动预检；
- 源码回滚只能反向应用本报告第 9 节的 P16 diff，不得用 `git reset --hard` 覆盖用户修改。

## 11. 仍未关闭

1. `P15-ORDINARY-PLUME`：受控生产路径已修并有 signed raw 正核心，但需要用户普通入口当前版本的窗口证据后关闭。
2. `P15-ORDINARY-WEATHER`：雪可辨；雨单帧和原轨迹云普通映射仍偏淡，需要当前普通窗口连续视频和用户确认。
3. `P16-MODEL-FLICKER-ORDINARY`：采样已通用缓解，既有媒体无可排除运动/编码的精确复现段，需要相同普通入口、同模型、同轨迹的修改前后连续片段。
4. `P14-C-CAL`：保持 `NOT_VERIFIED_CALIBRATION`。
5. `P14-C-DOMAIN`：仍为已有离散覆盖及合法插值，不扩写成所有天气/全域。
6. `P14-D-QT-PAINT`：历史 Windows Qt paint 长尾与模型纹理闪烁分开，仍为 OPEN；本轮没有重跑无关性能矩阵。

