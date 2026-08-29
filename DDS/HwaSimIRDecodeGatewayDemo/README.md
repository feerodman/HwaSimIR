# HwaSimIRDecodeGatewayDemo

RK3588/AArch64 上的正式演示 Gateway。它订阅 HwaSimIR 的 H264 Annex-B AU，使用真实
Rockchip MPP 解码为 NV12，提取无 stride padding 的 Y 平面，并按一整帧一个
`DDS::Bytes` Sample 发布 RawGray8。它不使用 ShapeType 或 VideoChunk。

```bash
./HwaSimIRDecodeGatewayDemo --domain 150 \
  --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --channel precise \
  --decoded-topic HwaSimIR.Decoded.precise.RawGray8 --frames 0
```

`--frames 0` 表示等待源 `VideoStatus.running=false`，执行 MPP EOS drain、DDS writer
ack + bounded drain 后退出。
