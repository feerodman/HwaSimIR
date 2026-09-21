# P14 首个有效实时位置 DDS fixture

这个程序直接构造协议对象并通过 `DdsStimClient` 发布；它不打开、解析或预知
`DataDrivenTestQT/1.txt`。固定使用业务身份 `1001/2` 和 `800x800`。

`--scenario all` 依次独立执行：START 后无包、全零 INIT 后有效、全零占位后有效、平台有效而目标
晚到、`ViewValid` 与位置有效性分离、有效后再无效、同世代 STOP/START、合法零地理坐标。每个场景
都有新的 RESET/INIT 世代，日志中的 `expectedReceiver` 必须和板端
`[FormalReferenceCommit]`、`[RealtimeValidity]`、`[Stage6 FrameDiag]` 对账。

示例：

```
HwaSimIRP14FirstValidFixture --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --band 2 --scenario all
```

集合 B 的两波段短录像使用 `--scenario boundary_demo --tail-frames 180`。该场景按
“START 后无包 -> 全零占位 -> 首个有效实时位置”发送，并明确记录测试端
`ForceVisibleForDemo` 的源/有效显示标志；生产过滤逻辑不变。
