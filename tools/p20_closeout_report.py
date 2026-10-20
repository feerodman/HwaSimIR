"""从当前候选真实回执生成中文结项/性能报告；不提升缺失或失败结论。"""
import argparse,hashlib,json,pathlib,subprocess,re
root=pathlib.Path(__file__).resolve().parents[1];out=root/'logs/p20'
ap=argparse.ArgumentParser();ap.add_argument('--build',required=True);ap.add_argument('--prefix',required=True);ap.add_argument('--windows-log',required=True);a=ap.parse_args()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
receipt=read(out/'deployment'/a.build/'receipt.json')
acceptance=read(out/'acceptance.json')
assert acceptance['build']['elf_sha256']==receipt['build']['elf_sha256'], '先用同一构建生成机器验收'
engineering=acceptance['overall'];optical=acceptance['opticalFixture'];transparent=acceptance['transparentCloudContract']
selected=sorted((out/'runs').glob(a.prefix+'_*/result.json'))
reports=[read(p.parent/'performance.json') for p in selected if (p.parent/'performance.json').exists()]
def table(items):
 lines=['| 场景 | 路 | 新帧 | FPS | P95 / P99 / max ms | >80ms | 队列峰值 | STOP ms | 守恒 |','|---|---|---:|---:|---|---:|---:|---:|---|']
 for r in items:
  for role,s in r['streams'].items():
   l=s.get('windowsStimulusToDecodedImageMs');l=l if isinstance(l,dict) else {}
   vals=' / '.join(f'{l[k]:.2f}' if k in l else '未核验' for k in ['p95','p99','maximum'])
   lines.append(f"| [{r['case']}](../logs/p20/runs/{r['case']}/performance.json) | {role} | {s['renderFrames']} | {s['renderFps']:.3f} | {vals} | {100*l['over80msRatio']:.3f}% | {s['maximumPendingInputFrames']} | {max(s['stopTotalMs'],default=0):.3f} | {s['queueConservation']['pass']} |" if l else f"| {r['case']} | {role} | {s['renderFrames']} | {s['renderFps']:.3f} | 未核验 | — | {s['maximumPendingInputFrames']} | {max(s['stopTotalMs'],default=0):.3f} | {s['queueConservation']['pass']} |")
 return '\n'.join(lines)
baseline=[]
for name in ['baseline24_cloudy_original_r1','baseline24_cloudy_original_r2','baseline24_cloudy_original_r3','baseline24_cloudy_performance_r1']:
 p=out/'runs'/name/'performance.json'
 if p.exists():baseline.append(read(p))
end=['| 场景 | STOP排空 | EOS→parser末输出 | EOS→末帧解码 | EOS→decoder排空 | EOS→MP4完成 |','|---|---:|---:|---:|---:|---:|']
for r in reports:
 if 'precise' not in r['streams']:continue
 s=r['streams']['precise'];e=s.get('endTimingMs',{})
 if e:end.append('| '+r['case']+' | '+' | '.join(f'{v:.3f}' for v in [max(s['stopTotalMs'],default=0),e.get('lastParserOutputNs',0),e.get('lastDecodedNs',0),e.get('drainCompleteNs',0),e.get('mp4CompleteNs',0)])+' |')
