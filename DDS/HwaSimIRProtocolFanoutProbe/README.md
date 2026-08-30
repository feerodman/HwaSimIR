# HwaSimIRProtocolFanoutProbe

该工具不启动 Panda3D、MPP 或视频，只验证共享 Control/Init/Realtime Topic 的 middleware fan-out 与统一 ID route。

三个 Reader 进程分别运行：

```text
HwaSimIRProtocolFanoutProbe --mode reader --plat-id 1001 --sensor-id 1 --accept-broadcast 1 --expected-callbacks 12
HwaSimIRProtocolFanoutProbe --mode reader --plat-id 1001 --sensor-id 2 --accept-broadcast 1 --expected-callbacks 12
HwaSimIRProtocolFanoutProbe --mode reader --plat-id 1001 --sensor-id 3 --accept-broadcast 1 --expected-callbacks 12
```

一个 Sender：

```text
HwaSimIRProtocolFanoutProbe --mode sender --plat-id 1001
```

Sender 只创建一组 Control/Init/Realtime Writer，发送三个不同 geometry 的 exact Init、一个 sensorID=255 broadcast Init、三份 exact Realtime、一份 broadcast Realtime，以及三份错误平台样本。Reader 分别统计 `ddsCallbackSamples`、`routeAccepted` 和 `routeRejected`，以区分 middleware 未收到与应用路由拒绝。

CAEP Trial licence 会被 runtime 改写。正式多进程验收建议为每个进程准备独立、可写的 licence 副本。
