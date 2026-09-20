# P13 原始 1.txt 普通 DDS 回放手册

## 前置身份

运行前确认：

- `DataDrivenTestQT/1.txt` SHA-256 为 `f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901`。
- 控制端地址 `192.168.1.188`，RK3588 地址 `192.168.1.116`；板卡不需要且不得访问互联网。
- DDS 身份严格为 `outerPlatID=1001/sensorID=2`，接收端自动应用 `VideoStatus`。
- 板端正式 ELF、配置 manifest、Runtime INI、LUT 和覆盖 manifest 与《P13 最终执行状态》一致。
- 普通配置 `InputFile=1.txt`；不要使用 `ordinary_demo_1km.txt`，不要改名派生轨迹。

## 一键普通回放

在 `D:\HwaSimIR` 中运行，密码只在当前会话交互/环境中提供，不写入日志或交付包：

```powershell
$env:HWASIMIR_SSH_PASSWORD='<交互提供>'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\p13_original_dds_case.ps1 `
  -Band SWIR -Weather Clear `
  -Name original_1_SWIR_Clear_acceptance `
  -DurationGuardSec 100
```

MWIR 示例：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\p13_original_dds_case.ps1 `
  -Band MWIR -Weather Snow `
  -Name original_1_MWIR_Snow_acceptance `
  -DurationGuardSec 100
```

脚本按接收录像端 → 板端 `run_precise.sh` → DataDrivenTestQT 的顺序启动。发送器先全文件校验，再按 60 Hz 逐行发送全部 4318 行。它等待 STOP 状态、板端渲染/DDS 排空、末帧到达和录像 mux 完成；约 10 s 的退出是正常的完整排空，不应提前杀进程。

## 时间关系

- 源时间：文件 `Time(ms)`，20–43190 ms，决定仿真 epoch 的源偏移。
- 发送时间：接受行按 `rowIndex × 16.6666667 ms` 实时节拍发送；4318 行约 71.95 s。
- 仿真时间：`UTC 基准 + (sourceTime-firstSourceTime)`，因此源物理跨度仍是 43.17 s。
- 视频 PTS：按实际输出帧序列单调生成，约 `sourceSeq/60 s`；最终以 `frame_index.jsonl` 和 ffprobe 为准。

一条接受输入对应一个源序号，不通过跳过输入凑 60 FPS；四种时钟不应混为同一数值。

## 验收条件

- 严格解析 4318/4318，发送 4318/4318；表头/空行不误报。
- 接收产品、`frameSeq`、`sourceSeq` 都从 1 连续到 4318，无重复、无缺口。
- MP4 为 H.264、800×800、4318 帧、可完整解码；`received.h264` 同时存在。
- `P5MaterialView=0`、正式 M1 实际运行；输入/LUT/程序/配置身份写入 case plan/result。
- STOP 控制响应、渲染排空、DDS 总排空、录像 flush/mux 分项存在。

## 诊断与性能隔离

普通性能测试不得启用 `LinearDiagnosticSeqs` 或 `EnableAgcDiagnostic`。需要 raw 证据时单独运行诊断用例，例如序号 `1,2159,4318`；输出 PFM/RGB8/统计文件，并在 case plan 中明确标记 diagnostic。固定映射和 AGC 两个诊断可以共享原始输入，但不能用其耗时替代普通性能结果。