perf='''# P20 性能实测报告

当前隔离候选 BUILD；板端Mali硬件路径、MPP、原生粗320 Gray8 raw与精800灰度H264。功能、局部光学、性能和用户视觉分别判定，不能把历史候选性能套用到新构建。

## 统计方法

逐帧接受/执行/渲染/发送/接收/解码/保存计数全部保留，冷启动与结束排空未排除。FPS由真实新帧时间跨度计算；同Windows steady_clock配对激励与接收图像，并核对完整业务digest和路由顺序。解码QImage不是屏幕呈现；未经校准不相减板端和Windows时钟。输入队列曲线、1秒窗口、帧间隔P50/P95/P99/max均在各performance.json及performance_frames.csv，不能只看平均FPS。

短测offset3000/600帧；完整回放4318帧；压力输入18600帧、连续310秒单回合，包含运动和坐标跳变，不RESET消除积压。源行数只是对应fixture长度，普通源不限制名称、行数或SHA。

## P19基线与原/候选调度

BASELINE_TABLE

## 最终候选全部已完成用例

CURRENT_TABLE

原auto_min已请求支持的CPU/GPU/DDR最低频点；performance改变governor，没有提高频率上限。四组合按实际相同场景比较。P95/P99和最大时延全部披露，未删冷启动样本，不能宣称每帧均低于80ms。完整回放及压力结果单列，轻微FPS计时差不等于持续积压。

## 结束链路分段计时（精跟，ms）

END_TABLE

STOP是renderer已接受输入排空；EOS为客户标准H264结束NAL收到；parser、decoder与MP4完成各自记录。raw没有协议EOS，接收工具的4秒idle停止/MP4完成时间不能混作H264末帧延迟。

## 收益与否决实验

1. P19逐像素SI与白云外观分别积分，重云约39.7FPS、队列212～215、STOP约5.1秒。P20世界空间24个解析puffs复用源项/覆盖率，删除重复云通道后短测达到约60FPS。保留全分辨率真实不透明深度裁切，没有ROI清云。
2. 候选18完整回放仍约57.1FPS、积压196/197；310秒压力失败、曾出现transport18。状态探针显示RenderState数约140→9311，CPU绘制线程11→18ms，GPU等待约2ms与真实RGB读回约3.7ms未同步恶化。缓存参数数组（19）、跳过无用cutout（20）、关闭太阳热诊断均未消除增长，不能作为根因修复。
3. 21起每60业务帧在主线程绘制后清除组合缓存并回收无引用RenderState，每次约1.4～2ms。仍被场景引用的状态保留，GPU夹具证明像素/状态身份不变。21连续310秒18600帧两路收齐、约60.006FPS；23整合后重跑结果见上表。0可诊断关闭，不增队列，不降低输入频率，不复制旧帧。
4. 完整回放出现精跟4317/4318。增加有界传输ACK、试验Reader排空均未单独解决；Reader实验已撤回。协议Writer启用SDK push_on_write/TCP_NODELAY后同一21 ELF的双波段均4318，23保留此配置。该问题不是客户raw内部码7的根因证明。
5. 同帧PBO、RGBA、大核绑定未获得净收益，不作为推荐；同步GPU探针改变调度，只用于耗时定位，不作为正式FPS。未超频、未关闭温控。长期温度及每秒有效频率保存在board_monitor.log。
6. 23后来被透明光程反例否决，不能因性能良好发布。30—35已修复透明局部光程，但在同片段仍有约167～324ms的P95；视锥筛选36及状态compose34没有改善，均撤回生产引用。35的R32F精确gather与标量18份浮点输出逐字节一致，仍不足以单独解决主链延迟。
7. 38同步RGB8读回避开Panda重复分量重排/复制，仍保留原生尺寸、底行BGR RAM与图像序号；实际生产类逐像素夹具及主程序双读一致。同一38 ELF开关A/B，精跟P95约100.23/247.77ms、FPS60.506/59.214。没有改成RGBA或PBO，也没有丢帧。当前候选完整矩阵见上表。
8. 40将新透明实例准备接入INIT已有discard-only预热并恢复场景。600帧第一、二业务帧仍存在约70/50ms绘制尖峰，因此不能宣称启动成本完全消除，也不能把全部改进归因于预热。完整回放与长测仍保留这些冷帧。
9. 40第一次310秒压力中，第2262条Windows激励调用前间隔477.56/477.68ms，前一条write仅0.055/0.018ms；板端接收间隔相同量级，随后发生追赶发送。精跟对应绘制15.81ms，队列瞬时27、输入到解码最大508.22ms；没有删去这批帧。证据为`runs/puff40_stress_310s/stimulus_pause_analysis.json`。这定位到发送tick间隙，尚不能归因到具体Windows系统组件；第二轮完整压力另列，不能覆盖第一次的尖峰。

## 光学与客户边界

实际Mali不透明深度、透明气体前缀和相机合并检查的来源及结果见acceptance.json。23总云alpha衰减透明源的反例是历史失败：后续实现改用每个原气体积分步的真实局部前缀，不将历史失败隐藏，也不将局部夹具当全面物理标定。用户视觉PENDING。实际客户原始码7、最终互通、客户硬解、真实辐射标定和屏幕延迟仍NOT_VERIFIED。
'''.replace('BUILD','`'+a.build+'`').replace('BASELINE_TABLE',table(baseline)).replace('CURRENT_TABLE',table(reports)).replace('END_TABLE','\n'.join(end))
(root/'docs/HwaSimIR_P20_Performance_Report.md').write_text(perf,encoding='utf-8')
tests=['| 用例 | 功能结果 | 系统策略恢复 |','|---|---|---|']
for p in selected:
 r=read(p);tests.append(f"| [{p.parent.name}](../logs/p20/runs/{p.parent.name}/result.json) | {r.get('status')} | {r.get('systemPolicyRestored')} |")
