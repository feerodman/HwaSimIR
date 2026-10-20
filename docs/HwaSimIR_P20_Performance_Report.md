# P20 性能实测报告

当前隔离候选 `aarch64_40_prefix_prewarm`；板端Mali硬件路径、MPP、原生粗320 Gray8 raw与精800灰度H264。功能、局部光学、性能和用户视觉分别判定，不能把历史候选性能套用到新构建。

## 统计方法

逐帧接受/执行/渲染/发送/接收/解码/保存计数全部保留，冷启动与结束排空未排除。FPS由真实新帧时间跨度计算；同Windows steady_clock配对激励与接收图像，并核对完整业务digest和路由顺序。解码QImage不是屏幕呈现；未经校准不相减板端和Windows时钟。输入队列曲线、1秒窗口、帧间隔P50/P95/P99/max均在各performance.json及performance_frames.csv，不能只看平均FPS。

短测offset3000/600帧；完整回放4318帧；压力输入18600帧、连续310秒单回合，包含运动和坐标跳变，不RESET消除积压。源行数只是对应fixture长度，普通源不限制名称、行数或SHA。

## P19基线与原/候选调度

| 场景 | 路 | 新帧 | FPS | P95 / P99 / max ms | >80ms | 队列峰值 | STOP ms | 守恒 |
|---|---|---:|---:|---|---:|---:|---:|---|
| [baseline24_cloudy_original_r1](../logs/p20/runs/baseline24_cloudy_original_r1/performance.json) | coarse | 600 | 60.173 | 40.96 / 72.76 / 109.22 | 1.000% | 5 | 33.654 | True |
| [baseline24_cloudy_original_r1](../logs/p20/runs/baseline24_cloudy_original_r1/performance.json) | precise | 600 | 39.723 | 5010.38 / 5192.77 / 5231.55 | 100.000% | 213 | 5197.012 | True |
| [baseline24_cloudy_original_r2](../logs/p20/runs/baseline24_cloudy_original_r2/performance.json) | coarse | 600 | 60.148 | 39.56 / 72.01 / 106.08 | 0.833% | 5 | 33.093 | True |
| [baseline24_cloudy_original_r2](../logs/p20/runs/baseline24_cloudy_original_r2/performance.json) | precise | 600 | 39.843 | 4929.82 / 5105.67 / 5143.62 | 100.000% | 212 | 5108.452 | True |
| [baseline24_cloudy_original_r3](../logs/p20/runs/baseline24_cloudy_original_r3/performance.json) | coarse | 600 | 60.083 | 38.85 / 70.67 / 106.27 | 0.833% | 5 | 34.145 | True |
| [baseline24_cloudy_original_r3](../logs/p20/runs/baseline24_cloudy_original_r3/performance.json) | precise | 600 | 39.696 | 4978.59 / 5159.87 / 5199.98 | 100.000% | 212 | 5165.255 | True |
| [baseline24_cloudy_performance_r1](../logs/p20/runs/baseline24_cloudy_performance_r1/performance.json) | coarse | 600 | 60.094 | 42.04 / 86.20 / 104.23 | 1.333% | 5 | 32.884 | True |
| [baseline24_cloudy_performance_r1](../logs/p20/runs/baseline24_cloudy_performance_r1/performance.json) | precise | 600 | 39.619 | 5041.46 / 5223.40 / 5255.35 | 100.000% | 215 | 5218.201 | True |

## 最终候选全部已完成用例

