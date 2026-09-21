# HwaSimIR P15 交付入口

## 启动

双击或从命令行零参数启动：

`DataDrivenTestQT\DataDrivenTestQT.exe`

运行目录应为 `DataDrivenTestQT`。最终 EXE SHA-256：

`4d1238924beddd0626bf2ff4e0b1857c03618570d4b7668459d218cc249c6dde`

主窗口“版本信息”会显示实际程序、构建标识、配置、工作目录和程序哈希。

## 先看这些文件

- `docs/HwaSimIR_P15_Closeout.md`：完整结论、测试、身份、反馈和回滚。
- `docs/HwaSimIR_P15_Issue_Ledger.csv`：拆分状态的问题台账。
- `final_status.json`：机器可读最终状态。
- `evidence/ui/P15_final_DataDrivenTestQT_interaction.mp4`：最终 EXE 普通启动和实际交互。
- `evidence/numeric/`：数字标签前后、稳定编号和非显示字段一致性。
- `evidence/ordinary/P15_ordinary_original_1txt_mwir_dds.mp4`：当前版本普通入口 800×800 真实 DDS 短录像。
- `evidence/ordinary/P15_ordinary_user_entry_feedback_probe.mp4`：普通接收窗口录屏。
- `evidence/p14/p14_media_readonly_audit.json`：P14 历史媒体只读核对。
- `manifest.json`、`SHA256SUMS.txt`：包内容和完整性。

## 验收边界

UI 常显、最终 EXE 实际交互、数字标签和媒体解码均 PASS。

“用户普通运行未见天气”和“用户普通运行仍见黑色尾焰”保持 `REOPENED`。P15 当前普通入口记录显示 Cloudy/6 km 已进入运行态但云不可辨识；尾焰内部节点状态有效但画面不可单独辨识。P14 的受控 fixture、raw 差值和可解码 MP4 不作为普通窗口关闭证据。

真实标定保持 `NOT_VERIFIED_CALIBRATION`。
