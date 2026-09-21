# P14 外部 DDS 首有效位置参考

## 生产语义

坐标原点按“INIT 世代”管理：

1. INIT 只创建世代、验证严格身份 1001/2、建立资源和临时预热参考；临时参考不能提交正式原点。
2. START 可以先到，但在首个有效实时平台位置提交前，不生成伪造业务帧。
3. 实时样本先做身份与位置有效性判断，再进入正式原点提交；每世代至多提交一次。
4. `ViewValid` 只表达显示意图，不参与位置有效性判断。`ViewValid=0` 且几何有效的包被接受、消费和逐项映射，只隐藏目标。
5. 首次正式位置与临时预热参考相同则直接提升，避免无谓重建；不同则执行 rebase 和相关缓存失效。
6. 正式原点提交后再收到全零、NaN、超地理范围或错误身份包时，单独拒绝计数，不重置/漂移原点，也不复用旧包伪造新帧。后续合法包继续沿用本世代原点。
7. STOP→START 若仍是同一 INIT 世代，不重复提交原点；新的 INIT 创建新世代。

位置有效性规则在 `DDS/Protocol/RealtimeSampleValidity.h` 中是独立、无 `ViewValid` 依赖的纯判定。零经度或零纬度本身合法；只有成套全零占位特征被识别为占位，避免把合法零地理坐标误杀。

## 独立 fixture

`DDS/HwaSimIRP14FirstValidFixture` 直接构造 DDS 协议样本，不读取或解析 `DataDrivenTestQT/1.txt`，也不从 DataDrivenTestQT 获知轨迹。AArch64 fixture SHA-256 为 `4e213fb4c6eea17e34d2c728aa8fb79ddecde6c7c7e759468363556d7a6fa712`，Build ID 为 `7bd27f78e6e579795259b5bef5add45216bd5927`。

八个独立场景：

| 场景 | 预期 |
| --- | --- |
| START 后无包 | 等待首有效位置；不提前提交原点、不出伪造业务帧 |
| INIT 全零后有效包 | INIT 全零只作临时占位；有效实时位置提交正式原点 |
| 实时全零占位后有效包 | 占位单独拒绝计数；下一合法包提交 |
| INIT 与首包位置不同 | 首包位置提交并触发 rebase/cache invalidation |
| 平台先有效、目标晚到 | 平台先提交；目标后到按完整 `targetType+targetPlatID+targetID` 解析 |
| `ViewValid=0→1` | 两包几何均接受；显示状态独立变化 |
| 有效后再无效再恢复 | placeholder/range/NaN/wrong identity 均不漂移原点；恢复包继续 |
| 合法零地理坐标 | 不被当成全零占位 |

最终 checker 为 PASS，验证提交世代 2/4/6/8/10/12/14/16，各世代恰好一次；在提交之前没有业务帧。SWIR 与 MWIR 各有一个 181/181 帧的边界 DDS MP4，并包含事件表、received、帧索引和关键帧。

## 原始轨迹的显示边界

原 `1.txt` 的前 659 个数据行 `ViewValid=0`，第 660 个数据行首次为 1（源时间 6610 ms）。默认 UI 和普通命令行均按源标志发送；这些早期行不是“无效位置”，因此不能丢弃。界面将以下原因分开显示：

- 等待本世代首个有效平台位置；
- 当前业务包位置有效但显示标志为 0；
- 大气查询超出测量单元/缺少完整顶点；
- 身份、非有限值、全零占位或地理范围非法。

“演示强制显示（测试端覆盖 ViewValid；不修改 1.txt）”是显式测试端开关。它只改发出包的显示意图，不改位置判定、不改文件、不改板端生产过滤，默认关闭。

## 证据入口

- `logs/p14/p14_first_valid_fixture_final5_mwir_check.json`
- `logs/p14/runs/p14_final5_first_valid_swir_r2/`
- `logs/p14/runs/p14_final5_first_valid_mwir/`
- `logs/p14/ui/target_type_click.json`
- `logs/p14/images_final/first_valid_boundary_2band.png`

