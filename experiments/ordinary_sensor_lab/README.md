# 独立普通传感器人工实验

这里保留 P6B 的单像元参考计算，并新增 P6C 独立二维人工实验。原 400–700 nm 人工基准与 28 项测试继续保留；核心增加明确的人工 NIR/MWIR 光子波段以及曝光/转换增益单位换算。程序不链接 HwaSimIR、业务目标、热点、跟踪或状态模块，不写生产配置。[二维模型说明与运行方法](IMAGE_LAB.md)。

运行：

```powershell
F:\Programs\anaconda3\python.exe experiments/ordinary_sensor_lab/sensor_cli.py experiments/ordinary_sensor_lab/synthetic_csv.json --output logs/p6b/sensor_lab/synthetic_csv.json
F:\Programs\anaconda3\python.exe experiments/ordinary_sensor_lab/test_reference.py logs/p6b/sensor_lab
```

JSON 明确每条曲线的波长单位（nm/um）、纵轴单位、人工来源、响应类型、已包含因素、入射参考面。CSV 只存 `wavelength,value`；数值范围不能代替单位定义。输入文件和实现文件各自记录 SHA-256。拒绝重复 JSON 键、非有限数、无序波长、外推、未知单位、图像/PFM 输入，以及重复 QE/光学因素。

绝对光子效率路径计算：

`μ_photo = A × t × ∫ Eλ(λ) × η(λ) × T(λ) × λ/(h c) dλ`

这里 `Eλ` 是指定平面的谱辐照度 W/(m²·nm)，不是谱辐亮度；λ 的光子能量换算使用米。像面输入不再乘镜头透过率；输入位于光学系统前且响应未含光学因素时才乘独立 T。绝对系统效率包含 QE 和光学因素，因此拒绝第二个 T。相对能量/光子响应只能给出相对加权量，不能生成绝对电子数。逐段线性曲线在所有断点上拆分，以三点高斯积分计算，避免只在端点相乘导致积分误差。

人工模型按独立 Poisson 光电子、Poisson 暗电子、满阱裁剪、Gaussian 读出噪声、DN 增益/黑电平、半向上舍入、ADC 裁剪的顺序计算。暗电流单位是 electron/pixel/s；它与安装资料中的 A/m² 不可直接互换。测试分别检查解析常量光谱、三角窄带谱、曝光线性、单位等价、无光、零曝光、固定随机种子、噪声均值/方差、满阱与 ADC 饱和、数字化边界。

限制：单像元入口没有空间层。二维入口新增人工 PRNU/DSNU 和离散 PSF，但没有器件实测 SRF、真实光学系统、温度依赖暗电流、实测 MTF、滚动快门、色彩标定或真实相机校准。PCG64 可在记录的 NumPy 环境中复现；不声明任意未来 NumPy 版本输出逐字节不变。当前渲染器公共缩放 PFM 无谱辐照度单位，不能接入此实验冒充测量值。

概念依据：[PBRT 4 §5.4](https://pbr-book.org/4ed/Cameras_and_Film/Film_and_Imaging) 对像面辐照度、曝光与像元面积的说明，以及 [EMVA 1288 官方资料](https://www.emva.org/standards-technology/emva-1288/) 对普通相机表征参数的定义。本程序没有执行完整 EMVA 标准测量，也不声称取得标准认证。
