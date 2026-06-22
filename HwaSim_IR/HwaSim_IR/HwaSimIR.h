#pragma once
#include "pandaFramework.h"
#include "windowFramework.h"
#include "windowProperties.h"
#include "nodePath.h"
#include "loader.h"
#include "texturePool.h"
#include "Core"

#include <iostream>
#include <cstdint>
#include <string>
#include <map>
#include <deque>
#include <mutex>
#include <unordered_set>
#include <vector>
#include <atomic>


#include "pandaSystem.h"
#include "eventHandler.h"
#include "event.h"
#include "keyboardButton.h"  // 用于按键常量


#include "genericAsyncTask.h"
#include "asyncTaskManager.h"

#if defined(__linux__)
#include "UdpCommThread_Linux.h"
#else
#include "UdpCommThread.h"
#endif
#include "CommonData.h"
#include "CommonDefine.h"

#if defined(__linux__)
#include "TcpCommThread_Linux.h"
#else
#include "TcpCommThread.h"
#endif

#include "GeoTransform.h"
#include "AttitudeTransform.h"
#include "IRSimulation.h"
#include "IR/IRAeroThermalModel.h"
#include "IR/IRConfig.h"
#include "IR/IREnginePlumeModel.h"
#include "IR/IRModtranRadianceLut.h"
#include "IR/IRPerfStats.h"
#include "IR/IRSceneMaterialMapper.h"
#include "IR/IRRadianceModelV2.h"
#include "IR/IRRuntimeConfig.h"
#include "IR/IRSensorModel.h"
#include "IR/IRSensorPostProcess.h"
#include "IR/IRTemperatureModel.h"
#include "IR/IRWeatherEffects.h"
#include "Annotation/AnnotationManager.h"

#include "shader.h"             // 新增：着色器支持
#include "clockObject.h"        // 新增：获取全局时间
#include "cardMaker.h"
#include "transparencyAttrib.h"
#include "graphicsOutput.h"
#include "displayRegion.h"
#include "camera.h"
#include "orthographicLens.h"
#include "pandaNode.h"

#include "load_prc_file.h"
#include <condition_variable>
#include <atomic>

/**
* 主应用类
* 封装框架初始化、窗口管理、自定义业务逻辑
*/
class HwaSimIR {
public:
	// 构造函数：初始化HwaSimIR框架+创建主窗口
	HwaSimIR(int argc, char** argv);

	// 析构函数：清理框架和窗口资源
	~HwaSimIR();

	// 启动应用主循环
	void run();

	// 获取主窗口（对外暴露）
	WindowFramework* get_main_window() const;

	// ========== 自定义功能接口 ==========
	// 修改窗口分辨率
	void resize_window(int new_width, int new_height);

	// ========== UDP指令处理接口（供UdpCommThread调用） ==========
	// 处理控制指令（复位/开始/停止）
	void handleControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);
	// 处理成像初始化指令（并发送应答）
	void handleInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& cmd);
	// 处理实时成像数据包
	void handleDisplayData(const BYHWICD::DisplayC2cObjTrackingData& data);

	// 窗口初始化（封装通用配置：键盘、轨迹球、背景等）
	void InitHwaSimIRWindow();
	// 模型初始化
	void InitPlatformModels();

	void ProcessRealSimSceneInitData();

	void ProcessRealSimSceneDrivenData();

	//void ProcessAddRemovePlatData();

	void ProcessAddRemovePakPlatform();   // 处理PlatParamPak平台
	void ProcessAddRemoveWeaponPlatform();// 处理WeaponState平台
	void ProcessAddRemoveTargetPlatform();// 处理TargetState平台

	// 渲染模式控制接口
	// isSync: true为同步渲染(1数据1帧)，false为异步渲染(锁帧)
	// targetFPS: 异步模式下的锁定帧率（设置为0表示不限帧跑满上限）
	void SetRenderMode(bool isSync, double targetFPS);
	void OnTcpFrameSent(
		const IRFrameTelemetry& telemetry,
		std::uint64_t outputOrdinal,
		double flipMs,
		double resizeMs,
		double jpegMs,
		double tcpSendMs,
		int queueDepth,
		double queueWaitMs,
		bool overwritten);
