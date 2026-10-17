收到控制指令：
  军别：1
  挂载平台ID：1001
  仿真指令：开始
  总回合数：1
  当前回合：1
[Stage0] Control command received: command=2, round=1/1
执行开始仿真逻辑...
[SyncStartBoundary] preparedByInit=1 preservedInputs=0
[StartBoundaryPrewarm] pass=1 rendered=1 elapsedMs=37.260 discardOnly=1 publishedVideo=0 acceptedRealtime=0 formalReferenceCommitted=0 temporaryReference=1 beforeRunningStatus=1
[StartBoundaryPrewarm] pass=2 rendered=1 elapsedMs=8.337 discardOnly=1 publishedVideo=0 acceptedRealtime=0 formalReferenceCommitted=0 temporaryReference=1 beforeRunningStatus=1
[FormalReferenceState] event=start referenceGeneration=2 committed=0 sourceSeq=0 action=wait_first_valid_realtime
[CustomerTXRound] round=1 topic=TX/LDecode type=LDDecoder requestedH264=0 actualH264=0 frameIdReset=1 privateSei=0
[OutputRoundStart] round=1 frameSeqReset=1
[VideoEncoder] channel=coarse platID=1001 sensorID=1 pid=8916 requestKeyFrame=1 reason=round_start
[TcpControlForward] skipped=1 reason=ForwardInitControl_false simCommand=2
TCP线程初始化标志已重置，回合重新开始
[VideoStatus] published=1 running=1 codec=raw_gray8 pixelFormat=gray8 topic=HwaSimIR.Video.1001.1.RawGray8 width=320 height=320 fps=60 currentRound=1
仿真开始：当前回合=1
[ProtocolRoute] transport=dds type=realtime accepted=1 localPlatID=1001 localSensorID=1 packetPlatID=1001 packetSensorID=1 reason=exact_match routeCount=1
[ProtocolIngress] transport=dds type=realtime accepted=1 duplicate=0 platID=1001 sensorID=1 ingressCount=1
[FormalReferenceCommit] referenceGeneration=2 commitCount=1 sourceSeq=1 sourceTimeMs=1788665400000.000 platform=40.000,100.000,11000.048 initTemporaryDifferent=0 inputViewValid=0 viewValidUsedForPosition=0 cloudFrameReady=1 groundReferenceZ=-11000.048 prewarmFramePromoted=1 atmosphereCacheInvalidated=0 beforeFirstDraw=1
[MappedTargetShaderKeepalive] enabled=1 mode=vertex_clip_no_fragments publishedPixelContribution=0 targetInputModified=0 tauModified=0 displayGainModified=0
[CameraControl] mode=lookat targetPlatID=3 targetID=3 relPitch=-2.673 relYaw=-2.197 range=22274.244
[AnnotationDraw] test=0 targets=0 size=320x320
实时数据更新完成，时间戳=1788665400000.000ms
[M1 SolarPosition] lat=40.000 lon=100.000 altM=11000.048 UTC=2026-09-06T3.500000h solarAz=136.800761 solarEl=48.573077 solarZenith=41.426923 sunDirection=(0.452934,-0.482339,0.749800) frame=ENU/Panda(X=East,Y=North,Z=Up) positionSource=protocol_realtime_platform solarOverride=0 dateSource=config_fallback protocolTimeSource=time_ms_since_midnight valid=1 fallbackReason=none
[ProtocolRoute] transport=dds type=realtime accepted=1 localPlatID=1001 localSensorID=1 packetPlatID=1001 packetSensorID=1 reason=exact_match routeCount=2
[ProtocolIngress] transport=dds type=realtime accepted=1 duplicate=0 platID=1001 sensorID=1 ingressCount=2
[Stage3] Environment (runtime): hour=3.500000, airTempC=25.000000, visibilityM=6000.000000, humidity=85.000000, wind=8.000000m/s@30.000000deg, weatherCode=1, sunAzimuth=136.800761, sunElevation=48.573077, sunStrength=0.650000, profile=fallback
[Stage7 3DSkyGround] reason=runtime enable=1 band=MWIR envTerrain=0 envSky=1 skyGrayRaw=0.000000 groundGrayRaw=0.000000 skyGrayFinal=0.000000 groundGrayFinal=0.000000 skyGray=0.000000 groundGray=0.000000 skyDome=1 lowerShell=1 groundPlane=0 source=env+band_default
[Stage7 Debug] mode=Off skyVisible=1 groundVisible=1 forcedSkyGray=NA forcedGroundGray=NA
[Stage7 Real3DBackground] skyDome=1 lowerShell=1 flatGroundPlane=0 cameraPos=0.000000,0.000000,0.000000 backgroundMode=real_3d
[Stage7 GroundReference] mode=formal_realtime_reference_altitude groundZOffset=0.000000 activeReferenceAltitudeM=11000.047850 formalReferenceCommitted=1 finalGroundReferenceZ=-11000.047850
[Stage7 SkyDomeDiag] cameraPos=0.000000,0.000000,0.000000 skyDomePos=0.000000,0.000000,0.000000 farClipM=200000.000000 skyRadius=170000.000000 radius=170000.000000 radiusSource=farClip_scaled radiusLessThanFarClip=1 twoSided=1 depthWrite=0 depthTest=0
[Stage7 GroundDiag] groundPos=0.000000,0.000000,0.000000 farClipM=200000.000000 groundSize=170000.000000 lowerShellRadius=170000.000000 groundZOffset=0.000000 envTerrain=0 groundGray=0.000000
[Stage7 Weather] reason=runtime envSky=1 weatherName=Cloudy visibilityM=6000.000000 humidity=85.000000 windV=8.000000 windDir=30.000000 cloudCoverage=0.595000 fogDensity=0.000000 precipitationType=none sunDirectScale=0.550000 skyDiffuseScale=0.900000 targetContrastScale=0.900000 source=UDP+weather_profiles+RuntimeConfig
[WeatherCloud] envSky=1 enabled=1 mode=StreamedWorld3D texture=Config/Weather/Textures/cloud_scattered.png textureFound=1 layers=1 coverage=0.595000 opacity=0.480000 cloudTempK=252.000000 band=MWIR opticalDepth=1.000000 maskChannel=alpha backgroundGray=0.000000 cloudGray=0.000000 baseAltitudeM=10500.000000 thicknessM=800.000000 worldSizeM=30000.000000 tileSizeM=5000.000000 worldUv=absolute_world_xy
[Stage7 Fog] enabled=0 visibilityM=6000.000000 fogDensity=0.000000 fogGray=0.420000 targetContrastScale=0.900000
[Stage7 Precipitation] enabled=0 type=none density=0.000000 speed=0.000000 windV=8.000000 windDir=30.000000 maxHeight=0.000000 transHeight=0.000000
[Stage7 PrecipitationOverlay] mode=Batch active=0 type=none density=0.000000 sensorFovDeg=0.458366 pattern=screen_sparse_low_contrast giantVerticalBars=0 sameOutput=1
[ProtocolRoute] transport=dds type=realtime accepted=1 localPlatID=1001 localSensorID=1 packetPlatID=1001 packetSensorID=1 reason=exact_match routeCount=3
[ProtocolIngress] transport=dds type=realtime accepted=1 duplicate=0 platID=1001 sensorID=1 ingressCount=3
[World3DCloudStreaming] centerReady=1 targetKey=camera_view_ahead center=-149.312561,3898.455078,10818.153320 profile=Cloudy probability=0.350000 candidates=6 culledFrustum=7 culledBudget=0 culledZero=0 cameraZ=0.000000 maxProjectedDiameterPx=0.000000 visibilityMeaning=selected_for_render_not_pixel_proof activeCloudVolumes=7 visibleCloudVolumes=0 averageRaySteps=0.000000 placement=world_grid parent=m_renderRoot
[Stage5 PlumePerf] plumeNodeCount=16 visiblePlumeCount=0 textureLoadCountThisFrame=0 updateHz=30.000000 updatePlumeMs=0.000000 updatePlumeMsMax=0.000000 perfBudgetMs=1.000000
[TargetUpdateCull] enabled=0 total=12 visible=0 skippedBeyondFar=0 skippedShaderApply=0 shaderInputApplyMs=23.214632 stage4Stage5UpdateMs=47.278005
[Stage6 RawAttachment] requested=RTM_bind_or_copy actual=RTM_bind_or_copy directBound=1 formalSiDomain=1 storageFormat=RGBA16F_SI textureStorageVerified=1 actualTextureComponentType=half_float actualTextureComponentWidth=2 actualTextureComponents=4 attachmentCount=1
[NativeRgbCopy] components=3 bytes=307200 PandaReference=EXACT retainedRowOrder=1
[StartupTiming] milestone=first_valid_video_frame elapsedMs=30342.053 sourceSeq=1 width=320 height=320
[RealtimeIngress] ddsRealtimeReceived=5 ddsRealtimeCallbackCount=5 appRealtimeQueued=5 appRealtimeConsumed=1 inputOverwritten=0 inputQueueOverflow=0 currentQueueDepth=4 maxQueueDepth=4 firstSourceSeq=1 lastSourceSeq=5 consumedSourceSeq=1 sourceSeqGapCount=0 callbackInterArrivalUs=16586.283 callbackInterArrivalUsMax=24301.085 arrivalGapLe1ms=0 burstSize1=5 burstSize2=0 burstSize3=0 burstSize4Plus=0 inputBackpressureCount=0 inputBackpressureWaitMs=0.000 maxInputBackpressureWaitMs=0.000 catchUpFrames=0 maxCatchUpBurst=0 queueRecoveryMs=0.000 uniqueInputFrames=0 repeatedStateOutputFrames=0 receivedMinusConsumedOverwrittenPending=0
[CameraControl] mode=lookat targetPlatID=3 targetID=3 relPitch=-2.673 relYaw=-2.197 range=22269.914
[AnnotationDraw] test=0 targets=0 size=320x320
[DdsRawPrep] format=gray8 width=320 height=320 bytes=102400 prepMs=0.294
[VideoOutputProducts] frame=1 needH264=0 h264EncodeCount=0 needJpeg=0 jpegEncodeCount=0 tcpH264=0 ddsH264=0 ddsRaw=1 localRecordH264=0
[DdsFrameProducts] round=1 frameSeq=1 videoMeta=1 annotation=1 metaWriteMs=0.035 annotationWriteMs=0.012 writeErrors=0
[VideoOutputPerf] channel=coarse platID=1001 sensorID=1 frame=1 bgrToNv12Ms=0.000 mppEncodeMs=0.000 ddsPublishCallMs=0.072 ddsBackpressureMs=0.000 annotationMs=0.000 metaAnnotationMs=0.062 localMp4EnqueueMs=0.000 localMp4BackpressureMs=0.000 frameTotalMs=0.655 h264Encodes=0 ddsFrames=1 recordFrames=0
[DdsVideoPerf] topic=HwaSimIR.Video.1001.1.RawGray8 sentSamples=1 sentBytes=102400 writeMsAvg=0.078 writeMsMax=0.078 queueDepth=0 maxQueueDepth=1 backpressureMs=0.000 appCopyMs=0.066 enqueueMs=0.006 queueWaitMs=0.000 writeErrors=0 droppedSamples=0
[DdsVideoTiming] ddsAppCopyMs=0.066 ddsEnqueueMs=0.006 ddsQueueWaitMs=0.000 ddsWriteMs=0.078 ddsTotalSenderMs=0.150 samples=1
********************  2026-09-05 04:22:12:640  ********************
LOGINFO: LEVEL(ERROR)
FUNC: DataWriterImpl.cpp:DataWriterProcSendFailed(LINE: 14845)
(TX/LDecode) Send message failed:7, locator deleted.
*******************************************************************
********************  2026-09-05 04:22:12:640  ********************
LOGINFO: LEVEL(ERROR)
FUNC: DataWriterImpl.cpp:DataWriterProcSendFailed(LINE: 14845)
(TX/LDecode) Send message failed:7, locator deleted.
*******************************************************************
********************  2026-09-05 04:22:12:640  ********************
LOGINFO: LEVEL(ERROR)
FUNC: DataWriterImpl.cpp:DataWriterProcSendFailed(LINE: 14845)
(TX/LDecode) Send message failed:7, locator deleted.
*******************************************************************
********************  2026-09-05 04:22:12:640  ********************
LOGINFO: LEVEL(ERROR)
FUNC: DataWriterImpl.cpp:DataWriterProcSendFailed(LINE: 14845)
(TX/LDecode) Send message failed:7, locator deleted.
*******************************************************************
********************  2026-09-05 04:22:12:640  ********************
LOGINFO: LEVEL(ERROR)
FUNC: DataWriterImpl.cpp:DataWriterProcSendFailed(LINE: 14845)
(TX/LDecode) Send message failed:7, locator deleted.
*******************************************************************
********************  2026-09-05 04:22:12:640  ********************
LOGINFO: LEVEL(ERROR)
FUNC: DataWriterImpl.cpp:DataWriterProcSendFailed(LINE: 14845)
(TX/LDecode) Send message failed:7, locator deleted.
*******************************************************************
********************  2026-09-05 04:22:12:640  ********************
LOGINFO: LEVEL(ERROR)
FUNC: DataWriterImpl.cpp:DataWriterProcSendFailed(LINE: 14845)
(TX/LDecode) Send message failed:7, locator deleted.
*******************************************************************
********************  2026-09-05 04:22:12:640  ********************
LOGINFO: LEVEL(ERROR)
FUNC: DataWriterImpl.cpp:DataWriterProcSendFailed(LINE: 14845)
(TX/LDecode) Send message failed:7, locator deleted.
*******************************************************************
********************  2026-09-05 04:22:12:640  ********************
LOGINFO: LEVEL(ERROR)
FUNC: DataWriterImpl.cpp:DataWriterProcSendFailed(LINE: 14845)
(TX/LDecode) Send message failed:7, locator deleted.
*******************************************************************
********************  2026-09-05 04:22:12:641  ********************
LOGINFO: LEVEL(ERROR)
FUNC: DataWriterImpl.cpp:DataWriterProcSendFailed(LINE: 14845)
(TX/LDecode) Send message failed:7, locator deleted.

