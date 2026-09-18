# HwaSimIR P12 交付包说明

## 内容布局

- `runtime/DataDrivenTestQT`：Windows 控制端及普通 1 km 示例输入。
- `runtime/HwaSim_IR_Windows`：Windows HwaSim_IR、Panda3D/FFmpeg 依赖和正式运行 Config。
- `runtime/HwaSim_IR_VideoDisplay`：Windows DDS 显示端及 Qt/FFmpeg 依赖。
- `runtime/RK3588`：最终 aarch64 ELF、普通 launcher 和板端性能模式辅助脚本。板端 Config 取自 `runtime/HwaSim_IR_Windows/Config`，原子部署脚本位于 `tools`。
- `source`：本轮三端和配置增量源文件。
- `tools`：P12 可续跑/审计/部署/回滚脚本。
- `docs`：P12 启动、成像、状态、问题台账和收口报告。
- `evidence`：精选真实图片、结构化结果、2 km MODTRAN 原始证据、性能、生命周期、部署和回滚证据。
- `report/index.html`：离线 HTML 总索引。
- `final_status.json`：结构化最终状态。
- `delivery_file_manifest.json`：ZIP 内所有其它成员的路径、字节数和 SHA-256。

## 普通启动

完整步骤见 `docs/HwaSimIR_P12_ManualStartup_And_NoImage_Fix.md`。日常启动不需要测试脚本隐藏参数：

1. RK3588：把 `runtime/RK3588/HwaSim_IR` 和 launcher 部署到 `/userdata/HwaSimIR`，把 `runtime/HwaSim_IR_Windows/Config` 作为相同版本 Config 部署；执行 `./run_precise.sh`。
2. Windows：从各自目录启动 DataDrivenTestQT 与 VideoDisplay；全 Windows 拓扑再从 `runtime/HwaSim_IR_Windows` 无参数启动 HwaSim_IR。
3. DataDrivenTestQT 依次 RESET、INIT、START，结束时 STOP。

普通示例输入为 `runtime/DataDrivenTestQT/ordinary_demo_1km.txt`。旧 `1.txt` 高空轨迹不作为正式普通示例。

## 身份与边界

- DDS domain 150，identity `1001/2`。
- status `HwaSimIR.VideoStatus`，自动发现视频 `HwaSimIR.Video.1001.2.H264`。
- 严格身份过滤保留；不订阅任意发布者，不静默回退。
- 正式波段为 SWIR/MWIR；NIR 仅兼容，VIS-SWIR 明确拒绝。
- P12 总体为 `PARTIAL`：SWIR 冷启动仍有 3 帧超过 80 ms；真实标定未验证。

## 未包含

包内不含 DDS 许可证、MODTRAN 可执行程序或许可证、账号口令、调试符号/中间文件、历史 MP4/H.264 大录像和无关 P11 UDP/TCP 大矩阵。运行者必须在合法环境中另行提供 DDS 许可证；MODTRAN 只在重新生成大气数据时需要，普通运行读取包内正式 LUT。

先用同目录 `.sha256` 校验 ZIP，再用 `delivery_file_manifest.json` 校验解压成员。任何成员身份变化都应视为新版本，不能与本报告中的测试结果混用。
