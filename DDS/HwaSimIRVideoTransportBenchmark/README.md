# HwaSimIRVideoTransportBenchmark

固定内存 payload 的独立 DDS transport benchmark，不包含 Panda、readback、IR、天气或编解码。
所有模式都必须使用 `ZRDDS_PROTOCOL_QOS.xml` 的 tcpv4、RELIABLE、KEEP_ALL profile。

模式：`direct`（一个 Bytes Sample 一帧）、`shape1k`、`shape32k`、`chunk64k`、
`chunk32k`。Shape 模式复用 `DDS/dds_pub_sub/idl/ShapeType`；Chunk 模式仅是 benchmark
分片协议，真实重组后才计 receiverFrames，不是生产视频 wire contract。

```text
HwaSimIRVideoTransportBenchmark --role sub --mode direct --payload-bytes 640000 --frames 600 --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --topic HwaSimIR.Benchmark.Raw
HwaSimIRVideoTransportBenchmark --role pub --mode direct --payload-bytes 640000 --frames 600 --fps 60 --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --topic HwaSimIR.Benchmark.Raw
```

Publisher 默认在写完后执行 `wait_for_acknowledgments`，并保留
`--shutdown-drain-ms 2000` 的 bounded drain。最终可靠性仍以两端
`sourceFrames == receiverFrames` 为准，不把 ack 返回值单独当作尾帧证据。