| 场景 | 路 | 新帧 | FPS | P95 / P99 / max ms | >80ms | 队列峰值 | STOP ms | 守恒 |
|---|---|---:|---:|---|---:|---:|---:|---|
| [puff40_cloudy_600](../logs/p20/runs/puff40_cloudy_600/performance.json) | coarse | 600 | 60.478 | 17.45 / 95.41 / 131.68 | 1.667% | 6 | 32.504 | True |
| [puff40_cloudy_600](../logs/p20/runs/puff40_cloudy_600/performance.json) | precise | 600 | 60.471 | 71.19 / 152.20 / 176.09 | 4.667% | 7 | 32.618 | True |
| [puff40_cloudy_original_r1](../logs/p20/runs/puff40_cloudy_original_r1/performance.json) | coarse | 600 | 60.220 | 17.62 / 54.15 / 89.92 | 0.500% | 4 | 32.642 | True |
| [puff40_cloudy_original_r1](../logs/p20/runs/puff40_cloudy_original_r1/performance.json) | precise | 600 | 60.452 | 72.55 / 150.05 / 175.98 | 4.667% | 7 | 32.718 | True |
| [puff40_cloudy_original_r2](../logs/p20/runs/puff40_cloudy_original_r2/performance.json) | coarse | 600 | 60.221 | 17.53 / 54.51 / 88.18 | 0.500% | 4 | 33.311 | True |
| [puff40_cloudy_original_r2](../logs/p20/runs/puff40_cloudy_original_r2/performance.json) | precise | 600 | 60.521 | 87.84 / 162.63 / 192.53 | 5.500% | 7 | 33.318 | True |
| [puff40_cloudy_original_r3](../logs/p20/runs/puff40_cloudy_original_r3/performance.json) | coarse | 600 | 60.227 | 17.49 / 54.94 / 89.47 | 0.500% | 4 | 32.638 | True |
| [puff40_cloudy_original_r3](../logs/p20/runs/puff40_cloudy_original_r3/performance.json) | precise | 600 | 60.506 | 87.50 / 163.21 / 184.91 | 5.500% | 7 | 32.672 | True |
| [puff40_cloudy_performance_r1](../logs/p20/runs/puff40_cloudy_performance_r1/performance.json) | coarse | 600 | 60.221 | 17.80 / 54.22 / 90.70 | 0.500% | 4 | 32.419 | True |
| [puff40_cloudy_performance_r1](../logs/p20/runs/puff40_cloudy_performance_r1/performance.json) | precise | 600 | 60.458 | 95.83 / 158.36 / 178.75 | 5.833% | 7 | 32.596 | True |
| [puff40_full_mwir_coexist](../logs/p20/runs/puff40_full_mwir_coexist/performance.json) | coarse | 4318 | 60.016 | 15.54 / 18.13 / 49.43 | 0.000% | 1 | 32.323 | True |
| [puff40_full_mwir_coexist](../logs/p20/runs/puff40_full_mwir_coexist/performance.json) | precise | 4318 | 60.047 | 53.50 / 55.83 / 160.41 | 0.278% | 3 | 32.374 | True |
| [puff40_full_swir_no_vd](../logs/p20/runs/puff40_full_swir_no_vd/performance.json) | coarse | 4318 | 60.015 | 15.64 / 17.45 / 36.95 | 0.000% | 1 | 32.535 | True |
| [puff40_full_swir_no_vd](../logs/p20/runs/puff40_full_swir_no_vd/performance.json) | precise | 4318 | 60.048 | 54.32 / 57.17 / 130.69 | 0.185% | 3 | 32.646 | True |
| [puff40_gray_inverse_topics_all150](../logs/p20/runs/puff40_gray_inverse_topics_all150/performance.json) | coarse | 120 | 60.768 | 37.71 / 42.09 / 51.30 | 0.000% | 0 | 33.648 | True |
| [puff40_gray_inverse_topics_all150](../logs/p20/runs/puff40_gray_inverse_topics_all150/performance.json) | precise | 120 | 60.793 | 16.74 / 23.47 / 36.02 | 0.000% | 0 | 33.602 | True |
| [puff40_identity_customer_only](../logs/p20/runs/puff40_identity_customer_only/performance.json) | coarse | 120 | 61.532 | 39.96 / 63.06 / 71.43 | 0.000% | 2 | 32.974 | True |
| [puff40_identity_customer_only](../logs/p20/runs/puff40_identity_customer_only/performance.json) | precise | 120 | 61.923 | 37.14 / 63.17 / 82.44 | 0.833% | 2 | 33.060 | True |
| [puff40_lowalt_band0_sky0](../logs/p20/runs/puff40_lowalt_band0_sky0/performance.json) | coarse | 300 | 60.326 | 15.48 / 27.66 / 39.66 | 0.000% | 1 | 34.133 | True |
| [puff40_lowalt_band0_sky0](../logs/p20/runs/puff40_lowalt_band0_sky0/performance.json) | precise | 300 | 60.801 | 54.83 / 124.53 / 129.76 | 3.000% | 4 | 34.180 | True |
| [puff40_lowalt_band0_sky2](../logs/p20/runs/puff40_lowalt_band0_sky2/performance.json) | coarse | 300 | 60.422 | 18.02 / 50.29 / 58.59 | 0.000% | 2 | 33.522 | True |
| [puff40_lowalt_band0_sky2](../logs/p20/runs/puff40_lowalt_band0_sky2/performance.json) | precise | 300 | 60.922 | 123.08 / 150.82 / 153.20 | 12.667% | 5 | 33.681 | True |
| [puff40_lowalt_band0_sky3](../logs/p20/runs/puff40_lowalt_band0_sky3/performance.json) | coarse | 300 | 60.461 | 18.00 / 49.73 / 57.02 | 0.000% | 2 | 32.482 | True |
| [puff40_lowalt_band0_sky3](../logs/p20/runs/puff40_lowalt_band0_sky3/performance.json) | precise | 300 | 60.968 | 123.25 / 150.77 / 153.65 | 11.667% | 6 | 32.640 | True |
| [puff40_lowalt_band2_sky0](../logs/p20/runs/puff40_lowalt_band2_sky0/performance.json) | coarse | 300 | 60.351 | 16.16 / 29.18 / 43.01 | 0.000% | 1 | 33.605 | True |
| [puff40_lowalt_band2_sky0](../logs/p20/runs/puff40_lowalt_band2_sky0/performance.json) | precise | 300 | 60.825 | 53.91 / 126.44 / 129.76 | 3.333% | 4 | 33.757 | True |
| [puff40_lowalt_band2_sky2](../logs/p20/runs/puff40_lowalt_band2_sky2/performance.json) | coarse | 300 | 60.426 | 17.81 / 49.85 / 58.03 | 0.000% | 2 | 33.513 | True |
| [puff40_lowalt_band2_sky2](../logs/p20/runs/puff40_lowalt_band2_sky2/performance.json) | precise | 300 | 60.937 | 116.51 / 141.42 / 160.06 | 11.333% | 5 | 33.661 | True |
| [puff40_lowalt_band2_sky3](../logs/p20/runs/puff40_lowalt_band2_sky3/performance.json) | coarse | 300 | 60.417 | 17.98 / 50.19 / 57.46 | 0.000% | 2 | 33.338 | True |
| [puff40_lowalt_band2_sky3](../logs/p20/runs/puff40_lowalt_band2_sky3/performance.json) | precise | 300 | 60.913 | 116.22 / 149.87 / 151.11 | 9.333% | 5 | 33.487 | True |
| [puff40_native_raw_600](../logs/p20/runs/puff40_native_raw_600/performance.json) | coarse | 600 | 60.249 | 17.44 / 56.26 / 91.37 | 0.500% | 4 | 33.301 | True |
| [puff40_native_raw_600](../logs/p20/runs/puff40_native_raw_600/performance.json) | precise | 600 | 60.245 | 45.73 / 115.23 / 130.73 | 3.167% | 5 | 33.302 | True |
| [puff40_overcast_sky5](../logs/p20/runs/puff40_overcast_sky5/performance.json) | coarse | 600 | 60.220 | 17.26 / 54.02 / 88.52 | 0.500% | 4 | 33.842 | True |
| [puff40_overcast_sky5](../logs/p20/runs/puff40_overcast_sky5/performance.json) | precise | 600 | 60.514 | 93.71 / 161.13 / 184.52 | 5.667% | 7 | 33.912 | True |
| [puff40_overcast_swir](../logs/p20/runs/puff40_overcast_swir/performance.json) | coarse | 600 | 60.223 | 17.57 / 56.97 / 92.70 | 0.500% | 4 | 33.617 | True |
| [puff40_overcast_swir](../logs/p20/runs/puff40_overcast_swir/performance.json) | precise | 600 | 60.247 | 74.15 / 131.64 / 145.67 | 4.667% | 5 | 33.728 | True |
| [puff40_rgb_inverse_topics](../logs/p20/runs/puff40_rgb_inverse_topics/performance.json) | coarse | 120 | 60.570 | 33.97 / 64.83 / 86.92 | 0.833% | 2 | 34.229 | True |
| [puff40_rgb_inverse_topics](../logs/p20/runs/puff40_rgb_inverse_topics/performance.json) | precise | 120 | 60.726 | 21.10 / 29.85 / 37.55 | 0.000% | 0 | 34.184 | True |
| [puff40_stress_310s](../logs/p20/runs/puff40_stress_310s/performance.json) | coarse | 18600 | 60.008 | 17.41 / 18.87 / 315.30 | 0.317% | 28 | 32.624 | True |
| [puff40_stress_310s](../logs/p20/runs/puff40_stress_310s/performance.json) | precise | 18600 | 60.015 | 56.65 / 184.96 / 508.22 | 1.414% | 27 | 32.726 | True |
| [puff40_stress_310s_repeat](../logs/p20/runs/puff40_stress_310s_repeat/performance.json) | coarse | 18600 | 60.008 | 17.33 / 18.44 / 89.96 | 0.016% | 4 | 34.075 | True |
| [puff40_stress_310s_repeat](../logs/p20/runs/puff40_stress_310s_repeat/performance.json) | precise | 18600 | 60.015 | 56.31 / 58.40 / 214.38 | 0.247% | 9 | 34.089 | True |
| [puff40_three_domains_150_1_2](../logs/p20/runs/puff40_three_domains_150_1_2/performance.json) | coarse | 120 | 60.557 | 16.80 / 18.56 / 32.57 | 0.000% | 0 | 32.641 | True |
| [puff40_three_domains_150_1_2](../logs/p20/runs/puff40_three_domains_150_1_2/performance.json) | precise | 120 | 61.910 | 96.70 / 121.94 / 125.26 | 6.667% | 3 | 32.786 | True |

