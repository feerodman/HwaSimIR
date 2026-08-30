# HwaSimIRStimDdsDemo

纯 C++ DDS 参考激励端。它按 `RESET -> INIT -> wait InitAck -> START -> Realtime -> STOP`
发送 `HwaSimIRProtocolV1.idl` 的 typed Topic；不依赖 DataDrivenTestQT，也不发送视频。

客户最短接入路径是：创建一个进程级 `DdsRuntimeManager`，用
`ControlCommandV1TypeSupport`、`InitCommandV1TypeSupport`、
`RealtimeDataV1TypeSupport` 创建三个 Writer，再用 `InitAckV1TypeSupport`
创建 Reader。业务数据先填现有 BYHWICD struct，再经 `CommonDataDdsAdapter::ToDds()`
逐字段转换，禁止对 DDS object 做 `memcpy`。

VS2015 x64：

```powershell
cmake -S . -B build-vs2015 -G "Visual Studio 14 2015 Win64" -DZRDDS_ROOT=F:/Programs/ZRDDS/ZRDDS_VS2015/ZRDDS-2.4.5
cmake --build build-vs2015 --config Release
```

MinGW 7.3.0 x64：

```powershell
cmake -S . -B build-mingw -G "MinGW Makefiles" -DZRDDS_ROOT=F:/Programs/ZRDDS/ZRDDS_MinGW7.3.0/ZRDDS-2.4.5
cmake --build build-mingw --config Release
```

AArch64（VM）：

```bash
cmake -S . -B build-aarch64 -DCMAKE_C_COMPILER=/usr/bin/aarch64-linux-gnu-gcc -DCMAKE_CXX_COMPILER=/usr/bin/aarch64-linux-gnu-g++ -DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64
cmake --build build-aarch64 -j4
```

运行：

```text
HwaSimIRStimDdsDemo --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --plat-id 1 --sensor-id 1 --sim-mode 2 --video-fps 60 --width 800 --height 800 --h264 1 --save-mp4 0 --realtime-annotation 1 --rounds 1 --inter-round-wait-ms 6000 --duration 10 --realtime-hz 60
```

可靠启动/退出参数默认值为 `--discovery-wait-ms 2000`、`--ack-timeout-ms 30000`、
`--inter-round-wait-ms 6000`、`--shutdown-drain-ms 5000`。多回合时每回合必须消费一份新的 InitAck，不能复用上一回合的 Ack。每个并发 Windows DDS 进程必须在自己的工作目录中使用独立、
可写的 `zrddslicence.lic`；VS2015 和 MinGW 必须分别使用对应 SDK 的 DLL/导入库。
