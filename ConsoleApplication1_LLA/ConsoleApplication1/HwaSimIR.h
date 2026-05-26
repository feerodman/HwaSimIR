#pragma once
#include "pandaFramework.h"
#include "windowFramework.h"
#include "windowProperties.h"
#include "nodePath.h"
#include "loader.h"
#include "texturePool.h"
#include "core"

#include <iostream>
#include <string>
#include <map>
#include <unordered_set>
#include <vector>


#include "pandaSystem.h"
#include "eventHandler.h"
#include "event.h"
#include "keyboardButton.h"  // 用于按键常量


#include "genericAsyncTask.h"
#include "asyncTaskManager.h"

#include "UdpCommThread.h"
#include "CommonData.h"
#include "CommonDefine.h"

#include "TcpCommThread.h"

#include "GeoTransform.h"
#include "AttitudeTransform.h"
#include "IRSimulation.h"
#include "IR/IRConfig.h"

#include "shader.h"             // 新增：着色器支持
#include "clockObject.h"        // 新增：获取全局时间
#include "cardMaker.h"
#include "transparencyAttrib.h"

#include "load_prc_file.h"
#include <condition_variable>

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
private:
	//targetType转PLATFORM_TYPE
	PLATFORM_TYPE TargetTypeToPlatformType(int targetType);
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
	IRSensorProfileDatabase m_irSensorProfiles;             // SensorWave传感器配置
	bool m_irMaterialReady = false;
	bool m_irAtmosphereReady = false;
	bool m_irSensorProfilesReady = false;
	int m_lastLoggedSensorProtocolBand = -999;

	void InitInfraredShader();                              // 初始化着色器代码
	void InitInfraredSimulation();                          // 初始化低复杂度红外全链路参数
	void InitSkyAndCloudScene();                            // 初始化天空背景和粒子云近似层
	void ApplyInfraredShader(NodePath& node, bool isBackground); // 挂载着色器并初始化参数
	void UpdatePlatformIRStatus();                          // 动态更新红外状态（时间、波段、亮斑等）
	void ApplyRadianceInputs(NodePath& node, const IRObjectRadianceOutput& radiance, int objectKind);
	IRObjectRadianceOutput EvaluateNodeRadiance(const std::string& materialName, const NodePath& node, bool engineOn, bool damaged, bool isSky, bool isCloud, double cloudDensity);
	std::string MaterialNameForPlatform(PLATFORM_TYPE type) const;
	float EstimateRangeToCamera(const NodePath& node) const;
	void LogActiveIRSensorProfile(int protocolBand, const char* reason, bool forceLog);

															// 异步任务：每帧刷新着色器动态参数
	static AsyncTask::DoneStatus shader_update_task(GenericAsyncTask* task, void* data);


	// 新增：主线程场景更新任务
	static AsyncTask::DoneStatus scene_update_task(GenericAsyncTask* task, void* data);

	// 新增：标记是否有最新的网络数据需要更新场景
	bool m_bHasNewData = false;




	// 初始化UDP通讯线程
	bool InitUdpThread();
	// 初始化UDP通讯线程
	bool InitTcpThread();

	// 核心成员变量
	GraphicsOutput* m_pGraphicsOutput;
	GraphicsWindow* m_pGraphicsWindow;
	PandaFramework* m_pFramework;       // HwaSimIR框架
	WindowFramework* m_pMainWindow;    // 主窗口实例
	UdpCommThread* m_pUdpThread;        // UDP通讯线程
	TcpCommThread* m_pTcpThread;		// TCP通信线程
	std::mutex m_mtx;                  // 业务逻辑互斥锁


									   // 模型/纹理路径映射：平台类型 -> 资源路径
	std::map<PLATFORM_TYPE, PlatformResPath> m_platformResMap;

	// 三类平台列表（独立存储）
	std::vector<PakPlatformData> m_pakPlatformList;     // PlatParamPak平台
	std::vector<WeaponPlatformData> m_weaponPlatformList;// WeaponState平台
	std::vector<TargetPlatformData> m_targetPlatformList;// TargetState平台

	bool m_isInitTargetPlatID;	//TargetState平台初始化ID映射标记

	// 协议数据缓存
	BYHWICD::InitP2cObjectTrackingCmd m_initSceneData;       // 初始化数据缓存
	BYHWICD::DisplayC2cObjTrackingData m_realTimeSceneData;  // 实时数据缓存
	BYHWICD::trackerSensorParam m_sensorParam;               // 传感器参数缓存
	unsigned long long m_stage0DisplayFrameCount;            // 阶段0基线诊断：实时数据包计数

															 // 控制标记
	bool m_isAddPlatform;    // 增删标记：true-增加 false-删除
	bool m_isSimRunning;     // 仿真运行标记：true-运行 false-停止
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
	bool m_bSyncRenderMode = false;   // 同步模式标志位
	std::condition_variable m_cvNewData; // 用于同步模式的条件变量阻塞
};


