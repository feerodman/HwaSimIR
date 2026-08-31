# HwaSimIRCustomerReceiverDemo

板端纯 C++ 客户接收端。`--stream-role direct`（默认）订阅
`HwaSimIR.VideoStatus`，`--stream-role decoded` 订阅
`HwaSimIR.DecodedVideoStatus`，然后自动按 Status 的
`videoTopic/codec/pixelFormat/width/height/fps` 订阅 `DDS::Bytes`。H264 原样保存
Annex-B，Raw 严格验证完整帧长度。AArch64 构建打开 `HWASIMIR_ENABLE_RKMPP=ON`
后，`--decode mpp` 使用真实 RKMPP 解码 H264 为 NV12，并可保存去 stride 的 Y/Gray8。

```bash
./HwaSimIRCustomerReceiverDemo --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml \
  --stream-role direct --plat-id 1001 --sensor-id 2 \
  --output received.h264 --decode mpp --frames 300 --timeout-sec 120
```

H264 + MPP + Meta/Annotation：

```bash
./HwaSimIRCustomerReceiverDemo --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml \
  --stream-role direct --plat-id 1001 --sensor-id 2 \
  --channel precise --expect-codec h264 --output received.h264 --decode mpp \
  --gray-output decoded.gray --receive-meta 1 --receive-annotation 1 \
  --meta-output frame_meta.txt --annotation-output annotation.txt --frames 300
```

Gateway RawGray8：

```bash
./HwaSimIRCustomerReceiverDemo --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml \
  --stream-role decoded --plat-id 1001 --sensor-id 2 \
  --channel precise --expect-codec raw_gray8 --output decoded.raw --decode none --frames 300
```

AArch64 交叉构建：

```bash
cmake -S DDS/HwaSimIRCustomerReceiverDemo -B build/customer-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=/home/linaro/userdata/HwaSimIR/toolchains/aarch64-linux-gnu.cmake \
  -DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64 \
  -DHWASIMIR_ENABLE_RKMPP=ON -DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp
cmake --build build/customer-aarch64 -j4
```

板端运行前：

```bash
export ZRDDS_HOME=/usr/ZRDDS/ZRDDS-2.4.5
export LD_LIBRARY_PATH=$ZRDDS_HOME/lib:$LD_LIBRARY_PATH
cp "$ZRDDS_HOME/zrddslicence.lic" ./zrddslicence.lic
chmod u+w ./zrddslicence.lic
```

`--output` 对 H264 是 Annex-B 文件，对 Raw 是逐帧原样 append 的 raw 文件。程序只接受
`running=true` 的 VideoStatus，严格用 Status 的 `width*height*channels` 校验 Raw Sample。
正常运行不需要 `--video-topic`；该参数和 `--status-topic` 仅用于故障定位覆盖。输出中的
`wrongTopicSelections` 必须为 0。
多网卡测试可换用明确绑定板卡 IP 的实验室 QoS；客户通用模板仍为 `tcpv4://default//0`。
