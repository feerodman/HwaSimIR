# HwaSimIRCustomerReceiverDemo

板端纯 C++ 客户接收端。先订阅 `HwaSimIR.VideoStatus`，然后自动按 Status 的
`videoTopic/codec/pixelFormat/width/height/fps` 订阅 `DDS::Bytes`。H264 原样保存
Annex-B，Raw 严格验证完整帧长度。当前基础构建不链接 MPP，因而 `decodeFps=0`；
MPP 解码为可选现场后端，不影响接收和落盘契约。

```bash
./HwaSimIRCustomerReceiverDemo --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --status-topic HwaSimIR.VideoStatus --output received.h264 --frames 600 --timeout-sec 30
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
多网卡测试可换用明确绑定板卡 IP 的实验室 QoS；客户通用模板仍为 `tcpv4://default//0`。
