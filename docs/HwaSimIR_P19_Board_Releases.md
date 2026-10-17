# P19 板端版本与回滚

板卡 `192.168.1.116`。根目录 `/userdata/HwaSimIR` 只保留两个完整部署：`releases/P19`（最新）与 `releases/P18`（上一版）。每版都有 coarse、precise 两套独立 Config/network.ini、匹配 ELF、资源、SDK路径及启动入口。当前 ELF 摘要与构建信息以 `logs/p19/deployment/current.json` 为准；P18 ELF 为 `f7dddbdb4e19f4e3676f9e288255c59ee86cf41ae0817a3aa724bc09e044e40d`。

2026-10-17 用户已恢复测试并授权本轮独占板卡。历史08交接回执保留供追溯，不能用它判断现在可断开。最新 ELF、配置摘要以 `logs/p19/deployment/current.json` 为准；本轮DDS分域与延迟结果见 [DDS域配置](HwaSimIR_P19_DDS_Domains.md) 和最新结项记录。

## 启动

分别在两个终端运行，先打开接收端和相应 Domain 的激励 UI：

```sh
cd /userdata/HwaSimIR
./start_current.sh coarse
# 第二个终端：
cd /userdata/HwaSimIR
./start_current.sh precise
```

固定版本可直接 `./start_p19.sh coarse` / `precise`，或 `./start_p18.sh coarse` / `precise`。脚本设置实例工作目录、HWASIMIR_ROOT、channel、network、runtime、ZRDDS_HOME；原实例 launcher 继续检查资源摘要、Xorg :0、Mali 和性能策略。不要将实例 ELF 改成指向版本父目录的软链接：Linux `/proc/self/exe` 路径会改变资源根目录。两实例使用硬链接 ELF，各有独立资源路径。

P19 两路默认 platID=1001、sensorID=1/2、role=coarse/precise。P19 UI 分别发送完整 INIT 决定本回合320raw / 800H264。已保留的 P18 **实际域也是1**，这是本轮读取旧配置和启动日志确认的结果；不能照历史Domain150命令直接连接它。实验150配置单独保存在 `ConfigExamples/P19/lab_domain150`，现场1配置在 `customer_domain1`。P19恢复测试版本支持三域独立配置，当前板端控制150、原视频1、客户1，匹配 `control150_video1` 配置；同域复用Participant、异域在同一Factory下创建。P18仍按旧版域1使用。

## 切换与回滚

先用 UI STOP，等待排空，然后正常退出两个 renderer。以下脚本在存在 renderer 进程时拒绝切换：

```sh
cd /userdata/HwaSimIR
./use_release.sh P18
./start_current.sh coarse
# 第二终端 ./start_current.sh precise
# 停止并退出两路后恢复：
./use_release.sh P19
```

`current` 指向选中的版本，`previous` 固定指向P18。根目录 `Config`、`HwaSim_IR` 兼容入口随 current；`run_precise.sh` 转发当前版precise。身份/端口以各实例 `network.ini` 为准；ID/role/Domain不能通过旧INIT协议远程修改。退出重开VideoDisplay不是renderer开关。

P18回滚实际两路烟测分别保存181帧；证据为 `logs/p18/runs/p19_rollback_coarse_v2`、`p19_rollback_precise_v2`（最终交付索引同时收录）。初次尝试SSH连接重置的失败也保留，没有改写为成功。

## P17板外归档及清理

已核实用户的 `Config_new` 与 `HwaSim_IR_new` 并归档；P17不是按名称猜测为P19。独立归档：

- `logs/p19/board/p17_offboard/user_p17_v2.tar.gz`，SHA256 `499088feff3eaf503e8c6a2bf175d0b88c2e45ed2c06ffd4fc26ebb3c7792cbd`。
- `logs/p19/board/history_offboard/historical_deployments.tar.gz`，SHA256 `b731c917db4e866ce688d0e4638d85f418e6cc3b0620dc0dd3190e455c02ea3f`。

历史总归档逐文件/硬链接核验416728项，另在本机恢复了P18完整双路15463文件。独立P17归档也做了解包摘要核验；这证明资源可恢复，不等同于本轮重跑P17视觉验收。归档含许可，仅保存在用户工作区，**不进入客户交付包**。

若需恢复P17，先核对归档SHA，在独立目录解包 `Config_new` / `HwaSim_IR_new` 及其启动依赖，按归档清单配对恢复为 Config/HwaSim_IR，并重建/核对部署manifest。不要覆盖现有current，也不要把历史许可发给客户。

清理依据 `tools/p19_cleanup_releases.py` 和 `logs/p19/board/cleanup/cleanup_remote.py` 的明确路径白名单。执行前重新核验每个待删文件摘要，拒绝未归档文件、软链接路径和仍被进程使用的目录。删除的是已归档旧部署根，不使用宽泛通配删除，不清空logs，也不删除DDS安装、许可或未知用户文件。精确删除项、after清单在 `logs/p19/board/cleanup/result.json`；本次释放 **584973312字节（约558MiB）**。

最终构建、后续测试会继续产生日志；不要为了展示空目录删除失败证据。旧文档candidate4路径已改为releases/P18，历史日志本身仍保留原命令路径作为证据。

## 恢复测试最终交接

当前P19构建aarch64_24_domains_clean，两路ELF为b7e50a1b4d7d919e1e79b99770a5444e4367457ddd758d84f68c7dc14ab103e3；控制150、原视频1、客户1。测试进程已停止，临时探针库已移除，见logs/p19/board/resumed_handoff.json。云重负载性能仍未解决，详见恢复测试报告。当前可启动使用或断开板卡；P18回滚仍用旧版域1。
