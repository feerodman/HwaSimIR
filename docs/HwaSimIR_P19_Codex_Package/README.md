# P19 执行资料包

把整个目录交给Codex，执行根目录 `HwaSimIR_P19_Codex_Prompt.md`。

- `inputs/粗跟DDS错误.md`：用户原始日志。
- `inputs/20261014-DDS视频帧信息.txt`：不修改的客户原始类型与Topic。
- `inputs/H264_reference/`：从用户RAR中原样提取的两段.264和编码配置txt，供复核。未包含旧VLC安装程序，未执行用户提供的analyze.py；原RAR仍由用户持有。
- `inputs/board_directory.png`：板端版本目录截图；实际清理仍须以现场盘点为准。
- `inputs/P18_plume_keep_unchanged.png`：用户已认可、P19保持不动的图像效果。
- `analysis/`：本次独立ffprobe/ffmpeg解析输出与解释，不是客户硬件验收，也不是P19已完成证据。

当前仅生成了执行Prompt与附件分析；没有修改仓库、执行板端清理或验证客户硬件。
