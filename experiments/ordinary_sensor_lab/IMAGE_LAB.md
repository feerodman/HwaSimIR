# 独立二维人工传感器参考实验（P6C）

这是 `synthetic_reference`，不是普通器件的实测标定。沿用单像元的曲线、单位、参考面和三点 Gauss 光谱积分，再增加空间图案、曝光内变化、固定像元差异、逐帧噪声及图像输出。NIR 700–1100 nm、MWIR 3000–5000 nm 均为人工光子数学模型；MWIR 文件不代表某个制冷器件，更不代表换波长即可模拟热型探测器。

## 运行

依赖 Python、NumPy、SciPy、Pillow；本机验证环境记录在每次输出的 manifest 中。输出目录必须新建或为空，避免混入上一次实验的帧。

```powershell
F:\Programs\anaconda3\python.exe experiments/ordinary_sensor_lab/test_reference.py logs/p6c/sensor_single_pixel
F:\Programs\anaconda3\python.exe experiments/ordinary_sensor_lab/test_image_reference.py logs/p6c/sensor_image_tests
F:\Programs\anaconda3\python.exe experiments/ordinary_sensor_lab/image_cli.py experiments/ordinary_sensor_lab/synthetic_nir_image.json --output logs/my_synthetic_nir --frames 40 --seed 20260913 --ablations
F:\Programs\anaconda3\python.exe experiments/ordinary_sensor_lab/image_cli.py experiments/ordinary_sensor_lab/synthetic_mwir_image.json --output logs/my_synthetic_mwir --frames 40 --seed 20260914 --ablations
```

配置中宽高均为 800，序列 10 FPS，40 帧为 4 秒。单帧实时运行不是本实验目标。生成器支持 uniform、steps、edge、equal_energy_spectra、chart、moving_edge；只接受数学场描述，不支持 PFM、视频或外部图像作为光谱输入。

## 光场与单位

每个像元的谱辐照度表示为一至两个人工光谱基底与非负空间强度系数之和。每条基底保留完整分段线性光谱和全部断点；没有替换成中心波长。基底共享同一探测器响应和光学路径。当前场景使用像面 `W/m²/nm` 输入，QE 为绝对转换概率，光敏面积人工假定为 pitch²、填充因子 1。

核心计算 `A × ∫dt ∫dλ Eλ(x,y,t,λ) η(λ) λ/(hc)`；光谱在断点分段三点积分，曝光内使用明确数量的中点样本。静态场与单像元严格一致，仿射时间变化由独立解析式验证。空间强度取全局像元中心；图块只改变计算顺序。短序列中的普通亮边按声明的 pixels/s 移动，固定图案不随之移动。

人工曝光支持 s/us；转换增益支持 DN/electron 或 electron/DN，统一到 DN/electron。像面辐照度不能重复乘前段光学透过率；相对 SRF 不能生成绝对电子数。没有辐亮度至像面辐照度的成像光学模型，因此拒绝 `W/m²/sr/...` 输入。响应已含 QE/光学因素时拒绝重复因子，也拒绝把含光子能量权重的曲线再当 QE。

## 分层顺序

1. 全波段积分、曝光内时间积分 → 理想期望光电子图。
2. 人工离散 PSF → 模糊后的期望图。它代表空间无色差的像面辐照度模糊，发生在像元响应与随机起伏前；并非读出后的修图。核非负、奇数尺寸、和为 1。边界明确为周期边界，保直流和总和；因此画面边缘会发生周期延拓，不代表真实镜头边界。
3. PRNU 固定乘性图 → 期望光电子；DSNU 固定加性暗率图 → 曝光暗电子。两者使用固定种子命名空间 0/1 的 Gaussian 样本，并只在生成时作非负裁剪。配置 RMS 指裁剪前分布；实际图统计另存。
4. 独立 Poisson 光电子、Poisson 暗电子 → 带噪电荷；再裁到满阱。
5. Gaussian 读出电子噪声 → 转换增益、黑电平 → 半向上舍入 → ADC 裁剪。
6. 原始数字码值另行映射为固定满量程 8 位预览。预览没有 Auto 或 gamma，不参与后续物理计算。

固定图案一次生成；光电子、暗电子、读出噪声使用独立 namespace 10/11/12 与帧编号的 PCG64 流。完整空间 RNG 生成与图块顺序无关。记录 Python/NumPy/SciPy、平台、种子和源码哈希，不承诺任意未来版本的逐字节相同。

## 输出

* `fixed_prnu_factor.npy`、`fixed_dark_rate_e_per_pixel_s.npy`：整个回合固定的空间图案。
* `frame_0000/*.npy`：理想/模糊/最终期望光电子、期望暗电子、随机光电子/暗电子、带噪电荷、满阱后电荷、读出噪声/电荷及原始 DN。电子层为 float64；DN 为 uint16。
* `raw_dn_uint16.png`：14 位实际 DN 存在 16 位容器，未拉伸到 65535。
* `preview_u8.png`：单独的查看图，按 `round(DN/16383×255)` 生成。暗图可能只占码值范围的一部分，这不改变原始数据。
* 后续 `frame_NNNN`：逐帧原始 DN、16 位 PNG 与预览；第一帧保留完整中间层，其余层的统计逐帧记录。
* `synthetic_preview_sequence.png`：无损动画 PNG。交付还提供单独标为预览的 MP4，不能当原始码流。
* `ablations/`：理想期望、仅 PSF、固定图案、仅 shot、完整链五组对照；使用相同输入、同一帧编号和 RNG 命名空间。
* `manifest.json`：全部输入与源码哈希、参数、单位、逐帧统计、层开关和文件 SHA-256。

## 验证和真实器件缺口

原 28 项单像元基准保留。二维测试包含单像元一致、NIR/MWIR 独立 SciPy 自适应积分、曝光线性、单位等价、相同能量不同谱、固定图案稳定、122880 个独立观测的噪声均值/方差、图块逆序逐像素一致、PSF 直流/总量、满阱/ADC 分离和原始码值存储。黄金数值不只由被测积分代码生成。

`device_readiness.py` 与 `device_missing_template.json` 只检查器件资料元数据是否齐备；不会把“字段齐全”视为校准通过，也没有生产导入接口。状态分为 synthetic_reference、documented_example、device_characterized、device_validated；当前全部计算仍为第一类。实际器件身份、温度/读出条件、绝对响应、光学透过、暗电流温度依赖、读噪、满阱和转换标定仍缺失。

辐照度、曝光和光子计数概念可对照 [PBRT 4 §5.4](https://pbr-book.org/4ed/Cameras_and_Film/Film_and_Imaging)；参数表征思想参照 [EMVA 1288](https://www.emva.org/standards-technology/emva-1288/)。本实验没有执行完整 EMVA 测量或获得标准认证。