原auto_min已请求支持的CPU/GPU/DDR最低频点；performance改变governor，没有提高频率上限。四组合按实际相同场景比较。P95/P99和最大时延全部披露，未删冷启动样本，不能宣称每帧均低于80ms。完整回放及压力结果单列，轻微FPS计时差不等于持续积压。

## 结束链路分段计时（精跟，ms）

| 场景 | STOP排空 | EOS→parser末输出 | EOS→末帧解码 | EOS→decoder排空 | EOS→MP4完成 |
|---|---:|---:|---:|---:|---:|
| puff40_cloudy_600 | 32.618 | 0.034 | 0.566 | 2.130 | 71.758 |
| puff40_cloudy_original_r1 | 32.718 | 0.025 | 0.625 | 2.356 | 73.029 |
| puff40_cloudy_original_r2 | 33.318 | 0.069 | 0.814 | 2.917 | 86.313 |
| puff40_cloudy_original_r3 | 32.672 | 0.101 | 0.801 | 2.445 | 81.407 |
| puff40_cloudy_performance_r1 | 32.596 | 0.142 | 0.952 | 3.965 | 87.810 |
| puff40_full_mwir_coexist | 32.374 | 0.115 | 0.692 | 2.459 | 109.677 |
| puff40_full_swir_no_vd | 32.646 | 0.086 | 0.641 | 4.397 | 110.018 |
| puff40_identity_customer_only | 33.060 | 0.046 | 0.222 | 0.943 | 70.875 |
| puff40_lowalt_band0_sky0 | 34.180 | 0.054 | 0.647 | 2.422 | 80.961 |
| puff40_lowalt_band0_sky2 | 33.681 | 0.114 | 1.071 | 3.793 | 86.857 |
| puff40_lowalt_band0_sky3 | 32.640 | 0.056 | 0.683 | 3.168 | 78.897 |
| puff40_lowalt_band2_sky0 | 33.757 | 0.054 | 0.574 | 2.234 | 78.775 |
| puff40_lowalt_band2_sky2 | 33.661 | 0.056 | 0.675 | 2.238 | 83.964 |
| puff40_lowalt_band2_sky3 | 33.487 | 0.086 | 1.539 | 3.570 | 85.180 |
| puff40_overcast_sky5 | 33.912 | 0.049 | 0.775 | 2.810 | 91.279 |
| puff40_overcast_swir | 33.728 | 0.198 | 2.055 | 8.317 | 84.383 |
| puff40_stress_310s | 32.726 | 0.049 | 1.135 | 3.506 | 200.424 |
| puff40_stress_310s_repeat | 34.089 | 0.048 | 0.546 | 2.152 | 207.842 |
| puff40_three_domains_150_1_2 | 32.786 | 0.097 | 1.216 | 3.577 | 90.579 |

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