final=read(out/'deployment/final_state.json') if (out/'deployment/final_state.json').exists() else None
if final and a.build not in final.get('candidate',''):final=None
handoff=('成对启动烟测已执行；两路退出，系统策略恢复='+str(final['pairLauncher']['systemPolicyRestored'])+'；剩余renderer='+repr(final['remainingRendererProcesses'])+'。实际current='+final['current']+'，previous='+final['previous']+'。' if final else '最终成对启动/退出与系统策略交接尚未完成，不能据此声明可断电。')
close='''# P20 工程交付与未关闭项

**工程状态 ENGINEERING，用户视觉PENDING；未切换正式current。** 已完成新云实现、Windows/AArch64构建、板端测试和真实媒体；功能、性能、局部光学和客户互通分别列出。未完成项不能被其它PASS覆盖。

入口：[离线媒体/性能/验收索引](../logs/p20/index.html)。[机器验收](../logs/p20/acceptance.json)、[性能报告](HwaSimIR_P20_Performance_Report.md)、[云设计](HwaSimIR_P20_Cloud_Rendering_Design.md)、[调度](HwaSimIR_P20_RK3588_Runtime_Tuning.md)、[三域说明](HwaSimIR_P20_DDS_Domain_Guide.md)、[源码索引](HwaSimIR_P20_Source_Index.md)。

## 实际版本

BUILD，ELF `ELFSHA`，buildId `BUILDID`。保留P19稳定24及P18，没有整体回退P17，也没有采用已撤回的P19云25/26。Windows构建日志 `WINDOWSLOG`；AArch64收据和源码快照位于 `logs/p20/build/BUILD/`。未commit/push。

## 逐项范围

| 要求 | 结论与范围 |
|---|---|
| 云算法替换 | 已实现24个世界空间紧支撑椭球的解析部分光程、CPU光照预计算、共享SI/WhiteCloud光学附件；保留LegacyVolume对照 |
| 原生分辨率 | 粗320/精800，未降低最终分辨率，未低分辨率云附件上采样 |
| 不透明小目标/入云 | 实际Mali800 RGBA16F夹具30项PASS，含1/3/7像素、云前中后、边缘、FOV与双云；不同源介质排序仍为有界样本近似 |
| 透明尾焰与云 | 使用原气体每步实际深度云前缀；独立GPU云前中后/跨边缘/关闭云对照及实际Mali检查，来源见机器验收；仍属所测集合 |
| 尾焰/雨雪/深度/HDR | 原资源/尾焰形状/对数深度文件保留，SWIR/MWIR低空代表性真实视频；云交叠失败另列，用户视觉未代签 |
| 性能与调度 | 原/新算法×原/performance四组合；完整双波段和310秒运动压力按实际表格，缓存增长根因已修，短测尾延迟抖动披露 |
| 三域 | 沿用P19架构，板端150/1/1；全150、150/1/2及错误域零接收实测；共享一次Factory/QoS文件 |
| 末帧/多回合 | 标准结束NAL；parser/decoder/MP4分段计时；真实SDK精确1400边界、13秒暂停及双回合，未缩短idle伪装 |
| 同PID恢复 | 应用分支故障注入后STOP/RESET/INIT/ACK/START恢复，initCount=1；真实故障Reader原码7未复现，不能等同SDK故障恢复已证明 |
| 原链与客户链 | 共存、VideoDisplay退出重开、无VideoDisplay、禁用旧输出、客户晚加入重连均有独立证据；私有HwaFrameV2不进入客户分支 |
| 四Topic/像素/身份 | 四Topic短测、Gray8/RGB24、800Gray raw、640000/640001容量边界；任意合法身份/角色与独立模型/源；P19双路UI未改 |
| 同步RGB读回 | 原生RGB8同步读取、现有NEON转Panda底行BGR RAM、modify_ram_image推进身份；双读/全像素/同PID尺寸切换另列补充验收，未引入异步额外帧 |
| 实际客户/硬解/标定 | 全部NOT_VERIFIED；不因本轮内部测试自动关闭 |

## 当前候选真实功能记录

TEST_TABLE

本地协议补充测试见 `logs/p20/local_tests/`；原始四Topic/容量检查日志、H264 SPS/PPS和标准SEI检查在 `logs/p20/`，并在机器验收中保留来源及版本。功能计数PASS不等于视觉或性能PASS。

原VideoDisplay退出/重开分别保存27和4198帧录像（重连从下一可解码关键帧恢复），客户两路仍完整4318帧。实际MP4已逐帧解码、独立解析HwaFrameV2、核对原SEI标注哈希、侧文件哈希和完整JSON字段。Qt会重排侧文件JSON键；两种哈希分别对应各自字节，不能要求它们相等。旧P7严格字节比较的失败日志保留，P20核验结果在两段录像目录的`p20_validation.json`。

## 启动和回退

候选路径 `/userdata/HwaSimIR/candidates/P20_BUILD`。先停止两路renderer及冲突的同Topic生产者；运行：

```sh
python3 /userdata/HwaSimIR/candidates/P20_BUILD/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_BUILD
```

该脚本成对启动、持有系统策略快照，Ctrl+C仅停止其拥有的两路并恢复策略。先在UI STOP等待排空，再退出脚本。它不切换current。普通P19入口仍为 `/userdata/HwaSimIR/start_current.sh coarse` 和另一终端 `precise`；两路退出后 `use_release.sh P19` 可明确选择P19，P18回退用 `use_release.sh P18`（P18域1）。不要对候选目录宽泛删除。

DataDrivenTestQT控制域150，两路各自INIT/ACK，coarse plat1001/sensor1/320raw、precise plat1001/sensor2/800H264，同一公共源按tick推进一次；VideoDisplay域1，客户Demo域1分别TX/LDecode和TX/HD264。实际完整INI/network/QoS导出在 `logs/p20/configs/BUILD/`。环境变量覆盖优先，修改域必须重启；详见三域说明。候选使用PuffHybrid及缓存维护60帧，仓库默认仍LegacyVolume；禁止把候选未通过的光学边界解释成P19正式效果已改变。

## 交接与保留证据

HANDOFF

用户明确授权后，逐文件核对本地归档大小和SHA256才删除完成测试的板端大日志，共释放156439141字节；26个不一致文件保留。没有删除程序、配置、P19/P18、未匹配文件或整个目录。证据在 `logs/p20/board_log_archive/`。后续测试日志改放/tmp并完整拷回本机。

## 透明路径修复与近似边界

历史23将已经混入base的透明尾焰按整条背景光程衰减。均匀slab反例（背景2、云源4、尾焰50、alpha0.5、总tau2）中，云前参考26.8647而旧式为6.9774，云内参考12.3259而旧式为6.9774。结果见 [历史解析反例](../logs/p20/optics/transparent_contract.json)。它明确否决23的透明路径，不能因23性能良好而发布。

当前按原24/40个气体步计算`Cprefix(z)+Tprefix(z)*L`，雨雪使用真实片元深度；后方云留在背景剩余贡献内。R32F参数上传规避实际Mali的RGBA浮点转换断言。30起复用已有RGBA16F云附件，R保存已传输SI、G保存白云源、B保存云SI、A保存合成覆盖率，不重复消光、不逆除T。31进一步共享云相机，保留原尾流/核心/雨雪相对顺序。具体构建身份以本报告首段为准。

35采用精确R32F gather，38启用同步原生RGB读回，40接入INIT透明实例准备。`DirectRgbReadbackAudit`与`HwaSimIRDirectRgbTiming`只用于诊断，正式运行关闭。短测P95偏差、冷启动最大值与长期结果分别披露；“无持续积压”不等于每帧都低于80ms。原模型/尾焰资源不改，透明局部云前缀带来的可见性变化仍需用户看实际媒体确认。

不同源函数的相交云仍有对象级排序近似，光学夹具只证明预设集合内误差。新表示允许形态差异，原生产视频未经增强；用户视觉和真实标定仍分别待确认/未验证。
'''
for key,value in [('ENGINEERING',engineering),('WINDOWSLOG',a.windows_log),('ELFSHA',receipt['build']['elf_sha256']),('BUILDID',receipt['build']['build_id']),('BUILD',a.build),('TEST_TABLE','\n'.join(tests)),('HANDOFF',handoff)]:close=close.replace(key,value)
(root/'docs/HwaSimIR_P20_Closeout.md').write_text(close,encoding='utf-8')
# 保留原保护清单，将当前值与先前记录逐文件比对；不把不同换行等同语义修改。
p=out/'source_preservation.json';old=read(p);baselineProtected=old.get('baselineProtectedHashes',old['protectedCurrentHashes']);current={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in baselineProtected}
old.update(baselineProtectedHashes=baselineProtected,previousProtectedHashes=old['protectedCurrentHashes'],protectedCurrentHashes=current,protectedHashChanges={k:{'before':baselineProtected[k],'after':v} for k,v in current.items() if v!=baselineProtected[k]},
 renderBuild=a.build,windowsExeSha256=hashlib.sha256((root/'HwaSim_IR/Bin/HwaSim_IR.exe').read_bytes()).hexdigest(),productionDdsUiDiff=['DDS/Protocol/DdsStimClient.cpp:有效QoS日志与有界preSTOP传输确认','HwaSim_IR/Bin/Config/DDS/ZRDDS_PROTOCOL_QOS.xml:协议Writer push_on_write=true'])
p.write_text(json.dumps(old,indent=2,ensure_ascii=False),encoding='utf-8')
print('已生成报告；工程状态',engineering,'当前候选用例数',len(selected))