*******************************************************************
[Stage7 3DSkyGround] reason=runtime enable=1 band=MWIR envTerrain=0 envSky=1 skyGrayRaw=0.000 groundGrayRaw=0.000 skyGrayFinal=0.000 groundGrayFinal=0.000 skyGray=0.000 groundGray=0.000 skyDome=1 lowerShell=1 groundPlane=0 source=env+band_default
[Stage7 Debug] mode=Off skyVisible=1 groundVisible=1 forcedSkyGray=NA forcedGroundGray=NA
[Stage7 Real3DBackground] skyDome=1 lowerShell=1 flatGroundPlane=0 cameraPos=0.000,2.781,-0.001 backgroundMode=real_3d
[Stage7 GroundReference] mode=formal_realtime_reference_altitude groundZOffset=0.000 activeReferenceAltitudeM=11000.048 formalReferenceCommitted=1 finalGroundReferenceZ=-11000.048
[Stage7 SkyDomeDiag] cameraPos=0.000,2.781,-0.001 skyDomePos=0.000,2.781,-0.001 farClipM=200000.000 skyRadius=170000.000 radius=170000.000 radiusSource=farClip_scaled radiusLessThanFarClip=1 twoSided=1 depthWrite=0 depthTest=0
[Stage7 GroundDiag] groundPos=0.000,2.781,-0.001 farClipM=200000.000 groundSize=170000.000 lowerShellRadius=170000.000 groundZOffset=0.000 envTerrain=0 groundGray=0.000
[Stage5 PlumePerf] plumeNodeCount=16 visiblePlumeCount=0 textureLoadCountThisFrame=0 updateHz=30.000 updatePlumeMs=0.000 updatePlumeMsMax=0.000 perfBudgetMs=1.000
[TargetUpdateCull] enabled=0 total=12 visible=0 skippedBeyondFar=0 skippedShaderApply=0 shaderInputApplyMs=0.419 stage4Stage5UpdateMs=2.814
[CameraControl] mode=lookat targetPlatID=3 targetID=3 relPitch=-2.672 relYaw=-2.197 range=22265.584
[AnnotationDraw] test=0 targets=0 size=320x320
[DdsRawPrep] format=gray8 width=320 height=320 bytes=102400 prepMs=0.228
[VideoOutputProducts] frame=2 needH264=0 h264EncodeCount=0 needJpeg=0 jpegEncodeCount=0 tcpH264=0 ddsH264=0 ddsRaw=1 localRecordH264=0
[DdsFrameProducts] round=1 frameSeq=2 videoMeta=2 annotation=2 metaWriteMs=0.027 annotationWriteMs=0.010 writeErrors=0
[VideoOutputPerf] channel=coarse platID=1001 sensorID=1 frame=2 bgrToNv12Ms=0.000 mppEncodeMs=0.000 ddsPublishCallMs=0.078 ddsBackpressureMs=0.000 annotationMs=0.000 metaAnnotationMs=0.037 localMp4EnqueueMs=0.000 localMp4BackpressureMs=0.000 frameTotalMs=0.485 h264Encodes=0 ddsFrames=1 recordFrames=0
[Stage5 PlumePerf] plumeNodeCount=16 visiblePlumeCount=0 textureLoadCountThisFrame=0 updateHz=30.000 updatePlumeMs=0.000 updatePlumeMsMax=0.000 perfBudgetMs=1.000
[TargetUpdateCull] enabled=0 total=12 visible=0 skippedBeyondFar=0 skippedShaderApply=0 shaderInputApplyMs=0.155 stage4Stage5UpdateMs=1.203
[DdsRawPrep] format=gray8 width=320 height=320 bytes=102400 prepMs=0.244
[VideoOutputProducts] frame=3 needH264=0 h264EncodeCount=0 needJpeg=0 jpegEncodeCount=0 tcpH264=0 ddsH264=0 ddsRaw=1 localRecordH264=0
[DdsFrameProducts] round=1 frameSeq=3 videoMeta=3 annotation=3 metaWriteMs=0.023 annotationWriteMs=0.008 writeErrors=0
[VideoOutputPerf] channel=coarse platID=1001 sensorID=1 frame=3 bgrToNv12Ms=0.000 mppEncodeMs=0.000 ddsPublishCallMs=0.019 ddsBackpressureMs=0.000 annotationMs=0.000 metaAnnotationMs=0.033 localMp4EnqueueMs=0.000 localMp4BackpressureMs=0.000 frameTotalMs=0.439 h264Encodes=0 ddsFrames=1 recordFrames=0
[Stage6 FrameDiag][WARN] NO_VISIBLE_TARGETS consecutiveFrames=5 targetMappedCount=1 hiddenByTargetNum=0 hiddenByTargetViewValid=1 hiddenByWeaponViewValid=1 beyondFarClip=0
[Stage6 FrameDiag][WARN] BACKGROUND_ONLY_FRAME consecutiveFrames=5 skyVisible=1 targetMappedCount=1
********************  2026-09-05 04:22:12:947  ********************
LOGINFO: LEVEL(WARNING)
FUNC: DataWriterImpl.cpp:obtainSample(LINE: 1705)
Waiting sample (sn:1) space failed: 10 in instance null in DataWriter (7401a8c0)(d4220000)(00000000)(1f000003) of topic TX/LDecode
*******************************************************************
[CustomerTX][ERROR] reason=typed_write_failed_or_timeout action=abort_customer_round old_branch_continues=1
[Stage6 FrameDiag][WARN] NO_VISIBLE_TARGETS consecutiveFrames=60 targetMappedCount=1 hiddenByTargetNum=0 hiddenByTargetViewValid=1 hiddenByWeaponViewValid=1 beyondFarClip=0
[Stage6 FrameDiag][WARN] BACKGROUND_ONLY_FRAME consecutiveFrames=60 skyVisible=1 targetMappedCount=1
[VideoStatus] published=1 running=1 codec=raw_gray8 pixelFormat=gray8 topic=HwaSimIR.Video.1001.1.RawGray8 width=320 height=320 fps=60 currentRound=1
[World3DCloudStreaming] centerReady=1 targetKey=camera_view_ahead center=-151.065,4064.564,10815.660 profile=Cloudy probability=0.350 candidates=6 culledFrustum=7 culledBudget=0 culledZero=0 cameraZ=-0.214 maxProjectedDiameterPx=0.000 visibilityMeaning=selected_for_render_not_pixel_proof activeCloudVolumes=7 visibleCloudVolumes=0 averageRaySteps=0.000 placement=world_grid parent=m_renderRoot
[RealtimeIngress] ddsRealtimeReceived=65 ddsRealtimeCallbackCount=65 appRealtimeQueued=65 appRealtimeConsumed=65 inputOverwritten=0 inputQueueOverflow=0 currentQueueDepth=0 maxQueueDepth=4 firstSourceSeq=1 lastSourceSeq=65 consumedSourceSeq=65 sourceSeqGapCount=0 callbackInterArrivalUs=16674.338 callbackInterArrivalUsMax=60402.715 arrivalGapLe1ms=2 burstSize1=57 burstSize2=0 burstSize3=1 burstSize4Plus=0 inputBackpressureCount=0 inputBackpressureWaitMs=0.000 maxInputBackpressureWaitMs=0.000 catchUpFrames=0 maxCatchUpBurst=0 queueRecoveryMs=0.000 uniqueInputFrames=0 repeatedStateOutputFrames=0 receivedMinusConsumedOverwrittenPending=0
[ProtocolRoute] transport=dds type=realtime accepted=1 localPlatID=1001 localSensorID=1 packetPlatID=1001 packetSensorID=1 reason=exact_match routeCount=120
[ProtocolIngress] transport=dds type=realtime accepted=1 duplicate=0 platID=1001 sensorID=1 ingressCount=120
[CameraControl] mode=lookat targetPlatID=3 targetID=3 relPitch=-3.097 relYaw=-2.266 range=21761.182
[Stage6 FrameDiag][WARN] NO_VISIBLE_TARGETS consecutiveFrames=120 targetMappedCount=1 hiddenByTargetNum=0 hiddenByTargetViewValid=1 hiddenByWeaponViewValid=1 beyondFarClip=0
[Stage6 FrameDiag][WARN] BACKGROUND_ONLY_FRAME consecutiveFrames=120 skyVisible=1 targetMappedCount=1
实时数据更新完成，时间戳=1788665401190.000ms
[DdsRawPrep] format=gray8 width=320 height=320 bytes=102400 prepMs=0.268
[VideoOutputProducts] frame=120 needH264=0 h264EncodeCount=0 needJpeg=0 jpegEncodeCount=0 tcpH264=0 ddsH264=0 ddsRaw=1 localRecordH264=0
[DdsFrameProducts] round=1 frameSeq=120 videoMeta=120 annotation=120 metaWriteMs=0.016 annotationWriteMs=0.036 writeErrors=0
[VideoOutputPerf] channel=coarse platID=1001 sensorID=1 frame=120 bgrToNv12Ms=0.000 mppEncodeMs=0.000 ddsPublishCallMs=0.018 ddsBackpressureMs=0.001 annotationMs=0.000 metaAnnotationMs=0.036 localMp4EnqueueMs=0.000 localMp4BackpressureMs=0.000 frameTotalMs=0.393 h264Encodes=0 ddsFrames=1 recordFrames=0
[VideoStatus] published=1 running=1 codec=raw_gray8 pixelFormat=gray8 topic=HwaSimIR.Video.1001.1.RawGray8 width=320 height=320 fps=60 currentRound=1
[DdsVideoPerf] topic=HwaSimIR.Video.1001.1.RawGray8 sentSamples=125 sentBytes=12800000 writeMsAvg=0.073 writeMsMax=0.335 queueDepth=0 maxQueueDepth=1 backpressureMs=0.036 appCopyMs=1.819 enqueueMs=1.018 queueWaitMs=0.036 writeErrors=0 droppedSamples=0
[DdsVideoTiming] ddsAppCopyMs=0.015 ddsEnqueueMs=0.008 ddsQueueWaitMs=0.000 ddsWriteMs=0.073 ddsTotalSenderMs=0.096 samples=125
[RealtimeIngress] ddsRealtimeReceived=126 ddsRealtimeCallbackCount=126 appRealtimeQueued=126 appRealtimeConsumed=126 inputOverwritten=0 inputQueueOverflow=0 currentQueueDepth=0 maxQueueDepth=4 firstSourceSeq=1 lastSourceSeq=126 consumedSourceSeq=126 sourceSeqGapCount=0 callbackInterArrivalUs=16676.427 callbackInterArrivalUsMax=19705.294 arrivalGapLe1ms=0 burstSize1=61 burstSize2=0 burstSize3=0 burstSize4Plus=0 inputBackpressureCount=0 inputBackpressureWaitMs=0.000 maxInputBackpressureWaitMs=0.000 catchUpFrames=0 maxCatchUpBurst=0 queueRecoveryMs=0.000 uniqueInputFrames=0 repeatedStateOutputFrames=0 receivedMinusConsumedOverwrittenPending=0
[Stage6 FrameDiag][WARN] NO_VISIBLE_TARGETS consecutiveFrames=180 targetMappedCount=1 hiddenByTargetNum=0 hiddenByTargetViewValid=1 hiddenByWeaponViewValid=1 beyondFarClip=0
[Stage6 FrameDiag][WARN] BACKGROUND_ONLY_FRAME consecutiveFrames=180 skyVisible=1 targetMappedCount=1
[VideoStatus] published=1 running=1 codec=raw_gray8 pixelFormat=gray8 topic=HwaSimIR.Video.1001.1.RawGray8 width=320 height=320 fps=60 currentRound=1
[RealtimeIngress] ddsRealtimeReceived=187 ddsRealtimeCallbackCount=187 appRealtimeQueued=187 appRealtimeConsumed=186 inputOverwritten=0 inputQueueOverflow=0 currentQueueDepth=1 maxQueueDepth=4 firstSourceSeq=1 lastSourceSeq=187 consumedSourceSeq=186 sourceSeqGapCount=0 callbackInterArrivalUs=16786.395 callbackInterArrivalUsMax=43390.671 arrivalGapLe1ms=14 burstSize1=33 burstSize2=14 burstSize3=0 burstSize4Plus=0 inputBackpressureCount=0 inputBackpressureWaitMs=0.000 maxInputBackpressureWaitMs=0.000 catchUpFrames=0 maxCatchUpBurst=0 queueRecoveryMs=0.000 uniqueInputFrames=0 repeatedStateOutputFrames=0 receivedMinusConsumedOverwrittenPending=0
[World3DCloudCell] action=activate cell=2,2 cloudId=E97833B4760100D4 seed=9409943998163551780 position=5537.328,6321.132,9603.714 radius=405.711,452.886,269.738 density=0.551 template=0 active=1 targetKey=camera_view_ahead
[Stage7 Perf] weatherNodeCount=2 cloudNodeCount=1 precipitationNodeCount=1 textureLoadCountThisFrame=0 updateWeatherNodesMs=0.388 weatherUpdateMs=0.393 totalWeatherMs=0.393 activeCloudVolumes=8 visibleCloudVolumes=0 averageRaySteps=0.000 precipitationMode=Batch
[World3DCloudStreaming] centerReady=1 targetKey=camera_view_ahead center=-155.284,4444.134,10808.225 profile=Cloudy probability=0.350 candidates=6 culledFrustum=8 culledBudget=0 culledZero=0 cameraZ=-0.906 maxProjectedDiameterPx=0.000 visibilityMeaning=selected_for_render_not_pixel_proof activeCloudVolumes=8 visibleCloudVolumes=0 averageRaySteps=0.000 placement=world_grid parent=m_renderRoot
[Stage5 PlumePerf] plumeNodeCount=16 visiblePlumeCount=0 textureLoadCountThisFrame=0 updateHz=30.000 updatePlumeMs=0.000 updatePlumeMsMax=0.000 perfBudgetMs=1.000
[TargetUpdateCull] enabled=0 total=12 visible=0 skippedBeyondFar=0 skippedShaderApply=0 shaderInputApplyMs=0.158 stage4Stage5UpdateMs=0.712
[ProtocolRoute] transport=dds type=realtime accepted=1 localPlatID=1001 localSensorID=1 packetPlatID=1001 packetSensorID=1 reason=exact_match routeCount=240
[ProtocolIngress] transport=dds type=realtime accepted=1 duplicate=0 platID=1001 sensorID=1 ingressCount=240
[CameraControl] mode=lookat targetPlatID=3 targetID=3 relPitch=-3.198 relYaw=-2.328 range=21239.921
[Stage6 FrameDiag][WARN] NO_VISIBLE_TARGETS consecutiveFrames=240 targetMappedCount=1 hiddenByTargetNum=0 hiddenByTargetViewValid=1 hiddenByWeaponViewValid=1 beyondFarClip=0
[Stage6 FrameDiag][WARN] BACKGROUND_ONLY_FRAME consecutiveFrames=240 skyVisible=1 targetMappedCount=1
实时数据更新完成，时间戳=1788665402390.000ms