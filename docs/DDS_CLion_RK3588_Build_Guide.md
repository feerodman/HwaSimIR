# DDS / RK3588 CLion 交叉编译指南

本文记录 D1/D2 后无需 Codex、可在 CLion 中手动完成的 HwaSim_IR
AArch64 交叉编译配置。不要在 RK3588 上联网安装依赖。

## 固定环境

- 本地工程：`D:\HwaSimIR\HwaSim_IR\HwaSim_IR`
- Debian VM：`linaro@192.168.203.128:22`
- VM 工程根：`/home/linaro/userdata/HwaSimIR`
- C：`/usr/bin/aarch64-linux-gnu-gcc`
- C++：`/usr/bin/aarch64-linux-gnu-g++`
- Ninja：`/usr/bin/ninja`
- Panda3D：`/opt/panda3d-aarch64`
- OpenCV：`/usr/lib/aarch64-linux-gnu/cmake/opencv4`
- RKMPP sysroot：`/home/linaro/sysroots/rk3588-mpp`
- ZRDDS sysroot：`/home/linaro/sysroots/zrdds-aarch64`

ZRDDS sysroot 的生产来源是板端实际安装的
`/usr/ZRDDS/ZRDDS-2.4.5/include` 和 `lib`。使用 tar 离线复制并保留库软链接，
不得从互联网下载另一个 SDK。

## Toolchain GUI 配置

1. 打开 `File | Settings | Build, Execution, Deployment | Toolchains`。
2. 选择现有 `Debian11-aarch64-rk3588`，不要删除或改坏原 MPP Toolchain。
3. 确认 SSH 是 `linaro@192.168.203.128:22` 且已连接。
4. Build Tool 选择 `/usr/bin/ninja`。
5. C Compiler 选择 `/usr/bin/aarch64-linux-gnu-gcc`。
6. C++ Compiler 选择 `/usr/bin/aarch64-linux-gnu-g++`。

在 `Build, Execution, Deployment | Deployment` 中确认：

- Root path：`/home/linaro/userdata/HwaSimIR`
- Local path：`D:\HwaSimIR\HwaSim_IR\HwaSim_IR`
- Deployment path：`/`

密码或密钥只保存在 CLion 用户级凭据中，不写入仓库。

## Release-aarch64-rk3588-ssh Profile

打开 `File | Settings | Build, Execution, Deployment | CMake`，选择或创建
`Release-aarch64-rk3588-ssh`：

- Build type：`Release`
- Toolchain：`Debian11-aarch64-rk3588`
- Generator：`Ninja`
- Build directory：`cmake-build-release-aarch64-rk3588-ssh`
- Build options：`-j 4`

CMake options 使用一行：

```text
-DHWASIMIR_ENABLE_RKMPP=ON -DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp -DHWASIMIR_ENABLE_ZRDDS=ON -DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64 -DHWASIMIR_ENABLE_FFMPEG=OFF -DPANDA3D_ROOT=/opt/panda3d-aarch64 -DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4
```

`HWASIMIR_ENABLE_ZRDDS` 默认 OFF；显式 ON 后缺少 C++ header 或
`libZRDDSCpp.so` 时 CMake 必须 `FATAL_ERROR`，不会静默关闭 DDS。
`HWASIMIR_ENABLE_FFMPEG=OFF` 只关闭 H264 FFmpeg 编码器；如果目标 sysroot
已存在 libavformat/libavcodec/libavutil，CMake 会启用本地 MP4 的
`shared_h264_remux` 后端。RK3588 H264 编码仍由 MPP 提供。

## 手动 Reload 和 Build

1. 点击 Apply/OK。
2. 选择顶部 Profile `Release-aarch64-rk3588-ssh`。
3. 执行 `Tools | CMake | Reload CMake Project`。
4. Reload 成功后选择目标 `HwaSim_IR`。
5. 执行 `Build | Build HwaSim_IR`。
6. Build 窗口应显示远端命令：

```text
/usr/bin/cmake --build /home/linaro/userdata/HwaSimIR/cmake-build-release-aarch64-rk3588-ssh --target HwaSim_IR -j 4
```

最终产物必须是 AArch64 ELF，并包含：

```text
HWASIMIR_HAS_RKMPP=1
HWASIMIR_HAS_ZRDDS=1
_ZRDDSCPPINTERFACE
```

用 `readelf -d HwaSim_IR` 还应看到目标侧 `librockchip_mpp.so.1` 和
`libZRDDSCpp.so`；若启用共享 H264 MP4 mux，还会看到目标侧
`libavformat.so`。不得链接 Windows/x86_64 ZRDDS 库。

## 命令行等价构建

IDE 故障排查时在 VM 执行：

```bash
cmake -S /home/linaro/userdata/HwaSimIR \
  -B /home/linaro/userdata/HwaSimIR/cmake-build-release-aarch64-rk3588-ssh \
  -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/usr/bin/aarch64-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=/usr/bin/aarch64-linux-gnu-g++ \
  -DHWASIMIR_ENABLE_RKMPP=ON \
  -DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp \
  -DHWASIMIR_ENABLE_ZRDDS=ON \
  -DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64 \
  -DHWASIMIR_ENABLE_FFMPEG=OFF \
  -DPANDA3D_ROOT=/opt/panda3d-aarch64 \
  -DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4
cmake --build \
  /home/linaro/userdata/HwaSimIR/cmake-build-release-aarch64-rk3588-ssh \
  --target HwaSim_IR -j4
file /home/linaro/userdata/HwaSimIR/cmake-build-release-aarch64-rk3588-ssh/HwaSim_IR
readelf -d /home/linaro/userdata/HwaSimIR/cmake-build-release-aarch64-rk3588-ssh/HwaSim_IR
```

## D2 运行注意事项

板端启动前设置：

```bash
export ZRDDS_HOME=/usr/ZRDDS/ZRDDS-2.4.5
export LD_LIBRARY_PATH="$ZRDDS_HOME/lib:$LD_LIBRARY_PATH"
```

从 HwaSim_IR 正常资产工作目录启动，不要永久 `cd` 到 ZRDDS SDK 目录。
DDS、TCP 和本地 MP4 均使用同一次最终传感器 readback；启用 DDS 或有效录像
时输出队列启用 no-drop/backpressure，二者均关闭时保留原异步 latest-overwrite
行为。

## 2026-08-26 D2 状态

D2 代码已在 RK3588 本机用同一 AArch64 头文件和库完成隔离构建，产物为
AArch64 ELF，依赖 `librockchip_mpp.so.1`、`libZRDDSCpp.so` 和
`libavformat.so.58`，并完成 MPP+DDS/Raw/MP4 短 smoke。

2026-08-26 D2 收口时已在 VM `192.168.203.128:22` 按上述
`Release-aarch64-rk3588-ssh` Profile 完成命令行等价 Reload 和 Build，无需操作
CLion GUI。Reload 退出码为 0，`HwaSim_IR` 目标全量构建 32/32、退出码为 0；产物为
ELF64 AArch64，并依赖 `librockchip_mpp.so.1`、`libZRDDSCpp.so` 和
`libavformat.so.58`。原始 Reload、Build、CMakeCache 和 ELF 验证记录保存在
`logs/dds-d2-20260826-004813/d2_clion_build_evidence.tgz`；首次 VM 不可达记录仍保留
在同目录 `vm_clion.txt` 中。
