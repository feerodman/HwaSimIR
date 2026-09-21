# HwaSimIR P16 交付入口

本目录是 P16 的轻量、可核验交付包。最终程序已经部署到 RK3588：

- 板端启动文件：`/userdata/HwaSimIR/HwaSim_IR`
- Windows 构建：`D:\HwaSimIR\HwaSim_IR\Bin\HwaSim_IR.exe`
- 完整结论：`docs/HwaSimIR_P16_Closeout.md`
- 问题状态：`docs/HwaSimIR_P16_Issue_Ledger.csv`
- 生产媒体索引：`evidence/production/media_index.json`

`deployment/windows/HwaSim_IR.exe` 和 `deployment/board/HwaSim_IR` 是二进制交付副本。`deployment/config_overlay` 只包含本轮实际修改/核对的外置 shader、Weather 资源索引和纹理，不重复复制约 5.4 GiB 的完整 Config；运行时必须继续使用现有、manifest 已验证的完整 `Config`。

媒体分类：

- `media/production`：实际 HwaSim_IR/RK3588 生产渲染路径；受控 fixture 或诊断相机属性见每个 `case_result.json`，不能冒充用户普通入口。
- `media/annotation`：`编号(x,y)` 截图和从真实连续 DDS 视频裁出的短片。
- `media/independent`：独立一般 RGB 图形样例，不是红外图像，不用于关闭生产问题。
- `evidence/raw_qc`：渲染器 RGBA16F SI readback 的 signed 差分报告/图；绝对差只作辅助。

校验：`SHA256SUMS.txt` 覆盖交付包内除自身和 `manifest.json` 外的所有文件；`manifest.json` 提供相同清单的结构化版本。真实标定状态保持 `NOT_VERIFIED_CALIBRATION`。

回滚：Windows P16 前 EXE 位于 `D:\HwaSimIR\logs\p16\backups\pre_p16\HwaSim_IR.exe`；板端备份为 `/userdata/HwaSimIR/HwaSim_IR.before_20260921-201312` 和 `/userdata/HwaSimIR/Config.before_20260921-201312`。不要用 `git reset --hard` 回滚源工作区。

