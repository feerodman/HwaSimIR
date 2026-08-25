# DDS CLion RK3588 Build Guide

本文记录 D1 后无需 Codex、可在 CLion 中手动完成的 HwaSim_IR 交叉编译配置。

## 固定环境

- 本地工程：`D:\HwaSimIR\HwaSim_IR\HwaSim_IR`
- VM：`linaro@192.168.203.128:22`
- VM 工程根：`/home/linaro/userdata/HwaSimIR`
- C：`/usr/bin/aarch64-linux-gnu-gcc`
- C++：`/usr/bin/aarch64-linux-gnu-g++`
- Ninja：`/usr/bin/ninja`
- Panda3D：`/opt/panda3d-aarch64`
- OpenCV：`/usr/lib/aarch64-linux-gnu/cmake/opencv4`
- RKMPP sysroot：`/home/linaro/sysroots/rk3588-mpp`
- ZRDDS sysroot：`/home/linaro/sysroots/zrdds-aarch64`

不要在 RK3588 上联网安装依赖。ZRDDS sysroot 的生产来源是板端实际安装
`/usr/ZRDDS/ZRDDS-2.4.5/include` 和 `lib`，通过离线 tar 中转到 VM。

## Toolchain GUI 配置

1. 打开 `File | Settings | Build, Execution, Deployment | Toolchains`。
2. 选择现有 `Debian11-aarch64-rk3588`，不要修改或删除原 MPP Toolchain。
3. 确认连接为 `linaro@192.168.203.128:22` 且显示已连接。
4. 确认 Build Tool 为 `/usr/bin/ninja`。
5. 确认 C Compiler 为 `/usr/bin/aarch64-linux-gnu-gcc`。
6. 确认 C++ Compiler 为 `/usr/bin/aarch64-linux-gnu-g++`。

在 `Build, Execution, Deployment | Deployment` 中确认：

- Root path：`/home/linaro/userdata/HwaSimIR`
- Mapping 的 Local path：`D:\HwaSimIR\HwaSim_IR\HwaSim_IR`
- Deployment path：`/`

密码/密钥保留在 CLion 用户级凭据中，不写入仓库。

## Release Profile

打开 `File | Settings | Build, Execution, Deployment | CMake`，选择
`Release-aarch64-rk3588-ssh`：

- Build type：`Release`
- Toolchain：`Debian11-aarch64-rk3588`
- Generator：`Ninja`
- Build directory：`cmake-build-release-aarch64-rk3588-ssh`
- Build options：`-j 4`

在 CMake options 中放入一行：

```text
-DHWASIMIR_ENABLE_RKMPP=ON -DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp -DHWASIMIR_ENABLE_ZRDDS=ON -DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64 -DHWASIMIR_ENABLE_FFMPEG=OFF -DPANDA3D_ROOT=/opt/panda3d-aarch64 -DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4
```

`HWASIMIR_ENABLE_ZRDDS` 默认为 OFF；显式 ON 后若 C++ header 或
`libZRDDSCpp.so` 不存在，CMake 必须 `FATAL_ERROR`，不会静默降级。

## 手动 Reload 和 Build

1. 点击 Apply/OK。
2. 选择顶部 Profile `Release-aarch64-rk3588-ssh`。
3. 执行 `Tools | CMake | Reload CMake Project`；等待 Reload 完成。
4. 顶部目标选择 `HwaSim_IR`。
5. 点击锤子 Build，或 `Build | Build HwaSim_IR`。
6. Build 窗口应显示远端命令：

```text
/usr/bin/cmake --build /home/linaro/userdata/HwaSimIR/cmake-build-release-aarch64-rk3588-ssh --target HwaSim_IR -j 4
```

7. 最终应显示 `30/30 Linking CXX executable HwaSim_IR` 和“构建 已完成”。

D1 实测还确认生成物为 AArch64 ELF，编译定义包含：

```text
HWASIMIR_HAS_RKMPP=1
HWASIMIR_HAS_ZRDDS=1
_ZRDDSCPPINTERFACE
```

本配置只建立 ZRDDS 编译/链接骨架。D1 不启用运行时 DDS 视频发送，不改变
现有 TCP、UDP、MPP 或协议数据结构。

## 命令行等价构建

IDE 故障排查时可在 VM 执行：

```bash
cmake -S /home/linaro/userdata/HwaSimIR \
  -B /home/linaro/userdata/HwaSimIR/cmake-build-release-aarch64-rk3588-ssh \
  -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DHWASIMIR_ENABLE_RKMPP=ON \
  -DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp \
  -DHWASIMIR_ENABLE_ZRDDS=ON \
  -DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64 \
  -DHWASIMIR_ENABLE_FFMPEG=OFF \
  -DPANDA3D_ROOT=/opt/panda3d-aarch64 \
  -DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4
cmake --build /home/linaro/userdata/HwaSimIR/cmake-build-release-aarch64-rk3588-ssh -j4
```