private:
	enum class PendingNetworkCommandType
	{
		Control,
		Init
	};

	struct PendingNetworkCommand
	{
		PendingNetworkCommandType type = PendingNetworkCommandType::Control;
		BYHWICD::ControlP2cX1ObjTrackingCmd controlCmd{};
		BYHWICD::InitP2cObjectTrackingCmd initCmd{};
	};

	void ProcessPendingNetworkCommands();
	void ProcessControlCmdOnMainThread(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);
	void ProcessInitCmdOnMainThread(const BYHWICD::InitP2cObjectTrackingCmd& cmd);
	void LogGraphicsBackend() const;

	struct PendingDisplayFrame
	{
		BYHWICD::DisplayC2cObjTrackingData data{};
		IRFrameTelemetry telemetry;
	};

	struct ShaderInputCachedValue
	{
		int kind = 0; // 1=float vector, 2=int vector
		int count = 0;
		float floats[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		int ints[4] = { 0, 0, 0, 0 };
	};

	struct ShaderInputCacheStats
	{
		std::uint64_t setCount = 0;
		std::uint64_t skipCount = 0;
		double applyMs = 0.0;
	};

	//targetType转PLATFORM_TYPE
	PLATFORM_TYPE TargetTypeToPlatformType(int targetType) const;
	// 注册自定义按键/功能回调
	//void register_custom_functions();

	//void handle_key_event(const Event * event, void * user_data);
	//void handle_key(const Event* event);
	// 静态回调函数（签名严格匹配 EventCallbackFunction）
	static void on_key_event(const Event* event, void* user_data);



	// ================= 红外仿真模块 =================
	PT(Shader) m_irShader;                                  // 缓存编译好的红外着色器
	IRMaterialDatabase m_irMaterialDatabase;                // 材质数据库
	IRAtmosphereModel m_irAtmosphereModel;                  // MODTRAN透过率近似表
	IRRadianceModel m_irRadianceModel;                      // CPU低复杂度辐亮度模型
	IRRadianceModelV2 m_irRadianceModelV2;                  // Stage5A minimal body/hotspot/brightspot radiance debug model
	IRAeroThermalModel m_stage5AeroThermalModel;            // Stage4A: log-only / A-B aerodynamic heating components
	IRModtranRadianceLut m_stage5ModtranRadianceLut;        // Stage5 3B: MODTRAN path/sky/solar log-only source
	IRSensorModel m_irSensorModel;                          // Stage6A sensor geometry and output size model
	IRSensorPostProcess m_irSensorPostProcess;              // Stage6B minimal display output postprocess
	IRSensorProfileDatabase m_irSensorProfiles;             // SensorWave传感器配置
	IRSceneMaterialMapper m_irSceneMaterialMapper;          // 阶段2：目标材质ID纹理与物理材质参数绑定器
	IRWeatherProfile m_irWeatherProfile;                    // 阶段3：温度/太阳高度/方位角环境profile
	IRWeatherEffects m_stage7WeatherEffects;                // 阶段7C：最小天气/云/雾/雨雪效果
	IRTemperatureModel m_irTemperatureModel;                 // 阶段4：发动机热源与特殊亮斑状态模型
	IREnginePlumeModel m_irEnginePlumeModel;                // Stage5E：发动机尾焰/羽流独立热辐射体
	IRRuntimeConfig m_runtimeConfig;                         // 运行配置：env > HwaSimIRRuntime.ini > default
	bool m_enablePerfLog = true;
	bool m_enableIRVerboseLog = false;
	double m_irUpdateHz = 30.0;
	double m_stage7UpdateHz = 10.0;
	double m_stage4UpdateHz = 30.0;
	bool m_stage6FlipInShader = false;
	bool m_stage6FlipInTcpThread = true;
	std::string m_tcpCodecConfig = "auto";
	int m_tcpJpegQuality = 100;
	bool m_tcpJpegGray = false;
	std::string m_tcpJpegGrayConvertMethod = "luma";
	bool m_enableH264Experimental = false;
	bool m_h264FallbackToJpeg = true;
	bool m_jpegPerfABTest = false;
	std::uint64_t m_lastIrUpdateSourceSeq = 0;
	std::uint64_t m_irBreakdownUpdateCounter = 0;
	double m_stage7LastFullUpdateTime = -1.0;
	std::string m_stage7LastFullUpdateKey;
	std::string m_lastIrUpdateState;
	bool m_irMaterialReady = false;
	bool m_irAtmosphereReady = false;
	bool m_irSensorProfilesReady = false;
	bool m_irWeatherReady = false;
	bool m_irTemperatureReady = false;
	bool m_irEnginePlumeReady = false;
	bool m_enableStage4HotspotVisualDebug = false; // 阶段4可视化诊断开关，默认关闭，不改变生产渲染
	bool m_forceStage4BrightSpotVisible = false;   // 阶段4调试：强制特殊亮斑可见
	bool m_forceStage4RearHotspotVisible = false;  // 阶段4调试：强制尾部热源可见
	bool m_stage4LegacyEngineBodyHeating = false;  // Legacy/debug only: engineState heats whole body radiance
	bool m_enableStage5PhysicalPipeline = true;    // Stage5正式分量链路，默认计算，不决定可视化输出
	bool m_enableStage5RadianceDebug = false;      // DebugView != Off 时打开可视化旁路，默认关闭
	int m_stage5DebugViewMode = 0;                 // 0 Off, 1 Body, 2 Reflected, 3 RearHotspot, 4 Plume, 5 BrightSpot, 6 Atmosphere, 7 SensorInput
	std::string m_stage5DebugViewModeName = "Off";
	bool m_stage5LogComponents = false;
	int m_stage5ComponentLogEveryFrames = 120;
	bool m_stage5UseSensorInputForDisplay = false;
	std::string m_stage5SensorInputDisplayMode = "Manual";
	double m_stage5SensorInputDisplayScale = 1.0;
	double m_stage5SensorInputDisplayOffset = 0.0;
	double m_stage5SensorInputDisplayClampMin = 0.0;
	double m_stage5SensorInputDisplayClampMax = 1.0;
	double m_stage5SensorInputDisplayGamma = 1.0;
	IRBand m_stage5SensorInputDisplayBand = IRBand::MidWaveInfrared;
	std::string m_stage5SensorInputDisplayBandName = "MWIR";
	bool m_stage5AeroThermalEnabled = true;
	bool m_stage5ApplyAeroToRadiance = false;
	bool m_stage5AeroDebugLog = false;
	int m_stage5AeroLogEveryFrames = 120;
	double m_stage5AeroApplyScale = 0.25;
	double m_stage5AeroApplyClampBodyDeltaK = 40.0;
	IRBand m_stage5AeroApplyOnlyBand = IRBand::MidWaveInfrared;
	std::string m_stage5AeroApplyOnlyBandName = "MWIR";
	IRAeroThermalOptions m_stage5AeroThermalOptions;
	bool m_enableStage5ModtranRadianceDebug = false;
	bool m_stage5UseModtranPathRuntime = false;
	bool m_stage5UseModtranSkyRuntime = false;
	bool m_stage5UseModtranSolarRuntime = false;
	bool m_stage5ModtranCompareLegacy = false;
	IRBand m_stage5ModtranPathRuntimeBand = IRBand::MidWaveInfrared;
	std::string m_stage5ModtranPathRuntimeBandName = "MWIR";
	std::string m_stage5ModtranPathRuntimeMode = "Off";
	std::string m_stage5ModtranPathUnitMode = "Native";
	double m_stage5ModtranPathScale = 1.0;
	double m_stage5ModtranPathOffset = 0.0;
	double m_stage5ModtranPathClampMin = 0.0;
	double m_stage5ModtranPathClampMax = 10.0;
	double m_stage5ModtranPathBlend = 1.0;
	bool m_stage5ModtranPathABLog = true;
	std::string m_stage5ModtranPreferredSource = "band_lut";
	int m_stage5ModtranLogEveryFrames = 120;
	bool m_stage5ModtranRadianceReady = false;
	std::string m_stage5ModtranRadiancePath;
	std::string m_effectiveRuntimeConfigSources;
	std::string m_stage5DebugToneMapName = "asinh";
	IRRadianceModelV2DebugConfig m_stage5DebugConfig;
	IRRadianceModelV2DebugConfig m_stage5DebugConfigs[5];
	bool m_stage5UseBaseTextureModulation = false;
	bool m_stage5UseBaseTextureModulationByBand[5] = { false, false, false, false, false };
	bool m_stage5DebugDisplayConfigReady = false;
	std::string m_stage5DebugDisplayConfigPath = "fallback";
	bool m_stage5OutputFrameDumpEnabled = false;    // Stage5A.3 smoke-only frame dump, default off
	std::string m_stage5OutputFrameDumpPath;
	int m_stage5OutputFrameDumpEvery = 5;
	int m_stage5OutputFrameCounter = 0;
	int m_stage5OutputFrameDumpWrites = 0;
	bool m_stage5OutputFrameDumpFailureLogged = false;
	int m_stage5ConsecutiveZeroFrames = 0;
	int m_stage5ConsecutiveReflectedZeroFrames = 0;
	IREnginePlumeRuntimeOptions m_stage5PlumeOptions;
	std::string m_stage5PlumeProfilePath = "Config/IRPlume/engine_plume_profiles.json";
	struct Stage5PlumeRuntimeCache
	{
		IREnginePlumeOutput output;
		IREnginePlumeOutput lastAppliedOutput;
		double lastUpdateTime = -1.0;
		bool hasOutput = false;
		bool hasAppliedOutput = false;
		bool lastEngineState = false;
		IRBand lastBand = IRBand::MidWaveInfrared;
	};
	std::map<std::string, Stage5PlumeRuntimeCache> m_stage5PlumeRuntimeCache;
	double m_stage5PlumeUpdateHz = 30.0;
	double m_stage5PlumePerfBudgetMs = 1.0;
	double m_stage5PlumePerfMsTotal = 0.0;
	double m_stage5PlumePerfMsMax = 0.0;
	std::uint64_t m_stage5PlumePerfSamples = 0;
	std::string m_stage5PlumeLastPerfState;
	int m_stage5PlumePerfLogCounter = 0;
	bool m_stage5BodyGrayPathHintLogged = false;
	bool m_stage5BaseTextureFallbackLogged = false;
	bool m_stage5NormalFallbackHintLogged = false;
	struct Stage5ModtranRadianceCacheEntry
	{
		IRModtranRadianceResult result;
	};
	std::map<std::string, Stage5ModtranRadianceCacheEntry> m_stage5ModtranRadianceCache;
	std::map<std::string, std::string> m_lastStage5ModtranRadianceCompareLogState;
	std::map<std::string, std::string> m_lastStage5ModtranPathABLogState;
	std::map<std::string, IRAeroThermalState> m_stage5AeroThermalStateByTarget;
	std::map<std::string, std::string> m_lastStage5AeroThermalLogState;
	std::map<std::string, std::string> m_lastAeroSpeedStateLogState;
	std::map<int, bool> m_stage5ModtranPathRuntimeBandWarned;
	double m_stage5RadianceComponentMsCurrent = 0.0;
	double m_stage5AeroThermalMsCurrent = 0.0;
	double m_stage5ModtranLookupMsCurrent = 0.0;
	std::uint64_t m_stage5ModtranCacheHitCurrent = 0;
	std::uint64_t m_stage5ModtranCacheMissCurrent = 0;
	int m_lastLoggedSensorProtocolBand = -999;
	int m_lastLoggedEnvironmentHour = -999;
	int m_lastLoggedEnvironmentWeather = -999;
	double m_lastStage4UpdateTime = -1.0;
	IRSensorDisplayConfig m_sensorDisplayConfig;
	bool m_sensorDisplayConfigReady = false;
	IRSensorPostProcessConfig m_stage6DisplayConfig;
	bool m_stage6DisplayConfigReady = false;
	bool m_stage6FarClipWarningLogged = false;
	int m_stage6CaptureLogCounter = 0;
	int m_stage6DisplayLogCounter = 0;
	int m_stage6FrameDiagLogCounter = 0;
	int m_stage6NoVisibleTargetFrames = 0;
	std::string m_stage6LastFrameDiagState;
	PT(Shader) m_stage6FinalPostShader;
	PT(Texture) m_stage6RawSceneTex;
	PT(GraphicsOutput) m_stage6RawSceneBuffer;
	PT(DisplayRegion) m_stage6RawSceneRegion;
	PT(DisplayRegion) m_stage6FinalRegion;
	PT(DisplayRegion) m_annotationRegion;
	NodePath m_stage6FinalRoot;
	NodePath m_stage6FinalCameraNode;
	NodePath m_stage6FinalCard;
	NodePath m_annotationRoot;
	NodePath m_annotationCameraNode;
	bool m_stage6FinalPipelineReady = false;
	int m_stage6FinalPipelineLogCounter = 0;
	int m_annotationOverlayLogCounter = 0;
	int m_stage6FinalWidth = 0;
	int m_stage6FinalHeight = 0;
	std::atomic<bool> m_requestExit{ false };
	bool m_enableStage7SkyHorizon = true;
	int m_stage7DebugMode = 0;
	std::string m_stage7DebugModeName = "Off";
	double m_stage7GroundZOffset = 0.0;
	bool m_stage7UseReal3DBackground = true;
	double m_stage7SkyDomeRadius = 42500.0;
	double m_stage7LowerShellRadius = 42500.0;
	double m_stage7GroundReferenceZ = 0.0;
	bool m_stage7NearFarClipWarningLogged = false;
	int m_stage7SkyHorizonLogCounter = 0;
	std::string m_stage7LastSkyHorizonState;
	NodePath m_stage7LowerShellNode;
	bool m_stage7WeatherEnabled = true;
	bool m_stage7CloudLayerEnabled = false;
	bool m_stage7FogEnabled = true;
	bool m_stage7PrecipitationEnabled = false;
	bool m_stage7UseWeatherUdpInput = true;
	int m_stage7PrecipitationMode = 1; // 0 None, 1 ScreenOverlay, 2 Cards
	std::string m_stage7PrecipitationModeName = "ScreenOverlay";
	int m_stage7CloudLayerMaxCards = 0;
	int m_stage7PrecipitationMaxParticles = 0;
	std::string m_stage7WeatherProfilePath = "Config/Weather/weather_profiles.json";
	std::string m_stage7WeatherTextureConfigPath = "Config/Weather/weather_textures.json";
	IRStage7WeatherState m_stage7WeatherState;
	int m_stage7WeatherLogCounter = 0;
	std::string m_stage7LastWeatherState;
	int m_stage7PerfLogCounter = 0;
	std::string m_stage7LastPerfState;
	std::string m_stage7WeatherTextureCacheKey;
	std::string m_stage7CachedCloudTexturePath;
	std::string m_stage7CachedRainTexturePath;
	std::string m_stage7CachedSnowTexturePath;
	PT(Texture) m_stage7CloudTexture;
	PT(Texture) m_stage7RainTexture;
	PT(Texture) m_stage7SnowTexture;
	std::vector<NodePath> m_stage7PrecipitationNodes;

	void InitInfraredShader();                              // 初始化着色器代码
	void InitStage6FinalPostShader();
	void SetupStage6FinalPipeline(int width, int height, const char* reason);
	void SetupAnnotationOverlayRegion(const char* reason);
	void ApplyStage6FinalPostprocessInputs();
	void InitInfraredSimulation();                          // 初始化低复杂度红外全链路参数
	void InitSkyAndCloudScene();                            // 初始化天空背景和粒子云近似层
	void ApplyInfraredShader(NodePath& node, bool isBackground); // 挂载着色器并初始化参数
	std::uintptr_t ShaderInputCacheKey(const NodePath& node) const;
	bool SetShaderInputCached(NodePath& node, const char* name, const LVecBase2f& value, bool force = false);
	bool SetShaderInputCached(NodePath& node, const char* name, const LVecBase3f& value, bool force = false);
	bool SetShaderInputCached(NodePath& node, const char* name, const LVecBase2i& value, bool force = false);
	void InvalidateShaderInputCache(const NodePath& node);
	void ResetShaderInputCounters();
	ShaderInputCacheStats SnapshotShaderInputCounters() const;
	std::string BuildStage7UpdateKey(const IRRuntimeEnvironment& environment) const;
	void UpdateStage7SkyHorizonPositionOnly();
	void ApplyStage6DisplayShaderInputs(NodePath& node);
	void RefreshStage6DisplayShaderInputs();
	void UpdateStage7SkyHorizon(const IRRuntimeEnvironment& environment, const char* reason, bool forceLog);
	void LogStage7SkyGround(const IRRuntimeEnvironment& environment, int envTerrain, int envSky, double skyGrayRaw, double groundGrayRaw, double skyGrayFinal, double groundGrayFinal, double farClipM, double groundReferenceZ, bool forceLog, const char* reason);
	void InitStage7WeatherScene();
	IRStage7WeatherRuntimeInput BuildStage7WeatherInput() const;
	IRStage7WeatherState EvaluateStage7WeatherState(const IRRuntimeEnvironment& environment) const;
	void ApplyStage7WeatherInputs(NodePath& node, const IRStage7WeatherState& weatherState);
	int RefreshStage7WeatherTextureCache(const IRStage7WeatherState& weatherState);
	void UpdateStage7WeatherNodes(const IRStage7WeatherState& weatherState, double currentTime);
	void LogStage7Weather(const IRStage7WeatherState& weatherState, const char* reason, bool forceLog);
	void LogStage7Perf(const IRStage7WeatherState& weatherState, int weatherNodeCount, int cloudNodeCount, int precipitationNodeCount, int textureLoadCountThisFrame, double updateWeatherNodesMs, double totalWeatherMs);
	void CreateEnginePlumeForTarget(TargetPlatformData& targetPlat);
	IREnginePlumeOutput UpdateEnginePlumeForTarget(TargetPlatformData& targetPlat, float dtSec, float ambientTempK, IRBand band, bool targetRenderable, double currentTime, bool* modelUpdated);
	void HideEnginePlume(TargetPlatformData& targetPlat);
	void LogStage5PlumePerf(int plumeNodeCount, int visiblePlumeCount, double updatePlumeMs);
	NodePath LoadPlatformAssetNode(PLATFORM_TYPE type, const PlatformResPath& res); // 加载模型、基础纹理和阶段2材质绑定
	void UpdatePlatformIRStatus();                          // 动态更新红外状态（时间、波段、亮斑等）
	void ApplyRadianceInputs(NodePath& node, const IRObjectRadianceOutput& radiance, int objectKind);
	IRObjectRadianceOutput EvaluateNodeRadiance(const std::string& materialName, const NodePath& node, bool engineOn, bool damaged, bool isSky, bool isCloud, double cloudDensity, double targetAltitudeMeters = -1000000.0);
	std::string MaterialNameForPlatform(PLATFORM_TYPE type) const;
	float EstimateRangeToCamera(const NodePath& node) const;
	bool IsValidTargetStateKey(const BYHWICD::TargetState& targetState) const;
	bool TargetKeyMatches(const BYHWICD::TargetState& targetState, const TargetPlatformData& targetPlat) const;
	bool WeaponTargetKeyMatches(const BYHWICD::WeaponState& weaponState, const TargetPlatformData& targetPlat) const;
	TargetPlatformData* FindTargetPlatformByTargetState(const BYHWICD::TargetState& targetState);
	TargetPlatformData* FindTargetPlatformByWeaponState(const BYHWICD::WeaponState& weaponState);
	TargetPlatformData* FindOrMapTargetPlatform(const BYHWICD::TargetState& targetState, int targetStateIndex);
	void ApplyWeaponCameraControl(BYHWICD::DisplayC2cObjTrackingData& currentData, TargetPlatformData* lookAtTarget);
	std::string Stage4PlatformName(PLATFORM_TYPE type) const;
	bool Stage4WeaponAppliesToTarget(const BYHWICD::WeaponState& weaponState, const TargetPlatformData& targetPlat) const;
	bool ApplyStage4TargetState(TargetPlatformData& targetPlat, const BYHWICD::WeaponState& weaponState, float dtSec, float ambientTempK, const IRObjectRadianceOutput& radiance, bool applyNodeInputs);
	void ApplyStage5RadianceDebug(TargetPlatformData& targetPlat, const IRObjectRadianceOutput& radiance, const IRHotspotState& rearHotspot, const IRBrightSpotState& brightSpot, bool rearEnabledForShader, float rearIntensityForShader, const std::string& targetKey, float dtSec);
	double MapSensorInputToDisplayGray(double sensorInputRadiance) const;
	void LogAeroSpeedState(const TargetPlatformData& targetPlat, bool renderVisible);
	IRAeroThermalOutput EvaluateStage5AeroThermal(TargetPlatformData& targetPlat, IRBand band, float dtSec, const IRRuntimeEnvironment& environment, const std::string& targetKey);
	void LogStage5AeroThermal(const TargetPlatformData& targetPlat, const IRRadianceComponents& components, const IRAeroThermalOutput& aeroOutput);
	IRModtranRadianceResult QueryStage5ModtranRadiance(const TargetPlatformData& targetPlat, const IRRuntimeEnvironment& environment, const IRObjectRadianceOutput& radiance, const std::string& targetKey);
	void LogStage5ModtranRadianceCompare(const TargetPlatformData& targetPlat, const IRRadianceComponents& components, const IRModtranRadianceResult& modtranResult, double rangeKm, double observerAltKm, double targetAltKm);
	void LogStage5ModtranPathAB(const TargetPlatformData& targetPlat, const IRRadianceComponents& components, const IRModtranRadianceResult& modtranResult, double rangeKm, double observerAltKm, double targetAltKm);
	bool Stage5ModtranRadianceCompareEnabled() const;
	bool Stage5ModtranPathRuntimeAffectsImage() const;
	void LogEffectiveRuntimeConfig(const char* reason, int videoFps, int targetNumValid, bool saveMP4En, const char* videoFpsSource, const char* targetNumSource, const char* saveMP4Source) const;
	void ApplySensorOutputConfig(const IRSensorDisplayConfig& config, const char* reason);
	void LogStage6SensorGeometry(const IRSensorDisplayConfig& config, const char* reason) const;
	void ApplyStage6DisplayConfig(const BYHWICD::trackerSensorParam& sensor, const char* reason);
	void LogStage6DisplayConfig(const IRSensorPostProcessConfig& config, const char* reason) const;
	void LogStage6DisplayRoute(const IRSensorPostProcessConfig& config, const char* reason) const;
	void LogStage6FinalPipeline(const char* reason);
	void LogStage6ViewportDiag(const char* reason) const;
	void LogStage6FrameDiag(const BYHWICD::DisplayC2cObjTrackingData& currentData, int targetMappedCount, int targetVisibleCount, int hiddenByTargetNum, int hiddenByTargetViewValid, int hiddenByWeaponViewValid, int beyondFarClipCount);
	bool ResolveAnnotationOutputSize(int& width, int& height) const;
	void RefreshAnnotationOverlay(const BYHWICD::DisplayC2cObjTrackingData& currentData);
	void LogActiveIRSensorProfile(int protocolBand, const char* reason, bool forceLog);
	double CurrentSimulationHour() const;                   // 从实时数据时间换算当前仿真小时，无实时数据时使用正午profile
	IRRuntimeEnvironment BuildRuntimeEnvironment() const;   // 阶段3：按 UDP > profile > 默认值合成环境状态
	void LogActiveIREnvironment(const IRRuntimeEnvironment& environment, const char* reason, bool forceLog);

															// 异步任务：每帧刷新着色器动态参数
	static AsyncTask::DoneStatus shader_update_task(GenericAsyncTask* task, void* data);


	// 新增：主线程场景更新任务
	static AsyncTask::DoneStatus scene_update_task(GenericAsyncTask* task, void* data);

	// 初始化UDP通讯线程
	bool InitUdpThread();
	// 初始化UDP通讯线程
	bool InitTcpThread();
	void LoadNetworkConfig();

	// 核心成员变量
	GraphicsOutput* m_pGraphicsOutput = nullptr;
	GraphicsWindow* m_pGraphicsWindow = nullptr;
	PandaFramework* m_pFramework;       // HwaSimIR框架
	WindowFramework* m_pMainWindow;    // 主窗口实例
	UdpCommThread* m_pUdpThread = nullptr;        // UDP通讯线程
	TcpCommThread* m_pTcpThread = nullptr;		// TCP通信线程
	std::mutex m_mtx;                  // 业务逻辑互斥锁
	std::deque<PendingNetworkCommand> m_pendingNetworkCommands;
	std::deque<PendingDisplayFrame> m_pendingDisplayFrames;
	static const std::size_t kMaxPendingDisplayFrames = 16;
	std::string m_udpLocalIp = "0.0.0.0";
	uint16_t m_udpLocalPort = 8888;
	std::string m_udpRemoteIp = "127.0.0.1";
	uint16_t m_udpRemotePort = 9999;
	std::string m_tcpServerIp = "127.0.0.1";
	uint16_t m_tcpServerPort = 5555;


									   // 模型/纹理路径映射：平台类型 -> 资源路径
	std::map<PLATFORM_TYPE, PlatformResPath> m_platformResMap;

	// 三类平台列表（独立存储）
	std::vector<PakPlatformData> m_pakPlatformList;     // PlatParamPak平台
	std::vector<WeaponPlatformData> m_weaponPlatformList;// WeaponState平台
	std::vector<TargetPlatformData> m_targetPlatformList;// TargetState平台
	AnnotationManager m_annotationManager;              // Stage1：实时窗口标注与内存快照
	double m_annotationUpdateHz = 15.0;
	std::uint64_t m_annotationLastProjectionSourceSeq = 0;
	std::uint64_t m_inputQueueBackpressureLogCount = 0;
	std::map<std::uintptr_t, std::map<std::string, ShaderInputCachedValue> > m_shaderInputCache;
	ShaderInputCacheStats m_shaderInputStats;
	double m_shaderInputFloatEpsilon = 1.0e-5;
	bool m_measureShaderInputApplyTime = false;

	bool m_isInitTargetPlatID;	//TargetState平台初始化ID映射标记

	// 协议数据缓存
	BYHWICD::InitP2cObjectTrackingCmd m_initSceneData;       // 初始化数据缓存
	BYHWICD::DisplayC2cObjTrackingData m_realTimeSceneData;  // 实时数据缓存
	BYHWICD::trackerSensorParam m_sensorParam;               // 传感器参数缓存
	unsigned long long m_stage0DisplayFrameCount;            // 阶段0基线诊断：实时数据包计数
	std::uint64_t m_udpSequence = 0;
	std::atomic<std::uint64_t> m_latestUdpSourceSeq{ 0 };
	IRFrameTelemetry m_currentFrameTelemetry;
	std::uint64_t m_lastCapturedSourceSeq = 0;
	std::atomic<std::uint64_t> m_lastOutputSourceSeq{ 0 };
	std::atomic<bool> m_lastSourceSeqContinuous{ true };
	std::string m_lastStage4InputState;
	std::map<std::string, std::string> m_lastStage4TargetLogState;
	std::map<std::string, std::string> m_lastStage5RadianceComponentLogState;
	int m_lastWeaponDamageFlag = -1;
	IRPerfStats m_perfStats;
	std::atomic<int> m_targetVideoFps{ 0 };

															 // 控制标记
	bool m_isAddPlatform;    // 增删标记：true-增加 false-删除
	std::atomic<bool> m_isSimRunning{ false };     // 仿真运行标记：true-运行 false-停止
	int m_currentRound;      // 当前仿真回合数


	NodePath aim9;
	NodePath m_renderRoot;
	NodePath m_cameraNode;          // 跟随平台的相机节点
	NodePath m_skyNode;
	std::vector<NodePath> m_cloudNodes;
	Camera *m_camera;
	Lens *m_cameraLens;
	bool m_isCameraAttached;        // 相机是否已绑定到平台
	//PT(Loader) m_loader;

	// 保存渲染画面的纹理对象
	PT(Texture) m_renderTex;
	// 定义主线程获取图像的异步任务
	static AsyncTask::DoneStatus capture_task(GenericAsyncTask* task, void* data);


	GeoTransform m_geoTrans;
	AttitudeTransform m_attitudeTrans;


	// 渲染控制变量
	std::atomic<bool> m_bSyncRenderMode{ false };   // 同步模式标志位
	std::atomic<bool> m_syncFrameActive{ false };
	std::condition_variable m_cvNewData; // 用于同步模式的条件变量阻塞
	std::condition_variable m_cvDisplayQueueSpace;
};
