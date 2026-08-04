# HwaSimIR V4：RK3588 MPP / CLion 交叉编译与实机验收

## 1. 环境边界

交叉编译在 Debian 虚拟机的 `/home/linaro/userdata/HwaSimIR` 中执行，目标为
Linux/aarch64。Windows 工程不读取或链接 MPP。已知依赖：

```text
C compiler       /usr/bin/aarch64-linux-gnu-gcc
C++ compiler     /usr/bin/aarch64-linux-gnu-g++
Panda3D          /opt/panda3d-aarch64
OpenCV CMake     /usr/lib/aarch64-linux-gnu/cmake/opencv4
MPP sysroot      /home/linaro/sysroots/rk3588-mpp
MPP header       <RKMPP_ROOT>/usr/include/rockchip/rk_mpi.h
MPP library      <RKMPP_ROOT>/usr/lib/aarch64-linux-gnu/librockchip_mpp.so
```

本次代码以板端 MPP 1.3.8 为目标，但版本号和设备可用性必须在实际板卡上再次确认。

## 2. CLion 配置

在 CLion 的 Debian 远程 Toolchain 中选择上述 GCC/G++，CMake Profile 使用
Release，并添加以下 CMake options：

```text
-DHWASIMIR_ENABLE_RKMPP=ON
-DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp
-DHWASIMIR_ENABLE_FFMPEG=OFF
-DPANDA3D_ROOT=/opt/panda3d-aarch64
-DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4
```

开启 `HWASIMIR_ENABLE_RKMPP` 后，CMake 会严格检查目标为 Linux/aarch64，并检查
以下两个精确路径；缺失时配置直接失败，不会静默退回无 MPP 构建：

```text
/home/linaro/sysroots/rk3588-mpp/usr/include/rockchip/rk_mpi.h
/home/linaro/sysroots/rk3588-mpp/usr/lib/aarch64-linux-gnu/librockchip_mpp.so
```

CMake 配置输出必须显示 `RKMPP_HEADER`、`RKMPP_LIBRARY` 和 “RK MPP encoder
enabled”。如果希望验证 MPP → FFmpeg → JPEG 的 auto 链，还需提供目标架构的
FFmpeg SDK 并设置 `HWASIMIR_ENABLE_FFMPEG=ON`；上述基准参数关闭 FFmpeg，因此
板端 auto 顺序实际为 MPP → JPEG。

## 3. 命令行等价流程

```bash
cd /home/linaro/userdata/HwaSimIR
chmod +x tools/rk3588_mpp_compile_check.sh tools/rk3588_v4_deploy_acceptance.sh

tools/rk3588_mpp_compile_check.sh
tools/rk3588_v4_deploy_acceptance.sh build
```

第一条命令只验证 MPP 头文件、aarch64 链接库和 AVC API 符号；生成的程序不能在
x86_64 虚拟机上运行。第二条命令执行完整 HwaSim_IR Release 交叉编译。

## 4. 部署

在虚拟机执行：

```bash
tools/rk3588_v4_deploy_acceptance.sh deploy <board-user@board-ip> /home/linaro/HwaSimIR-v4
```

脚本复制 `HwaSim_IR/Bin` 运行资产、aarch64 可执行文件和板端验收脚本。生产配置
不应为测试改写；建议另外准备 loopback/测试网络 INI，并显式传给 `--network-config`。

## 5. 板端验收

先确认：

```bash
test -e /dev/mpp_service
ldconfig -p | grep librockchip_mpp.so
```

启动 Windows VideoDisplay 和对应 UDP 激励端后，在板端执行：

```bash
cd /home/linaro/HwaSimIR-v4
chmod +x rk3588_v4_deploy_acceptance.sh
./rk3588_v4_deploy_acceptance.sh run <network-config.ini> 45
```

脚本固定请求 `H264Encoder=mpp`、禁用 JPEG 回退，并验证：

- `requestedBackend=mpp`；
- `activeBackend=mpp`；
- `activeCodec=h264_annexb`；
- `[MppPerf]` 有非零 payload；
- Packet v3 三段全开，flags 为 `0x7`；
- 没有回退 JPEG。

随后仍需手动完成并留存日志：

1. VideoDisplay 连续显示、output/display FPS、平均/P95 延迟和队列稳定性；
2. TCP 断开重连后的首个可解码 AU 为带 SPS/PPS 的 IDR；
3. 初始化、复位和新回合后的 IDR 恢复；
4. BGR24、RGB24、Gray8 三种输入的颜色/灰度方向；
5. precise/coarse 双进程 60 FPS；
6. MPP 设备、库版本、CPU 占用、颜色转换耗时、MPP 编码耗时和码率；
7. `mpp` 显式失败时，分别验证 `H264FallbackToJpeg=true/false`。

Windows 结果不能替代上述 aarch64 编译和 RK3588 实机结论。
