# HwaSimIRDecodeGatewayDemo

RK3588/AArch64 上的正式演示 Gateway。它订阅 HwaSimIR 的 H264 Annex-B AU，使用真实
Rockchip MPP 解码为 NV12，提取无 stride padding 的 Y 平面，并按一整帧一个
`DDS::Bytes` Sample 发布 RawGray8。它不使用 ShapeType 或 VideoChunk。

```bash
./HwaSimIRDecodeGatewayDemo --domain 150 \
  --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --channel precise \
  --plat-id 1001 --sensor-id 2 --frames 0
```

默认从 `HwaSimIR.VideoStatus` 自动发现源 H264 Topic，并在独立的
`HwaSimIR.DecodedVideoStatus` 发布 RawGray8 输出状态。正常运行不需要
`--video-topic` 或 `--decoded-topic`。`--source-status-topic`、
`--decoded-status-topic` 以及视频 Topic 参数仅用于调试覆盖。

`--frames 0` 表示等待源 `VideoStatus.running=false`，执行 MPP EOS drain、DDS writer
ack + bounded drain 后退出。
