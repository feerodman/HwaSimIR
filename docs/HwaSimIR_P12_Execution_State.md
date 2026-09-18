# HwaSimIR P12 执行状态

- 执行窗口：2026-09-17 至 2026-09-18（Asia/Shanghai）
- 工作区：`D:\HwaSimIR`
- 基线 Git HEAD：`739179c234b4040d07a04ff663035697ed272cc8`
- 修改前备份：`logs/p12/baseline/prechange_20260917-192900`
- 验收边界：DDS-only；未重跑 P11 UDP/TCP 大矩阵。
- 硬件所有者：本轮对 RK3588 `192.168.1.116`、DDS identity/topic、活动运行目录和最终配置串行独占。

## 最终里程碑

| 里程碑 | 状态 | 结论 |
|---|---|---|
| P12A 普通启动/无图 | `COMPLETE_WITH_LITERAL_MANUAL_CLICK_NOT_RUN` | 全 Windows 与 Windows+RK3588 普通 DDS 入口均收到并显示真实新帧；严格 `1001/2` 过滤和自动状态发现保留。唯一未执行项是人手在控制 UI 上逐个点击 RESET/INIT/START/STOP。 |
| P12B 成像/资产回归 | `COMPLETE_ENGINEERING_REGRESSION` | `aim120/aim9x/f22/f35` 共 16 个双波段/双视角真实 RK H.264 DDS 解码图通过；通用几何、合成材料、位姿、标注和显示链完成回归。未把这些通用资产冒称具体装备真实红外参数。 |
| P12C 云/2 km/玻璃 | `COMPLETE_ENGINEERING_DATA_CALIBRATION_NOT_VERIFIED` | 真实 LOS 云前后、两个稳定 cloudId 单独关闭、雨雪像素贡献、160 个真实 MODTRAN 2 km 工况、板端 8 个受控玻璃 DDS 案例均完成；真实材料/传感器标定仍为 `NOT_VERIFIED_CALIBRATION`。 |
| P12D 性能/生命周期/交付 | `COMPLETE_WITH_SWIR_COLD_PERFORMANCE_FAIL` | PFM CPU 写盘改为有界异步；普通生命周期和实际 P11→P12 回滚通过；MWIR 60 s 硬门通过，SWIR 60 s 冷启动仍有 3 帧 `>80 ms`，因此整体性能项保持 FAIL。 |

## 总体状态

`PARTIAL`。普通启动、功能链、工程成像回归、2 km 数据、生命周期、部署和回滚均有当前证据；但以下两项不允许被改写为 PASS：

1. SWIR 60 s 冷启动 accepted→writer 有 3 帧超过 80 ms（最大 84.159 ms）。稳态为 0 帧超门，但原硬门包含冷启动。
2. 没有可追溯的真实材料样片光谱、温度、器件 SRF/QE、光学和曝光真值，真实标定保持 `NOT_VERIFIED_CALIBRATION`。

普通启动的人手逐键点击也明确保留为未自动执行动作；可见窗口、零强制 topic/identity/QoS 的普通入口与真实图像已经实测。

## 最终候选身份

| 项目 | SHA-256 / Build ID |
|---|---|
| DataDrivenTestQT.exe | `1b8cdf2d88cf6bfa4d9de4cb0d8249ec3dca67eaba78db66ed47bd0c03d6ad16` |
| DataDrivenTestQT/NetworkConfig.ini | `d00cb810e0ebfdeb4047cc024d2d6abc68e0f17c984d2cb5dc1994fb97e6002e` |
| Windows HwaSim_IR.exe | `53d213a65c572287d23fce2cf120661653c7903e48b332b6bf7678dd609355ea` |
| RK3588 HwaSim_IR ELF | `2d5791360307af746e103d0fdb394a003ef72f70c02f25edcfd3cec3ead2d583` |
| RK3588 Build ID | `25372bcde3528cace876c4d10c55b9bf91046ed7` |
| HwaSimIRRuntime.ini | `ddd59b2d1e336f00edae4808b4eabcedad0b6a4c7739ac274743321ccecae63e` |
| NetworkConfig.ini / precise | `948dce57f5e6a687e7fc8aff644e6033f480b72b030cb10553951ffd589627f4` / `764ea2032ee4f13fbd8acca83c1f7c93e6322886036b5e229d1af6129b4137af` |
| MODTRAN band_lut_si.csv | `6f22d25d32bc00e753b454560af630d09c2e1a130d53835fa505e27d97f5fd99` |
| TargetLib/Targets.json | `16425775fac239f07d2dcf7186db5fccb684b43040fe03da626f6ffff393ad02` |
| VideoDisplay.exe | `a832e28feee08259c829f14b455f484489d19eb210d8b7dbb660c4ba4b40898e` |
| VideoDisplay/NetworkConfig.ini | `b53a17c0678db878008e21e46aacaa844a2f9a54c3adb561f3182d1da0b8388d` |
| DDS QoS | `174ea6aa72511016b16b535e17f67df1c07b3a7bcd4dd970f7665c73ca10a410` |
| 板端最终 Config manifest | `ca19fc482985ff6b0ea75eb2614f4892fae2d5d14687e64edacb033539e977e7` |

## 关键证据

- P12A 普通启动：`logs/p12/p12a/ordinary-mixed-ui-20260917-204319`、`ordinary-mixed-syncgate-20260917-204034`、`mixed-late-restart-20260917-204516`。
- P12B 资产：`logs/p12/p12b/asset_views/audit.json` 与 `index.html`，16/16 PASS。
- P12C 天气/云：`logs/p12/p12c/weather_cloud/audit.json` 与 `index.html`，12/12 PASS。
- P12C 玻璃：`logs/p12/p12c/controlled_glass/evidence/audit.json` 与 `index.html`，8/8 PASS。
- P12C MODTRAN：`logs/p12/p12c/modtran_2km/qc_results.json`，160 个工况、201 个 QC 检查通过；正式表原子追加 96 行。
- P12D 性能：`logs/p12/p12d/performance/ordinary-performance-20260918-043110/suite_summary.json`。
- P12D 生命周期：`logs/p12/p12d/lifecycle/ordinary-lifecycle-20260918-043854/lifecycle_summary.json`，PASS。
- P12D 实际回滚：`logs/p12/p12d/rollback/p12rb-20260918-045424/rollback_summary.json`，PASS。
- P12D 交付包：`deliverables/HwaSimIR_P12/HwaSimIR_P12_Delivery.zip`，454,790,715 字节，1957 个成员，SHA-256 `f112b2fd445915b5b654149e169cef0149685b87f405bf39b46763de094b16a8`，ZIP CRC、关键成员哈希和离线链接独立复核通过。

原失败运行和首次回滚脚本失败现场均保留，未删除、未改名为 PASS。
