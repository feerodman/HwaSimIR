// ConsoleApplication1.cpp : 定义控制台应用程序的入口点。
//
//#include "stdafx.h"

#include "HwaSimIR.h"
#include <chrono>
#include <algorithm>
#include <fstream>

namespace
{
bool FileExists(const std::string& path)
{
	std::ifstream file(path.c_str());
	return file.good();
}

std::string FirstExistingPath(const std::vector<std::string>& paths)
{
	for (size_t i = 0; i < paths.size(); ++i)
	{
		if (FileExists(paths[i]))
		{
			return paths[i];
		}
	}
	return paths.empty() ? std::string() : paths[0];
}
}


// ========== HwaSimIRMainApp 类实现 ==========
HwaSimIR::HwaSimIR(int argc, char** argv)
	: m_pFramework(new PandaFramework()), m_pMainWindow(nullptr)
	,m_isAddPlatform(false), m_isSimRunning(false), m_currentRound(0), m_isCameraAttached(false), m_isInitTargetPlatID(false), m_stage0DisplayFrameCount(0){
	// 关闭垂直同步，突破帧率上限
	load_prc_file_data("", "sync-video false");
	// 初始化HwaSimIR框架（解析命令行参数）
	m_pFramework->open_framework(argc, argv);
	//load_prc_file_data("", "win-size 800 800");
	// 设置初始窗口属性（800x800、标题、置顶）
	WindowProperties init_props;
	init_props.set_size(800, 800);
	init_props.set_title("HwaSimIR");
	init_props.set_foreground(true);


	// 打开主窗口（使用默认GraphicsPipe）
	m_pMainWindow = m_pFramework->open_window(
		init_props,
		0,
		m_pFramework->get_default_pipe()
	);
	//m_pGraphicsOutput = m_pMainWindow->get_graphics_window();
	m_pGraphicsWindow = m_pMainWindow->get_graphics_window();
	m_pGraphicsWindow->request_properties(init_props);
	m_renderRoot = m_pMainWindow->get_render();
	//m_loader = m_pFramework->get_loader();


	SetRenderMode(true, 0);
	//SetRenderMode(false, 25);

	// 窗口初始化+注册自定义功能
	if (m_pMainWindow) {
		InitHwaSimIRWindow();
		//register_custom_functions();
	// 初始化通讯线程
		InitUdpThread();

		InitTcpThread();
	// 初始化平台模型路径
		InitPlatformModels();

		// 编译红外着色器
		InitInfraredShader();
		InitInfraredSimulation();
		InitSkyAndCloudScene();

		// 向全局任务管理器添加红外动态更新任务
		PT(GenericAsyncTask) ir_task = new GenericAsyncTask("IR_UpdateTask", &HwaSimIR::shader_update_task, this);
		AsyncTaskManager::get_global_ptr()->add(ir_task);

		// ================= 新增：向全局任务管理器添加场景更新任务 =================
		PT(GenericAsyncTask) scene_task = new GenericAsyncTask("Scene_UpdateTask", &HwaSimIR::scene_update_task, this);
		AsyncTaskManager::get_global_ptr()->add(scene_task);
	}
	else {
		std::cerr << "Failed to create main window!" << std::endl;
	}
}

HwaSimIR::~HwaSimIR() {

	// 删除所有平台
	m_isAddPlatform = false;
	ProcessAddRemovePakPlatform();
	ProcessAddRemoveWeaponPlatform();
	ProcessAddRemoveTargetPlatform();

	// 停止UDP线程
	if (m_pUdpThread) {
		m_pUdpThread->stop();
		delete m_pUdpThread;
		m_pUdpThread = nullptr;
	}
	if (m_pTcpThread) {
		m_pTcpThread->stop();
		delete m_pTcpThread;
		m_pTcpThread = nullptr;
	}
	// 清理HwaSimIR框架和窗口资源
	if (m_pFramework) {
		m_pFramework->close_all_windows();
		m_pFramework->close_framework();
		delete m_pFramework;
		m_pFramework = nullptr;
	}
	m_pMainWindow = nullptr;
}

//void HwaSimIR::run() {
//	// 启动HwaSimIR主循环（对应Qt的exec()）
//	if (m_pMainWindow && m_pFramework) {
//		std::cout << "应用程序已启动。按ESC键退出。" << std::endl;
//		m_pFramework->main_loop();
//	}
//}

void HwaSimIR::run() {
	if (!m_pMainWindow || !m_pFramework) return;

	std::cout << "应用程序已启动。按ESC键退出。" << std::endl;

	Thread* current_thread = Thread::get_current_thread();

	// 接管主循环
	while (true) {
		// 如果是同步模式，且仿真正在运行，则死等UDP数据
		if (m_bSyncRenderMode && m_isSimRunning) {
			std::unique_lock<std::mutex> lock(m_mtx);

			// wait_for 会阻塞主线程（暂停渲染），直到 UDP 线程调用 m_cvNewData.notify_one()
			// 加一个 100ms 的超时是为了防止：如果 UDP 断线了，主窗口仍然能稍微响应一下系统拖拽、关闭等事件
			m_cvNewData.wait_for(lock, std::chrono::milliseconds(100), [this] { return m_bHasNewData; });

			// 解锁：如果不解锁，接下来的 do_frame 内部任务将无法获取锁，导致死锁
			lock.unlock();
		}

		// 渲染一帧（包含处理网络接收的场景更新任务、UI事件等）
		// 如果返回 false，说明用户点了右上角的 X 或按了 ESC 请求退出
		if (!m_pFramework->do_frame(current_thread)) {
			break;
		}
	}
}

WindowFramework* HwaSimIR::get_main_window() const {
	return m_pMainWindow;
}

// 修改窗口分辨率
void HwaSimIR::resize_window(int new_width, int new_height) {
	if (!m_pMainWindow) return;

	GraphicsWindow* graphics_win = m_pMainWindow->get_graphics_window();
	if (!graphics_win) return;

	// 复制当前窗口属性，仅修改尺寸
	WindowProperties new_props = graphics_win->get_properties();
	new_props.set_size(new_width, new_height);

	// 应用新尺寸并适配渲染布局
	/*if (graphics_win->set_properties(new_props)) {
		m_pMainWindow->adjust_dimensions();
		std::cout << "Window resized to " << new_width << "x" << new_height << std::endl;
	}
	else {
		std::cerr << "Failed to resize window!" << std::endl;
	}*/
}

// 窗口初始化（通用配置）
void HwaSimIR::InitHwaSimIRWindow() {
	m_pMainWindow->enable_keyboard();       // 启用键盘输入
	//m_pMainWindow->setup_trackball();       // 启用鼠标轨迹球（视角操控）
	m_pMainWindow->set_background_type(WindowFramework::BT_gray); // 灰色背景
	//m_pFramework->enable_default_keys();     // 启用默认按键（ESC退出）
	//m_pMainWindow->enable_keyboard();





	//帧率显示器
	PT(FrameRateMeter) meter;
	meter = new FrameRateMeter("frame_rate_meter");
	meter->setup_window(m_pGraphicsWindow);


	// 获取全局事件处理器（根据 eventHandler.h）
	//EventHandler* eh = EventHandler::get_global_event_handler();
	//eh->add_hook("keystroke_escape", &HwaSimIR::on_key_event, this);
	//eh->add_hook("keystroke_space", &HwaSimIR::on_key_event, this);

	m_pFramework->define_key("w", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("s", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("a", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("d", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("arrow_up", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("arrow_down", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("arrow_left", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("arrow_right", "Move forward", &HwaSimIR::on_key_event, this);


	// 配置渲染纹理，让引擎在每次主循环时自动把画面拷贝到内存中
	m_renderTex = new Texture("CaptureTexture");
	m_pGraphicsWindow->add_render_texture(m_renderTex, GraphicsOutput::RTM_copy_ram);

	// 向引擎全局任务管理器添加一个捕获任务，保证在主线程安全运行
	PT(GenericAsyncTask) cap_task = new GenericAsyncTask("CaptureTask", &HwaSimIR::capture_task, this);
	AsyncTaskManager::get_global_ptr()->add(cap_task);

}

void HwaSimIR::InitPlatformModels()
{
	// 清空原有路径映射
	m_platformResMap.clear();

	// 初始化各平台的模型路径和纹理路径
	// 飞机
	m_platformResMap[J20] = {
		"Config/TargetLib/models/J20.obj",
		"Config/TargetLib/models/j20.jpg"
		
	};
	m_platformResMap[F35] = {
		"Config/TargetLib/models/F35C.obj",
		"Config/TargetLib/models/f35c.jpg"
	};

	// 导弹
	m_platformResMap[AIM9] = {
		"Config/TargetLib/models/AIM9.obj",
		"Config/TargetLib/models/aim9.jpg"
	};
	m_platformResMap[AIM120] = {
		"Config/TargetLib/models/AIM120.obj",
		"Config/TargetLib/models/aim120.jpg"
	};

	std::cout << "平台模型路径初始化完成，共加载" << m_platformResMap.size() << "种平台资源" << std::endl;


	//Filename aim9_path = Filename::from_os_specific("F:/HwaSim_IR/EdgeSideIRSim_vs/ConsoleApplication1/Bin/Config/TargetLib/models/AIM9.obj");
	//Filename texture_path = Filename::from_os_specific("F:/HwaSim_IR/EdgeSideIRSim_vs/ConsoleApplication1/Bin/Config/TargetLib/models/aim9.jpg");
#if 0

	Filename aim9_path = Filename::from_os_specific("Config/TargetLib/models/AIM9.obj");
	Filename aim9_texture_path = Filename::from_os_specific("Config/TargetLib/models/aim9.jpg");

	aim9 = m_pMainWindow->load_model(m_renderRoot, aim9_path);
	PT(Texture) textureaim9 = TexturePool::load_texture(aim9_texture_path);
	if (textureaim9 != nullptr)
	{
		aim9.set_texture(textureaim9);
	}
	aim9.set_pos(1000, 0, 0);
	aim9.set_hpr(0, 0, 0);

#endif // 0

	//aim9.set_scale(0.1f, 0.1f, 0.1f);


	//m_camera = m_pMainWindow->get_camera(1);J20 textureJ20
	//m_cameraNode.set_pos(0, -20, 10);      // (x, y, z)
	//m_cameraNode.look_at(LPoint3f(0, 0, 0)); // 朝向原点
	//m_cameraNode.set_hpr(0, 0, 0);         // 可选：重置旋转


	// 获取主窗口的相机组节点
	m_cameraNode = m_pMainWindow->get_camera_group();
	//m_cameraNode.reparent_to(m_pMainWindow->get_render());
	m_cameraNode.reparent_to(m_renderRoot);
	//LPoint3 x = (0, 0, 0);
	//int num = m_pMainWindow->get_num_cameras();
	//std::cout << "get_num_cameras" << num << std::endl;
	m_cameraNode.set_pos(0,0,0); // 往后15单位，往上8单位
	m_cameraNode.set_hpr(0,0,0); 			 // 让相机始终看向平台模型中心
	//m_cameraNode.look_at(aim9);

	m_camera = m_pMainWindow->get_camera(0);
	m_cameraLens = m_camera->get_lens();
	m_cameraLens->set_fov(0.1, 0.1);


}

void HwaSimIR::ProcessRealSimSceneInitData()
{
	// 确保平台模型路径已初始化
	if (m_platformResMap.empty())
	{
		InitPlatformModels();
	}

	// 缓存传感器参数
	m_sensorParam = m_initSceneData.trackingInit.trackerSensor[0];
	std::cout << "传感器参数初始化完成，传感器ID=" << m_initSceneData.sensorID << std::endl;

	// 设置增删标记为"增加"
	m_isAddPlatform = true;

	// 调用增删逻辑生成平台
	ProcessAddRemovePakPlatform();
	ProcessAddRemoveWeaponPlatform();
	ProcessAddRemoveTargetPlatform();
	BYHWICD::SpatialState spatial;
	spatial = m_initSceneData.platParam[0].spatial;
	m_geoTrans.InitReferencePoint(spatial.lat, spatial.lon, spatial.alt);

	std::cout << "初始化仿真中心原点：ID=" << m_initSceneData.platParam[0].id
		<< " 位置(" << spatial.lat << "," << spatial.lon << "," << spatial.alt << ")" << std::endl;

	std::cout << "成像初始化完成：军别=" << m_initSceneData.JB
		<< " 挂载平台ID=" << m_initSceneData.platID
		<< " 仿真回合=" << m_currentRound << std::endl;
}

void HwaSimIR::ProcessRealSimSceneDrivenData()
{
	// 仿真未运行时不处理
	if (!m_isSimRunning)
	{
		return;
	}

	// ================= 新增：局部拷贝当前帧数据，防止在计算过程中被 UDP 线程覆盖 =================
	BYHWICD::DisplayC2cObjTrackingData currentData;
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		currentData = m_realTimeSceneData;
	}

	// 更新PlatParamPak平台（核心：platLoc映射到飞机平台）
	for (auto& pakPlat : m_pakPlatformList)
	{
		if (!pakPlat.isExist || pakPlat.platID != currentData.platID) continue;

		// 更新平台位置/姿态（从实时数据的platLoc读取）
		const BYHWICD::SpatialState& platSpatial = currentData.platLoc;


		/*double px, py, pz;
		m_geoTrans.Wgs84ToPandaXYZ(platSpatial, px, py, pz);
		pakPlat.nodePath.set_pos(px, py, pz);*/

		LMatrix4f exactTransform = m_geoTrans.GetPandaMatrix(platSpatial);

		pakPlat.nodePath.set_mat(LMatrix4(exactTransform));


		//pakPlat.nodePath.set_pos(platSpatial.lat, platSpatial.lon, platSpatial.alt);
		//pakPlat.nodePath.set_hpr(-platSpatial.yaw, platSpatial.pitch, platSpatial.roll);


		// 更新协议参数缓存
		pakPlat.platParam.spatial = platSpatial;
		/*std::cout << "更新PlatParamPak平台位置：ID=" << pakPlat.platID
			<< " 位置(" << platSpatial.lat << "," << platSpatial.lon << "," << platSpatial.alt << ")" 
			<< " 姿态(" << -platSpatial.yaw << "," << platSpatial.pitch << "," << platSpatial.roll << ")" << std::endl;*/
		/*std::cout << "更新PlatParamPak平台转换后位置：ID=" << pakPlat.platID
			<< " 位置(" << px << "," << py << "," << pz << ")"
			<< " 姿态(" << -platSpatial.yaw << "," << platSpatial.pitch << "," << platSpatial.roll << ")" << std::endl;*/
	}

	// 更新WeaponState平台
	for (auto& weaponPlat : m_weaponPlatformList)
	{
		if (!weaponPlat.isExist || weaponPlat.platID != currentData.platID) continue;

		// 更新武器状态
		weaponPlat.weaponState = currentData.weaponState;
		std::cout << "更新WeaponState平台：目标ID=" << weaponPlat.platID
			<< " 毁伤状态=" << weaponPlat.weaponState.damageFlag << std::endl;
	}

	// 更新TargetState平台
	int validTargetNum = std::min(currentData.targetNumValid, 5);
	//if (!m_isInitTargetPlatID)
	//{
	//	for (int i = 0; i < 5; ++i)//todo
	//	{
	//		const BYHWICD::TargetState& targetState = currentData.targetState[i];
	//		for (auto& targetPlat : m_targetPlatformList)
	//		{
	//			PLATFORM_TYPE platType = TargetTypeToPlatformType(targetState.targetType);
	//			if (targetPlat.isExist && targetPlat.type == platType && targetPlat.platID == i)
	//			{
	//				targetPlat.platID = targetState.targetID;
	//			}
	//		}
	//	}
	//	m_isInitTargetPlatID = true;
	//}
	if (!m_isInitTargetPlatID)
	{
		// 记录已完成ID映射的平台索引，避免重复分配
		std::unordered_set<size_t> mappedPlatformIndices;
		// 按顺序遍历每个传入的targetState，为其匹配第一个对应类型的平台
		for (int i = 0; i < validTargetNum; ++i)
		{
			const BYHWICD::TargetState& targetState = currentData.targetState[i];
			PLATFORM_TYPE targetPlatType = TargetTypeToPlatformType(targetState.targetType);
			int targetID = targetState.targetID;

			// 遍历平台列表，寻找第一个「类型匹配+未被映射+存在」的平台
			for (size_t platIdx = 0; platIdx < m_targetPlatformList.size(); ++platIdx)
			{
				auto& targetPlat = m_targetPlatformList[platIdx];
				// 匹配条件：平台存在 + 类型一致 + 未被分配过ID
				if (targetPlat.isExist
					&& targetPlat.type == targetPlatType
					&& mappedPlatformIndices.find(platIdx) == mappedPlatformIndices.end())
				{
					// 将平台的platID设置为当前targetState的targetID
					targetPlat.platID = targetID;
					// 标记该平台已映射，避免重复分配
					mappedPlatformIndices.insert(platIdx);

					std::cout << "[初始化ID映射] 目标ID=" << targetID
						<< " 平台类型=" << static_cast<int>(targetPlatType)
						<< " 平台索引=" << platIdx << std::endl;
					break; // 找到第一个匹配的平台，退出当前遍历
				}
			}
		}
		m_isInitTargetPlatID = true;
	}
	

	// ========== 更新TargetState平台状态和位置 ==========
	for (int i = 0; i < validTargetNum; ++i)
	{
		const BYHWICD::TargetState& targetState = currentData.targetState[i];
		const BYHWICD::SpatialState& platSpatial = currentData.platLoc;
		PLATFORM_TYPE platType = TargetTypeToPlatformType(targetState.targetType);
		int targetID = targetState.targetID;
		int targetPlatID = currentData.weaponState.targetPlatID;
		for (auto& targetPlat : m_targetPlatformList)
		{
			// 匹配条件：平台存在 + platID匹配 + 类型匹配
			if (!targetPlat.isExist
				|| targetPlat.platID != targetID
				|| targetPlat.type != platType)
			{
				continue;
			}

			// 更新目标状态和位置
			targetPlat.targetState = targetState;
			const BYHWICD::SpatialState& spatial = targetState.targetLoc;

			//double px, py, pz;
			//m_geoTrans.Wgs84ToPandaXYZ(spatial, px, py, pz);
			//targetPlat.nodePath.set_pos(px, py, pz);
			//targetPlat.nodePath.set_pos(spatial.lat, spatial.lon, spatial.alt);
			//targetPlat.nodePath.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);

			LMatrix4f exactTransform = m_geoTrans.GetPandaMatrix(spatial);

			targetPlat.nodePath.set_mat(LMatrix4(exactTransform));
			std::cout << " targetPlatID(" << targetPlatID << "）----targetID（" << targetID << "）"<< std::endl;
			if (targetPlatID == targetID)
			{
				double range, rel_pitch, rel_yaw;
				m_attitudeTrans.computeRelativePosition(platSpatial.lat, platSpatial.lon, platSpatial.alt, platSpatial.roll, platSpatial.pitch, platSpatial.yaw,
					spatial.lat, spatial.lon, spatial.alt, range, rel_pitch, rel_yaw);

				m_cameraNode.set_hpr(-rel_yaw, rel_pitch, 0.0);
				LVecBase3 hpr = m_cameraNode.get_hpr();
				//std::cout << " 获取传感器姿态(" << hpr[0] << "," << hpr[1] << "," << hpr[2] << ")" << std::endl;
				//std::cout << " 获取传感器计算姿态(" << -rel_yaw << "," << rel_pitch << "," << 0.0 << ")" << std::endl;
			}
			

			

			//m_cameraNode.look_at(targetPlat.nodePath);

			

			/*std::cout << "更新TargetState平台：目标ID=" << targetState.targetID
				<< " 位置(" << spatial.lat << "," << spatial.lon << "," << spatial.alt << ")"  
				<< " 姿态(" << -spatial.yaw << "," << spatial.pitch << "," << spatial.roll << ")" << std::endl;*/
		/*	std::cout << "更新TargetState平台转换后位置：ID=" << targetState.targetID
				<< " 位置(" << px << "," << py << "," << pz << ")"
				<< " 姿态(" << -spatial.yaw << "," << spatial.pitch << "," << spatial.roll << ")" << std::endl;*/

			break; // 一个目标ID仅对应一个平台，找到后退出遍历（提升效率）
		}
	}

	//for (int i = 0; i < validTargetNum; ++i)
	//{
	//	const BYHWICD::TargetState& targetState = currentData.targetState[i];
	//	for (auto& targetPlat : m_targetPlatformList)
	//	{
	//		PLATFORM_TYPE platType = TargetTypeToPlatformType(targetState.targetType);
	//		if (!targetPlat.isExist || targetPlat.platID != targetState.targetID || targetPlat.type != platType) continue;

	//		// 更新目标状态和位置
	//		targetPlat.targetState = targetState;
	//		const BYHWICD::SpatialState& spatial = targetState.targetLoc;


	//		double px, py, pz;
	//		m_geoTrans.Wgs84ToPandaXYZ(spatial, px, py, pz);
	//		targetPlat.nodePath.set_pos(px, py, pz);


	//		//targetPlat.nodePath.set_pos(spatial.lat, spatial.lon, spatial.alt);
	//		targetPlat.nodePath.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);
	//		m_cameraNode.look_at(targetPlat.nodePath);

	//		
	//		LVecBase3 hpr = m_cameraNode.get_hpr();
	//		std::cout << " 获取传感器姿态(" << hpr[0] << "," << hpr[1] << "," << hpr[2] << ")" << std::endl;


	//		std::cout << "更新TargetState平台：目标ID=" << targetState.targetID
	//			<< " 位置(" << spatial.lat << "," << spatial.lon << "," << spatial.alt << ")"  
	//			<< " 姿态(" << -spatial.yaw << "," << spatial.pitch << "," << spatial.roll << ")" << std::endl;
	//		std::cout << "更新TargetState平台转换后位置：ID=" << targetState.targetID
	//			<< " 位置(" << px << "," << py << "," << pz << ")"
	//			<< " 姿态(" << -spatial.yaw << "," << spatial.pitch << "," << spatial.roll << ")" << std::endl;
	//	}
	//}

	std::cout << "实时数据更新完成，时间戳=" << currentData.time << "ms" << std::endl;
}


PLATFORM_TYPE HwaSimIR::TargetTypeToPlatformType(int targetType)
{
	switch (targetType)
	{
	case 0x00: return NONE;
	case 0x11: return F35; // 飞机类型默认F35，可根据ID区分F35/J20
	case 0x22: return AIM120;
	case 0x33: return AIM9;
	case 0x44: return MMD;
	default: return NONE;
	}
}
// 处理PlatParamPak平台增删（飞机平台）
void HwaSimIR::ProcessAddRemovePakPlatform()
{
	if (m_isAddPlatform)
	{
		// 增加PlatParamPak平台（飞机）
		std::cout << "开始生成PlatParamPak平台，有效数：" << m_initSceneData.platNumValid << std::endl;

		for (int i = 0; i < m_initSceneData.platNumValid; ++i)
		{
			if (i >= 2) break; // 协议中platParam最多2个

			const BYHWICD::PlatParamPak& platParam = m_initSceneData.platParam[i];
			// 根据飞机编号映射平台类型
			PLATFORM_TYPE platType = TargetTypeToPlatformType(platParam.type);
			if (platType == NONE)
			{
				std::cerr << "无效的飞机编号：" << platParam.id << "，跳过生成" << std::endl;
				continue;
			}

			auto resIter = m_platformResMap.find(platType);
			if (resIter == m_platformResMap.end())
			{
				std::cerr << "未找到平台类型" << platType << "的资源路径，跳过" << std::endl;
				continue;
			}
			Filename modelPath = Filename::from_os_specific(resIter->second.modelPath);
			Filename texturePath = Filename::from_os_specific(resIter->second.texturePath);

			// 加载模型
			NodePath modelNode = m_pMainWindow->load_model(m_renderRoot, modelPath);
			if (modelNode.is_empty())
			{
				std::cerr << "模型加载失败：" << modelPath << std::endl;
				continue;
			}

			// 加载纹理
			PT(Texture) texture = TexturePool::load_texture(texturePath);
			if (texture != nullptr)
			{
				modelNode.set_texture(texture);
			}

			// 初始化PakPlatformData
			PakPlatformData newPakPlat;
			newPakPlat.type = platType;
			newPakPlat.platID = platParam.id;
			newPakPlat.platParam = platParam; // 直接拷贝协议参数
			newPakPlat.isExist = true;
			newPakPlat.nodePath = modelNode;

			// 设置初始位置/姿态（从协议SpatialState读取）
			const BYHWICD::SpatialState& spatial = platParam.spatial;
			modelNode.set_pos(spatial.lat, spatial.lon, spatial.alt);
			modelNode.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);

			// 添加到列表并显示
			m_pakPlatformList.push_back(newPakPlat);
			modelNode.show();

			// 【修改点 3】：给飞机挂载红外着色器 (false表示非背景)
			ApplyInfraredShader(modelNode, false);

			std::cout << "PlatParamPak平台生成成功：类型=" << platType << " ID=" << platParam.id << std::endl;
			// ========== 绑定相机到第一个平台 ==========
			if (i==0 && !m_isCameraAttached) {
				//m_cameraNode = m_pMainWindow->get_camera_group();
				//m_camera = m_pMainWindow->get_camera();
				m_cameraNode.reparent_to(modelNode);
				m_cameraNode.set_pos(0, 0, 0); // 往后15单位，往上8单位
				m_cameraLens->set_fov(m_sensorParam.coarseTrackResolution, m_sensorParam.preciseTrackResolution);
				//m_cameraNode.look_at(modelNode);
												 // 标记相机已绑定
				m_isCameraAttached = true;
				std::cout << "相机已绑定到第一个PlatParamPak平台（ID=" << platParam.id << "），偏移：(0, 0, -8)" << std::endl;
				std::cout << "相机视场角：" << m_sensorParam.coarseTrackResolution << "," << m_sensorParam.preciseTrackResolution <<std::endl;
			}
		}
	}
	else
	{
		// 删除PlatParamPak平台
		for (auto& pakPlat : m_pakPlatformList)
		{
			if (pakPlat.isExist)
			{
				pakPlat.nodePath.remove_node();
				pakPlat.isExist = false;
				std::cout << "PlatParamPak平台已删除：ID=" << pakPlat.platID << std::endl;
			}
		}
		// ========== 新增：解绑相机，恢复默认视角 ==========
		if (m_isCameraAttached) {
			// 将相机重新父节点设为渲染根节点
			m_cameraNode.reparent_to(m_renderRoot);
			// 重置相机位置和姿态（恢复初始视角，可选）
			m_cameraNode.set_pos(0, 0, 0);
			m_cameraNode.set_hpr(0, 0, 0);
			// 标记相机未绑定
			m_isCameraAttached = false;
			std::cout << "相机已解绑，恢复默认视角" << std::endl;
		}
		m_pakPlatformList.clear();
	}
}

// 处理WeaponState平台增删（武器状态）
void HwaSimIR::ProcessAddRemoveWeaponPlatform()
{
#if 0

	if (m_isAddPlatform)
	{
		// 增加WeaponState平台
		const BYHWICD::WeaponState& weaponState = m_initSceneData.weaponState;
		if (weaponState.targetType == 0x00)
		{
			std::cout << "WeaponState无有效目标，跳过生成" << std::endl;
			return;
		}

		PLATFORM_TYPE platType = TargetTypeToPlatformType(weaponState.targetType);
		if (platType == NONE)
		{
			std::cerr << "无效的WeaponState目标类型：0x" << std::hex << weaponState.targetType << std::endl;
			return;
		}

		auto resIter = m_platformResMap.find(platType);
		if (resIter == m_platformResMap.end()) return;


		// 加载模型和纹理
		NodePath modelNode = m_pMainWindow->load_model(m_renderRoot, resIter->second.modelPath);
		if (modelNode.is_empty()) return;

		PT(Texture) texture = TexturePool::load_texture(resIter->second.texturePath);
		if (texture != nullptr) modelNode.set_texture(texture);

		// 初始化WeaponPlatformData
		WeaponPlatformData newWeaponPlat;
		newWeaponPlat.type = platType;
		newWeaponPlat.platID = weaponState.targetID;
		newWeaponPlat.weaponState = weaponState; // 拷贝协议参数
		newWeaponPlat.isExist = true;
		newWeaponPlat.nodePath = modelNode;

		// 添加到列表并显示
		m_weaponPlatformList.push_back(newWeaponPlat);
		modelNode.show();
		std::cout << "WeaponState平台生成成功：类型=" << platType << " 目标ID=" << weaponState.targetID << std::endl;
	}
	else
	{
		// 删除WeaponState平台
		for (auto& weaponPlat : m_weaponPlatformList)
		{
			if (weaponPlat.isExist)
			{
				weaponPlat.nodePath.remove_node();
				weaponPlat.isExist = false;
			}
		}
		m_weaponPlatformList.clear();
	}

#endif // 0
}

// 处理TargetState平台增删（目标状态）
void HwaSimIR::ProcessAddRemoveTargetPlatform()
{

	if (m_isAddPlatform)
	{
		// 增加TargetState平台
		std::cout << "开始生成TargetState平台，有效数--120：" << m_initSceneData.MissileMaxCount120 <<"--9："<< m_initSceneData.MissileMaxCount9 << "--MMD：" << m_initSceneData.MissileMaxCountMMD << std::endl;
		//int 

		for (int i = 0; i < m_initSceneData.MissileMaxCount120; ++i)
		{
			//if (i >= 5) break; // 协议中targetState最多5个

			//const BYHWICD::TargetState& targetState = m_initSceneData.targetState[i];
			PLATFORM_TYPE platType = TargetTypeToPlatformType(0x22);
			if (platType == NONE)
			{
				std::cerr << "无效的TargetState目标类型：0x" << std::hex << 0x22 << std::endl;
				continue;
			}

			auto resIter = m_platformResMap.find(platType);
			if (resIter == m_platformResMap.end()) continue;

			// 加载模型和纹理
			NodePath modelNode = m_pMainWindow->load_model(m_renderRoot, resIter->second.modelPath);
			if (modelNode.is_empty()) continue;

			PT(Texture) texture = TexturePool::load_texture(resIter->second.texturePath);
			if (texture != nullptr) modelNode.set_texture(texture);

			// 初始化TargetPlatformData
			TargetPlatformData newTargetPlat;
			newTargetPlat.type = platType;
			newTargetPlat.platID = i;
			newTargetPlat.targetState.targetType = 0x22;
			newTargetPlat.targetState.targetPlatID = 0;
			newTargetPlat.targetState.targetID = 0;
			newTargetPlat.targetState.engineState = false;
			newTargetPlat.targetState.viewValid = false;
			newTargetPlat.targetState.targetLoc.lat = 0.0;
			newTargetPlat.targetState.targetLoc.lon = 0.0;
			newTargetPlat.targetState.targetLoc.alt = 0.0;
			newTargetPlat.targetState.targetLoc.yaw = 0.0;
			newTargetPlat.targetState.targetLoc.pitch = 0.0;
			newTargetPlat.targetState.targetLoc.roll = 0.0;
			newTargetPlat.targetState.targetState = 0x01;
			newTargetPlat.isExist = true;
			newTargetPlat.nodePath = modelNode;

			// 设置初始位置（从目标空间状态读取）
			const BYHWICD::SpatialState& spatial = newTargetPlat.targetState.targetLoc;
			modelNode.set_pos(spatial.lat, spatial.lon, spatial.alt);
			modelNode.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);

			// 添加到列表并显示
			m_targetPlatformList.push_back(newTargetPlat);
			modelNode.show();

			// 【修改点 4】：给导弹挂载红外着色器
			ApplyInfraredShader(modelNode, false);

			std::cout << "TargetState平台生成成功：类型=" << platType << " 目标ID=" << newTargetPlat.platID << std::endl;
			/*if (newTargetPlat.platID == 3)
			{
				m_cameraNode.look_at(modelNode);
			}*/
		}
		for (int i = 0; i < m_initSceneData.MissileMaxCount9; ++i)
		{
			//if (i >= 5) break; // 协议中targetState最多5个

			//const BYHWICD::TargetState& targetState = m_initSceneData.targetState[i];
			PLATFORM_TYPE platType = TargetTypeToPlatformType(0x33);
			if (platType == NONE)
			{
				std::cerr << "无效的TargetState目标类型：0x" << std::hex << 0x33 << std::endl;
				continue;
			}

			auto resIter = m_platformResMap.find(platType);
			if (resIter == m_platformResMap.end()) continue;

			// 加载模型和纹理
			NodePath modelNode = m_pMainWindow->load_model(m_renderRoot, resIter->second.modelPath);
			if (modelNode.is_empty()) continue;

			PT(Texture) texture = TexturePool::load_texture(resIter->second.texturePath);
			if (texture != nullptr) modelNode.set_texture(texture);

			// 初始化TargetPlatformData
			TargetPlatformData newTargetPlat;
			newTargetPlat.type = platType;
			newTargetPlat.platID = i;
			newTargetPlat.targetState.targetType = 0x33;
			newTargetPlat.targetState.targetPlatID = 0;
			newTargetPlat.targetState.targetID = 0;
			newTargetPlat.targetState.engineState = false;
			newTargetPlat.targetState.viewValid = false;
			newTargetPlat.targetState.targetLoc.lat = 0.0;
			newTargetPlat.targetState.targetLoc.lon = 0.0;
			newTargetPlat.targetState.targetLoc.alt = 0.0;
			newTargetPlat.targetState.targetLoc.yaw = 0.0;
			newTargetPlat.targetState.targetLoc.pitch = 0.0;
			newTargetPlat.targetState.targetLoc.roll = 0.0;
			newTargetPlat.targetState.targetState = 0x01;
			newTargetPlat.isExist = true;
			newTargetPlat.nodePath = modelNode;

			// 设置初始位置（从目标空间状态读取）
			const BYHWICD::SpatialState& spatial = newTargetPlat.targetState.targetLoc;
			modelNode.set_pos(spatial.lat, spatial.lon, spatial.alt);
			modelNode.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);

			// 添加到列表并显示
			m_targetPlatformList.push_back(newTargetPlat);
			modelNode.show();

			// 【修改点 4】：给导弹挂载红外着色器
			ApplyInfraredShader(modelNode, false);

			std::cout << "TargetState平台生成成功：类型=" << platType << " 目标ID=" << newTargetPlat.platID << std::endl;
		}
		for (int i = 0; i < m_initSceneData.MissileMaxCountMMD; ++i)
		{
			//if (i >= 5) break; // 协议中targetState最多5个

			//const BYHWICD::TargetState& targetState = m_initSceneData.targetState[i];
			PLATFORM_TYPE platType = TargetTypeToPlatformType(0x44);
			if (platType == NONE)
			{
				std::cerr << "无效的TargetState目标类型：0x" << std::hex << 0x44 << std::endl;
				continue;
			}

			auto resIter = m_platformResMap.find(platType);
			if (resIter == m_platformResMap.end()) continue;

			// 加载模型和纹理
			NodePath modelNode = m_pMainWindow->load_model(m_renderRoot, resIter->second.modelPath);
			if (modelNode.is_empty()) continue;

			PT(Texture) texture = TexturePool::load_texture(resIter->second.texturePath);
			if (texture != nullptr) modelNode.set_texture(texture);

			// 初始化TargetPlatformData
			TargetPlatformData newTargetPlat;
			newTargetPlat.type = platType;
			newTargetPlat.platID = i;
			newTargetPlat.targetState.targetType = 0x44;
			newTargetPlat.targetState.targetPlatID = 0;
			newTargetPlat.targetState.targetID = 0;
			newTargetPlat.targetState.engineState = false;
			newTargetPlat.targetState.viewValid = false;
			newTargetPlat.targetState.targetLoc.lat = 0.0;
			newTargetPlat.targetState.targetLoc.lon = 0.0;
			newTargetPlat.targetState.targetLoc.alt = 0.0;
			newTargetPlat.targetState.targetLoc.yaw = 0.0;
			newTargetPlat.targetState.targetLoc.pitch = 0.0;
			newTargetPlat.targetState.targetLoc.roll = 0.0;
			newTargetPlat.targetState.targetState = 0x01;
			newTargetPlat.isExist = true;
			newTargetPlat.nodePath = modelNode;

			// 设置初始位置（从目标空间状态读取）
			const BYHWICD::SpatialState& spatial = newTargetPlat.targetState.targetLoc;
			modelNode.set_pos(spatial.lat, spatial.lon, spatial.alt);
			modelNode.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);

			// 添加到列表并显示
			m_targetPlatformList.push_back(newTargetPlat);
			modelNode.show();

			// 【修改点 4】：给导弹挂载红外着色器
			ApplyInfraredShader(modelNode, false);

			std::cout << "TargetState平台生成成功：类型=" << platType << " 目标ID=" << newTargetPlat.platID << std::endl;
		}
	}
	else
	{
		// 删除TargetState平台
		for (auto& targetPlat : m_targetPlatformList)
		{
			if (targetPlat.isExist)
			{
				targetPlat.nodePath.remove_node();
				targetPlat.isExist = false;
			}
		}
		m_targetPlatformList.clear();
	}
}


void HwaSimIR::on_key_event(const Event * event, void * user_data)
{
	HwaSimIR* self = static_cast<HwaSimIR*>(user_data);
	
	LVecBase3 hpr = self->m_cameraNode.get_hpr();
	LVecBase3 pos = self->m_cameraNode.get_pos();
	double insu = 3.0;

	const std::string& name = event->get_name();


	if (name == "w") {
		std::cout << "[INPUT] w" << std::endl;
		hpr[1] = hpr[1] + insu;
		self->m_cameraNode.set_hpr(hpr);
		std::cout << "Camera HPR: H=" << hpr[0]
			<< ", P=" << hpr[1]
			<< ", R=" << hpr[2] << std::endl;
		
	}
	else if (name == "s") {
		std::cout << "[INPUT] s" << std::endl;
		hpr[1] = hpr[1] - insu;
		self->m_cameraNode.set_hpr(hpr);
		std::cout << "Camera HPR: H=" << hpr[0]
			<< ", P=" << hpr[1]
			<< ", R=" << hpr[2] << std::endl;
		
	}
	else if (name == "a") {
		std::cout << "[INPUT] a" << std::endl;
		hpr[0] = hpr[0] - insu;
		self->m_cameraNode.set_hpr(hpr);
		std::cout << "Camera HPR: H=" << hpr[0]
			<< ", P=" << hpr[1]
			<< ", R=" << hpr[2] << std::endl;

	}
	else if (name == "d") {
		std::cout << "[INPUT] d" << std::endl;
		hpr[0] = hpr[0] + insu;
		self->m_cameraNode.set_hpr(hpr);
		std::cout << "Camera HPR: H=" << hpr[0]
			<< ", P=" << hpr[1]
			<< ", R=" << hpr[2] << std::endl;

	}
	else if (name == "arrow_up") {
		std::cout << "[INPUT] arrow_up" << std::endl;
		pos[1] = pos[1] + insu;
		self->m_cameraNode.set_pos(pos);
		std::cout << "Camera XYZ: X=" << pos[0]
			<< ", Y=" << pos[1]
			<< ", Z=" << pos[2] << std::endl;

	}
	else if (name == "arrow_down") {
		std::cout << "[INPUT] arrow_down" << std::endl;
		pos[1] = pos[1] - insu;
		self->m_cameraNode.set_pos(pos);
		std::cout << "Camera XYZ: X=" << pos[0]
			<< ", Y=" << pos[1]
			<< ", Z=" << pos[2] << std::endl;
	}
	else if (name == "arrow_left") {
		std::cout << "[INPUT] arrow_left" << std::endl;
		pos[0] = pos[0] - insu;
		self->m_cameraNode.set_pos(pos);
		std::cout << "Camera XYZ: X=" << pos[0]
			<< ", Y=" << pos[1]
			<< ", Z=" << pos[2] << std::endl;

	}
	else if (name == "arrow_right") {
		std::cout << "[INPUT] arrow_right" << std::endl;
		pos[0] = pos[0] + insu;
		self->m_cameraNode.set_pos(pos);
		std::cout << "Camera XYZ: X=" << pos[0]
			<< ", Y=" << pos[1]
			<< ", Z=" << pos[2] << std::endl;

	}
	else
	{
		std::cout << "[INPUT] null" << std::endl;
	}
}

// 初始化UDP线程
bool HwaSimIR::InitUdpThread() {
	// 配置UDP参数（可根据实际需求修改IP和端口）
	std::string localIp = "0.0.0.0";       // 绑定所有本地IP
	uint16_t localPort = 8888;            // 本地UDP端口
	std::string remoteIp = "127.0.0.1";   // 激励数据软件IP
	//std::string remoteIp = "192.168.137.247";   // 激励数据软件IP
	//std::string remoteIp = "192.168.0.10";   // 激励数据软件IP
	uint16_t remotePort = 9999;           // 激励数据软件UDP端口
	std::cout << "[Stage0] UDP baseline local=" << localIp << ":" << localPort
		<< " remote=" << remoteIp << ":" << remotePort << std::endl;

	m_pUdpThread = new UdpCommThread(this, localIp, localPort, remoteIp, remotePort);
	if (!m_pUdpThread->start()) {
		std::cerr << "UDP线程启动失败！" << std::endl;
		delete m_pUdpThread;
		m_pUdpThread = nullptr;
		return false;
	}

	std::cout << "UDP线程初始化成功" << std::endl;
	return true;
}

bool HwaSimIR::InitTcpThread()
{
	// 配置TCP参数
	std::string serverIp = "127.0.0.1"; // 服务器IP地址
	uint16_t serverPort = 5555; // 服务器端口
	std::cout << "[Stage0] TCP video baseline server=" << serverIp << ":" << serverPort
		<< " format=length-prefixed JPEG" << std::endl;

	m_pTcpThread = new TcpCommThread(this, serverIp, serverPort);
	if (!m_pTcpThread->start()) {
		std::cerr << "TCP线程启动失败！" << std::endl;
		delete m_pTcpThread;
		m_pTcpThread = nullptr;
		return false;
	}
	std::cout << "TCP线程初始化成功" << std::endl;
	return true;
}

// 处理控制指令（复位/开始/停止）
void HwaSimIR::handleControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd) {
	std::lock_guard<std::mutex> lock(m_mtx);
	// 更新当前回合数
	m_currentRound = cmd.currentRound;

	std::cout << "收到控制指令：" << std::endl;
	std::cout << "  军别：" << cmd.JB << std::endl;
	std::cout << "  挂载平台ID：" << cmd.platID << std::endl;
	std::cout << "  仿真指令：" << (cmd.simCommand == 1 ? "复位" : (cmd.simCommand == 2 ? "开始" : "停止")) << std::endl;
	std::cout << "  总回合数：" << cmd.roundCut << std::endl;
	std::cout << "  当前回合：" << cmd.currentRound << std::endl;
	std::cout << "[Stage0] Control command received: command=" << cmd.simCommand
		<< ", round=" << cmd.currentRound << "/" << cmd.roundCut << std::endl;

	// ========== 业务逻辑（后续填充） ==========
	switch (cmd.simCommand) {
	case 1: // 复位
		std::cout << "执行复位逻辑..." << std::endl;
		m_stage0DisplayFrameCount = 0;
		// TODO: 实现复位逻辑（清空状态、重置传感器等）

		// 设置增删标记为"删除"
		m_isAddPlatform = false;
		// 设置TargetState平台初始化ID映射标记
		m_isInitTargetPlatID = false;
		// 删除所有三类平台
		ProcessAddRemovePakPlatform();
		ProcessAddRemoveWeaponPlatform();
		ProcessAddRemoveTargetPlatform();

		////测试坐标转换
		//m_geoTrans.InitReferencePoint(12.77632, 45.04385, 6.0);
		//double px, py, pz;
		//BYHWICD::SpatialState spatial;
		//spatial.lat = 12.77627;
		//spatial.lon = 45.04369;
		//spatial.alt = 6.0;
		//spatial.yaw = 239.0;
		//spatial.pitch = 0.0;
		//spatial.roll = 0.0;
		//m_geoTrans.Wgs84ToPandaXYZ(spatial, px, py, pz);
		//std::cout << "px=   " << px << "-----py=   " << py << "-----pz=   " << pz << std::endl;

		// 清空协议数据缓存
		memset(&m_initSceneData, 0, sizeof(BYHWICD::InitP2cObjectTrackingCmd));
		memset(&m_realTimeSceneData, 0, sizeof(BYHWICD::DisplayC2cObjTrackingData));
		memset(&m_sensorParam, 0, sizeof(BYHWICD::trackerSensorParam));
		m_lastLoggedSensorProtocolBand = -999;

		// 重置仿真状态
		m_isSimRunning = false;
		std::cout << "复位完成：所有平台已删除，数据已清空" << std::endl;
		break;
	case 2: // 开始
		std::cout << "执行开始仿真逻辑..." << std::endl;
		// TODO: 实现开始仿真逻辑（启动渲染、数据采集等）
		m_isSimRunning = true;
		
		std::cout << "仿真开始：当前回合=" << m_currentRound << std::endl;
		break;
	case 3: // 停止
		std::cout << "执行停止仿真逻辑..." << std::endl;
		// TODO: 实现停止仿真逻辑（停止渲染、保存数据等）
		m_isSimRunning = false;
	
		std::cout << "仿真停止：当前回合=" << m_currentRound << std::endl;
		break;
	default:
		std::cerr << "未知的仿真指令：" << cmd.simCommand << std::endl;
		break;
	}
}

// 处理初始化指令并发送应答
void HwaSimIR::handleInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& cmd) {
	std::lock_guard<std::mutex> lock(m_mtx);

	std::cout << "收到成像初始化指令：" << std::endl;
	std::cout << "  军别：" << cmd.JB << std::endl;
	std::cout << "  挂载平台ID：" << cmd.platID << std::endl;
	std::cout << "  传感器ID：" << cmd.sensorID << std::endl;
	std::cout << "  有效平台数：" << cmd.platNumValid << std::endl;
	const BYHWICD::trackerSensorParam& sensor = cmd.trackingInit.trackerSensor[0];
	std::cout << "[Stage0] Init baseline: sensorBand=" << sensor.trackerSensorBand
		<< ", sensorSize=" << sensor.trackerSensorWidth << "x" << sensor.trackerSensorHeight
		<< ", fov=" << sensor.coarseTrackResolution << "x" << sensor.preciseTrackResolution
		<< ", videoFps=" << cmd.trackingInit.videoFps
		<< ", missileMax(AIM120/AIM9/MMD)=" << cmd.MissileMaxCount120 << "/"
		<< cmd.MissileMaxCount9 << "/" << cmd.MissileMaxCountMMD << std::endl;
	LogActiveIRSensorProfile(sensor.trackerSensorBand, "init-command", true);

	// ========== 初始化业务逻辑（后续填充） ==========
	std::cout << "执行成像初始化逻辑..." << std::endl;
	// TODO: 解析传感器参数、初始化场景、加载模型等
	//缓存初始化数据
	m_initSceneData = cmd;
	m_currentRound = 0; // 重置回合数
	m_stage0DisplayFrameCount = 0;

	//处理成像初始化数据，生成平台
	ProcessRealSimSceneInitData();




	// 构造初始化应答
	BYHWICD::InitAckC2pObjectTrackingCmd ack;
	ack.flag = 0x37;
	ack.JB = cmd.JB;
	ack.platID = cmd.platID;
	ack.sensorID = cmd.sensorID;
	ack.trackingReady = true; // 标记为已准备好（根据实际初始化结果修改）

							  // 发送应答
	if (m_pUdpThread) {
		m_pUdpThread->sendInitAck(ack);
	}
	std::cout << "发送初始化应答：准备状态=" << (ack.trackingReady ? "就绪" : "未就绪") << std::endl;
}

// 处理实时成像数据包
void HwaSimIR::handleDisplayData(const BYHWICD::DisplayC2cObjTrackingData& data) {
	std::lock_guard<std::mutex> lock(m_mtx);
	++m_stage0DisplayFrameCount;
	if (m_stage0DisplayFrameCount == 1 || (m_stage0DisplayFrameCount % 100) == 0)
	{
		std::cout << "[Stage0] Display packet #" << m_stage0DisplayFrameCount
			<< ": platID=" << data.platID
			<< ", sensorID=" << data.sensorID
			<< ", targetNumValid=" << data.targetNumValid
			<< ", timeMs=" << data.time << std::endl;
	}

	/*std::cout << "收到实时成像数据：" << std::endl;
	std::cout << "  挂载平台ID：" << data.platID << std::endl;
	std::cout << "  传感器ID：" << data.sensorID << std::endl;
	std::cout << "  当前时间：" << data.time << "ms" << std::endl;
	std::cout << "  有效目标数：" << data.targetNumValid << std::endl;*/
#if 0
	std::cout << "==================== 收到实时成像数据 ====================" << std::endl;
	std::cout << "  数据标志位：0x" << std::hex << data.flag << std::dec << std::endl; // 十六进制打印标志位
	std::cout << "  挂载平台ID：" << data.platID << std::endl;
	std::cout << "  传感器ID：" << data.sensorID << std::endl;
	std::cout << "  当前时间：" << data.time << "ms" << std::endl;
	std::cout << "  有效目标数：" << data.targetNumValid << std::endl;

	// 打印传感器挂载平台姿态信息
	std::cout << "  挂载平台姿态信息：" << std::endl;
	std::cout << "    位置/姿态：x=" << data.platLoc.lon << ", y=" << data.platLoc.lat << ", z=" << data.platLoc.alt
		<< " | 俯仰=" << data.platLoc.pitch << "°, 偏航=" << data.platLoc.yaw << "°, 滚转=" << data.platLoc.roll << "°" << std::endl;

	// 打印实时武器状态信息
	std::cout << "  实时武器状态（Wg）：" << std::endl;
	std::cout << "    目标类型：0x" << std::hex << data.weaponState.targetType << std::dec
		<< " (" << (data.weaponState.targetType == 0x00 ? "无" :
			data.weaponState.targetType == 0x11 ? "飞机" :
			data.weaponState.targetType == 0x22 ? "雷达导弹" :
			data.weaponState.targetType == 0x33 ? "红外弹" : "MMD") << ")" << std::endl;
	std::cout << "    被打击目标平台ID：" << data.weaponState.targetPlatID << std::endl;
	std::cout << "    被打击目标ID：" << data.weaponState.targetID << std::endl;
	std::cout << "    目标与机轴角度：方位角=" << data.weaponState.xxOutAng[0] << "°, 俯仰角=" << data.weaponState.xxOutAng[1] << "°" << std::endl;
	std::cout << "    自动对准使能：" << (data.weaponState.lookatEn ? "是" : "否") << std::endl;
	std::cout << "    照明器开启状态：" << (data.weaponState.illuminatorEn ? "开启" : "关闭") << std::endl;
	std::cout << "    角度偏移量：Pitch=" << data.weaponState.offsetAng[0] << "°, Yaw=" << data.weaponState.offsetAng[1] << "°" << std::endl;
	std::cout << "    目标是否在视场：" << (data.weaponState.viewValid ? "是" : "否") << std::endl;
	std::cout << "    目标毁伤标志：" << (data.weaponState.damageFlag == 0 ? "未毁伤" : "已毁伤") << std::endl;
	std::cout << "    武器打击标志：" << (data.weaponState.strikeFlag ? "已打击" : "未打击") << std::endl;
	std::cout << "    目标打击部位：" << (data.weaponState.strikePart == 1 ? "头部" : (data.weaponState.strikePart == 2 ? "舱体" : "未知")) << std::endl;

	// 打印多目标状态信息（遍历所有有效目标）
	std::cout << "  有效目标状态列表（共" << data.targetNumValid << "个）：" << std::endl;
	for (int i = 0; i < data.targetNumValid; ++i) {
		const auto& target = data.targetState[i];
		std::cout << "    【目标" << (i + 1) << "】" << std::endl;
		std::cout << "      目标类型：0x" << std::hex << target.targetType << std::dec
			<< " (" << (target.targetType == 0x11 ? "飞机" :
				target.targetType == 0x22 ? "雷达导弹" :
				target.targetType == 0x33 ? "红外弹" : "MMD") << ")" << std::endl;
		std::cout << "      目标挂载平台ID：" << target.targetPlatID << std::endl;
		std::cout << "      目标ID：" << target.targetID << std::endl;
		std::cout << "      发动机状态：" << (target.engineState ? "开机" : "关机") << std::endl;
		std::cout << "      目标是否在视场：" << (target.viewValid ? "是" : "否") << std::endl;
		std::cout << "      目标位置/姿态：x=" << target.targetLoc.lon << ", y=" << target.targetLoc.lat << ", z=" << target.targetLoc.alt
			<< " | 俯仰=" << target.targetLoc.pitch << "°, 偏航=" << target.targetLoc.yaw << "°, 滚转=" << target.targetLoc.roll << "°" << std::endl;
		std::cout << "      目标状态：0x" << std::hex << target.targetState << std::dec
			<< " (" << (target.targetState == 0x01 ? "打击态" :
				target.targetState == 0x02 ? "爆炸态" : "击毁态") << ")" << std::endl;
	}

	// ========== 实时数据处理逻辑（后续填充） ==========
	std::cout << "============================================================" << std::endl;
#endif // 0
	// ========== 实时数据处理逻辑（后续填充） ==========
	std::cout << "处理实时成像数据..." << std::endl;
	// TODO: 更新传感器姿态、目标位置、渲染红外图像等

	// 缓存实时数据
	m_realTimeSceneData = data;

	// 标记有新数据需要主线程去处理
	m_bHasNewData = true;
	//// 更新平台位置和姿态
	//ProcessRealSimSceneDrivenData();

	// 通知主线程，有新数据到达
	m_cvNewData.notify_one();
}

void HwaSimIR::InitInfraredSimulation()
{
	std::string materialPath = FirstExistingPath({
		"materials/MaterialDatabase.csv",
		"../materials/MaterialDatabase.csv",
		"../../materials/MaterialDatabase.csv"
	});
	std::string transmittancePath = FirstExistingPath({
		"transmittance/transmittance_0.3_15.txt",
		"../transmittance/transmittance_0.3_15.txt",
		"../../transmittance/transmittance_0.3_15.txt"
	});
	std::vector<std::string> sensorWaveDirs;
	sensorWaveDirs.push_back("Config/SensorWave");
	sensorWaveDirs.push_back("../Bin/Config/SensorWave");
	sensorWaveDirs.push_back("ConsoleApplication1_LLA/Bin/Config/SensorWave");
	sensorWaveDirs.push_back("../ConsoleApplication1_LLA/Bin/Config/SensorWave");

	m_irMaterialReady = m_irMaterialDatabase.load(materialPath);
	m_irAtmosphereReady = m_irAtmosphereModel.loadTransmissionTable(transmittancePath);
	m_irSensorProfilesReady = m_irSensorProfiles.loadFromDirectoryCandidates(sensorWaveDirs);
	m_irRadianceModel.setMaterialDatabase(&m_irMaterialDatabase);
	m_irRadianceModel.setAtmosphereModel(&m_irAtmosphereModel);

	IRRuntimeEnvironment environment;
	environment.band = IRBandFromProtocol(2);
	environment.airTemperatureC = 25.0;
	environment.visibilityMeters = 23000.0;
	environment.sunElevationDeg = 45.0;
	environment.sunStrength = 1.0;
	m_irRadianceModel.setEnvironment(environment);

	std::cout << "红外全链路CPU模型初始化：材质库="
		<< (m_irMaterialReady ? "OK" : "未加载，使用默认材质")
		<< " 路径=" << materialPath
		<< "，MODTRAN透过率="
		<< (m_irAtmosphereReady ? "OK" : "未加载，使用经验透过率")
		<< " 路径=" << transmittancePath << std::endl;
	std::cout << "[Stage1] IR配置输入：SensorWave="
		<< (m_irSensorProfilesReady ? "OK" : "未加载，使用内置传感器默认值")
		<< " 路径=" << (m_irSensorProfilesReady ? m_irSensorProfiles.loadedDirectory() : "fallback")
		<< "，MaterialDatabase=" << (m_irMaterialReady ? materialPath : "fallback")
		<< "，Transmittance=" << (m_irAtmosphereReady ? transmittancePath : "fallback") << std::endl;
	LogActiveIRSensorProfile(2, "startup-default", true);
}

void HwaSimIR::InitSkyAndCloudScene()
{
	if (!m_pMainWindow || m_cameraNode.is_empty())
	{
		return;
	}

	CardMaker skyMaker("IR_Sky_Background");
	skyMaker.set_frame(-1.0f, 1.0f, -1.0f, 1.0f);
	m_skyNode = m_cameraNode.attach_new_node(skyMaker.generate());
	m_skyNode.set_pos(0.0f, 2500.0f, 0.0f);
	m_skyNode.set_scale(2600.0f, 1.0f, 1600.0f);
	m_skyNode.set_depth_write(false);
	m_skyNode.set_depth_test(false);
	m_skyNode.set_bin("background", 0);
	ApplyInfraredShader(m_skyNode, true);

	m_cloudNodes.clear();
	for (int i = 0; i < 18; ++i)
	{
		CardMaker cloudMaker("IR_Cloud_Particle");
		cloudMaker.set_frame(-1.0f, 1.0f, -0.45f, 0.45f);
		NodePath cloud = m_cameraNode.attach_new_node(cloudMaker.generate());
		float x = -900.0f + static_cast<float>((i * 137) % 1800);
		float z = 180.0f + static_cast<float>((i * 79) % 620);
		float y = 1200.0f + static_cast<float>((i % 5) * 120);
		float scale = 160.0f + static_cast<float>((i % 4) * 45);
		cloud.set_pos(x, y, z);
		cloud.set_scale(scale * 1.9f, 1.0f, scale);
		cloud.set_transparency(TransparencyAttrib::M_alpha);
		cloud.set_depth_write(false);
		cloud.set_bin("transparent", 10);
		ApplyInfraredShader(cloud, false);
		m_cloudNodes.push_back(cloud);
	}

	std::cout << "天空背景与粒子云初始化完成：cloud cards=" << m_cloudNodes.size() << std::endl;
}

// 初始化红外仿真着色器
void HwaSimIR::InitInfraredShader() {
	// 顶点着色器：计算世界坐标并传递局部坐标给片段着色器计算亮斑距离
	std::string vertex_shader = R"(
    #version 100
    uniform mat4 p3d_ModelViewProjectionMatrix;
    attribute vec4 p3d_Vertex;
    attribute vec2 p3d_MultiTexCoord0;
    
    varying vec2 texcoord;
    varying vec3 v_local_pos; // 传递模型局部坐标系下的三维坐标

    void main() {
        gl_Position = p3d_ModelViewProjectionMatrix * p3d_Vertex;
        texcoord = p3d_MultiTexCoord0;
        v_local_pos = p3d_Vertex.xyz; // 提取局部坐标
    }
    )";

	// 片段着色器：实现白热模式、中/长波切换、一头一尾独立热源亮斑
	std::string fragment_shader = R"(
    #version 100
    precision mediump float;

    uniform sampler2D p3d_Texture0;
    
    uniform int u_is_background;
    uniform int u_object_kind;     // 0:目标 1:天空 2:粒子云
    uniform int u_wave_band;       // 0: 短波 (SWIR), 1: 中波 (MWIR)
    uniform float u_base_temperature;
    uniform float u_time;
    uniform float u_ir_radiance;
    uniform float u_emissivity;
    uniform float u_reflectance;
    uniform float u_tau_up;
    uniform float u_path_radiance;
    uniform float u_sky_radiance;
    uniform float u_display_gain;
    uniform float u_display_offset;
    uniform float u_cloud_density;

    // 头部热源参数
    uniform int u_hotspot_front_en;
    uniform vec3 u_hotspot_front_pos;
    uniform float u_hotspot_front_radius;
    uniform float u_hotspot_front_temp;

    // 尾部热源参数
    uniform int u_hotspot_rear_en;
    uniform vec3 u_hotspot_rear_pos;
    uniform float u_hotspot_rear_radius;
    uniform float u_hotspot_rear_temp;

	// ================= 表面亮斑参数 =================
    uniform int u_brightspot_en;       // 亮斑开关：0-关，1-开
    uniform vec3 u_brightspot_pos;     // 亮斑中心位置(局部坐标)
    uniform float u_brightspot_radius; // 亮斑基础半径
    uniform float u_brightspot_temp;   // 亮斑亮度/温度增量
    // ======================================================

    varying vec2 texcoord;
    varying vec3 v_local_pos;

    void main() {
        vec4 texColor = texture2D(p3d_Texture0, texcoord);

        if (u_object_kind == 1 || u_is_background == 1) {
            float sky_tint = (u_wave_band <= 2) ? 0.18 : 0.055;
            float bg_intensity = clamp(u_ir_radiance * u_display_gain + sky_tint, 0.0, 1.0);
            gl_FragColor = vec4(bg_intensity, bg_intensity, bg_intensity, 1.0);
            return;
        }

        if (u_object_kind == 2) {
            float edge = 1.0 - smoothstep(0.45, 1.0, length(v_local_pos.xz));
            float cloud_noise = 0.65 + 0.20 * sin(v_local_pos.x * 8.0 + u_time * 0.2)
                                      + 0.15 * cos(v_local_pos.y * 11.0);
            float cloud_intensity = clamp((u_ir_radiance + u_sky_radiance + 0.12) * cloud_noise, 0.0, 1.0);
            gl_FragColor = vec4(cloud_intensity, cloud_intensity, cloud_intensity, edge * clamp(u_cloud_density, 0.0, 0.85));
            return;
        }
		// 计算基础热辐射与范围热源
        float current_temp = u_base_temperature;

        if (u_hotspot_front_en == 1) {
            float dist_front = distance(v_local_pos, u_hotspot_front_pos);
            float factor_front = 1.0 - smoothstep(0.0, u_hotspot_front_radius, dist_front);
            current_temp += factor_front * u_hotspot_front_temp;
        }

        if (u_hotspot_rear_en == 1) {
            float dist_rear = distance(v_local_pos, u_hotspot_rear_pos);
            float flicker = 0.85 + 0.15 * sin(u_time * 15.0);
            float factor_rear = 1.0 - smoothstep(0.0, u_hotspot_rear_radius, dist_rear);
            current_temp += factor_rear * (u_hotspot_rear_temp * flicker);
        }

        float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));

        // 短波/中波物理特性
        // 短波 (0)：高度依赖反射光（表现为纹理明暗细节），对常温热辐射不敏感，但能看到高温热源
        // 中波 (1)：对热辐射极度敏感，纹理细节会被热量掩盖
        float detail_weight = (u_wave_band == 0) ? 0.7 : 0.2; 
        float temp_weight   = (u_wave_band == 0) ? 0.3 : 1.0;

        float final_intensity = current_temp * temp_weight + luminance * detail_weight;
        float chain_intensity = u_ir_radiance * u_display_gain + u_path_radiance + u_display_offset;
        final_intensity = mix(chain_intensity, final_intensity, 0.28);

		// 计算表面不规则亮斑 (仅在局部生效)
        float brightspot_intensity = 0.0;
        if (u_brightspot_en == 1) {
            float dist_bs = distance(v_local_pos, u_brightspot_pos);
            
            // 使用三角函数对局部空间坐标进行高频采样，制造“不规则”的边缘效果
            float irregularity = (sin(v_local_pos.x * 15.0) + cos(v_local_pos.y * 15.0 + v_local_pos.z * 5.0)) * 0.15 * u_brightspot_radius;
            float current_radius = u_brightspot_radius + irregularity;
            
            // 边缘过渡：从 current_radius 的 40% 处开始向外羽化衰减
            float factor_bs = 1.0 - smoothstep(current_radius * 0.4, current_radius, dist_bs);
            brightspot_intensity = factor_bs * u_brightspot_temp;
        }

        // 合成（限制在纯白 1.0 以内）
        final_intensity = clamp(final_intensity + brightspot_intensity, 0.0, 1.0);
        //float final_intensity = clamp(current_temp * temp_weight + luminance * detail_weight, 0.0, 1.0);

        gl_FragColor = vec4(final_intensity, final_intensity, final_intensity, texColor.a);
    }
    )";

	m_irShader = Shader::make(Shader::SL_GLSL, vertex_shader, fragment_shader);
	if (!m_irShader) {
		std::cerr << "红外仿真着色器编译失败！" << std::endl;
	}
}

// 挂载红外着色器及默认参数
void HwaSimIR::ApplyInfraredShader(NodePath& node, bool isBackground) {
	if (!m_irShader || node.is_empty()) return;

	// 优先级设为 1，强制覆盖底层自带材质
	node.set_shader(m_irShader, 1);

	// 为所有声明过的 uniform 赋初值，防止 HwaSimIR 渲染器报 not present 错误！

	// 全局与环境参数
	node.set_shader_input("u_is_background", LVecBase2i(isBackground ? 1 : 0, 0));
	node.set_shader_input("u_object_kind", LVecBase2i(isBackground ? 1 : 0, 0));
	node.set_shader_input("u_wave_band", LVecBase2i(3, 0));       // 默认中波
	node.set_shader_input("u_time", LVecBase2f(0.0f, 0.0f));      // 初始化时间，解决报错
	node.set_shader_input("u_base_temperature", LVecBase2f(0.4f, 0.0f));
	node.set_shader_input("u_ir_radiance", LVecBase2f(0.25f, 0.0f));
	node.set_shader_input("u_emissivity", LVecBase2f(0.85f, 0.0f));
	node.set_shader_input("u_reflectance", LVecBase2f(0.15f, 0.0f));
	node.set_shader_input("u_tau_up", LVecBase2f(0.85f, 0.0f));
	node.set_shader_input("u_path_radiance", LVecBase2f(0.02f, 0.0f));
	node.set_shader_input("u_sky_radiance", LVecBase2f(0.02f, 0.0f));
	node.set_shader_input("u_display_gain", LVecBase2f(1.0f, 0.0f));
	node.set_shader_input("u_display_offset", LVecBase2f(0.02f, 0.0f));
	node.set_shader_input("u_cloud_density", LVecBase2f(0.35f, 0.0f));

	// 头部亮斑默认配置
	node.set_shader_input("u_hotspot_front_en", LVecBase2i(0, 0));
	node.set_shader_input("u_hotspot_front_pos", LVecBase3f(-0.3f, 0.5f, 0.0f));
	node.set_shader_input("u_hotspot_front_radius", LVecBase2f(0.5f, 0.0f));
	node.set_shader_input("u_hotspot_front_temp", LVecBase2f(1.0f, 0.0f));

	// 尾部亮斑默认配置
	node.set_shader_input("u_hotspot_rear_en", LVecBase2i(0, 0));
	node.set_shader_input("u_hotspot_rear_pos", LVecBase3f(0.0f, 0.0f, 0.0f));
	node.set_shader_input("u_hotspot_rear_radius", LVecBase2f(2.0f, 0.0f));
	node.set_shader_input("u_hotspot_rear_temp", LVecBase2f(1.2f, 0.0f));

	// 表面亮斑默认配置
	node.set_shader_input("u_brightspot_en", LVecBase2i(0, 0)); // 默认关闭
	// 初始位置
	node.set_shader_input("u_brightspot_pos", LVecBase3f(0.0f, 0.0f, 2.0f));
	node.set_shader_input("u_brightspot_radius", LVecBase2f(1.0f, 0.0f)); // 亮斑大小
	node.set_shader_input("u_brightspot_temp", LVecBase2f(10.0f, 0.0f));   // 增加的亮度值															
}

void HwaSimIR::ApplyRadianceInputs(NodePath& node, const IRObjectRadianceOutput& radiance, int objectKind)
{
	if (node.is_empty())
	{
		return;
	}

	node.set_shader_input("u_object_kind", LVecBase2i(objectKind, 0));
	node.set_shader_input("u_wave_band", LVecBase2i(static_cast<int>(radiance.bandIndex), 0));
	node.set_shader_input("u_ir_radiance", LVecBase2f(radiance.baseRadiance, 0.0f));
	node.set_shader_input("u_emissivity", LVecBase2f(radiance.emissivity, 0.0f));
	node.set_shader_input("u_reflectance", LVecBase2f(radiance.reflectance, 0.0f));
	node.set_shader_input("u_tau_up", LVecBase2f(radiance.tauUp, 0.0f));
	node.set_shader_input("u_path_radiance", LVecBase2f(radiance.pathRadiance, 0.0f));
	node.set_shader_input("u_sky_radiance", LVecBase2f(radiance.skyRadiance, 0.0f));
	node.set_shader_input("u_display_gain", LVecBase2f(radiance.displayGain, 0.0f));
	node.set_shader_input("u_display_offset", LVecBase2f(radiance.displayOffset, 0.0f));
	node.set_shader_input("u_base_temperature", LVecBase2f(radiance.baseRadiance, 0.0f));
}

IRObjectRadianceOutput HwaSimIR::EvaluateNodeRadiance(const std::string& materialName, const NodePath& node, bool engineOn, bool damaged, bool isSky, bool isCloud, double cloudDensity)
{
	IRObjectRadianceInput input;
	input.materialName = materialName;
	input.rangeMeters = isSky ? 5000.0 : static_cast<double>(EstimateRangeToCamera(node));
	input.engineOn = engineOn;
	input.damaged = damaged;
	input.isSky = isSky;
	input.isCloud = isCloud;
	input.cloudDensity = cloudDensity;
	return m_irRadianceModel.evaluate(input);
}

std::string HwaSimIR::MaterialNameForPlatform(PLATFORM_TYPE type) const
{
	switch (type)
	{
	case F35:
	case J20:
		return "BM_METAL-ALUMINIUM";
	case AIM9:
		return "IR_CERAMIC";
	case AIM120:
		return "BM_METAL-STEEL";
	case MMD:
		return "BM_METAL";
	default:
		return "BM_METAL-ALUMINIUM";
	}
}

float HwaSimIR::EstimateRangeToCamera(const NodePath& node) const
{
	if (node.is_empty() || m_cameraNode.is_empty())
	{
		return 500.0f;
	}

	LPoint3f nodePos = node.get_pos(m_renderRoot);
	LPoint3f cameraPos = m_cameraNode.get_pos(m_renderRoot);
	LVector3f delta = nodePos - cameraPos;
	float range = delta.length();
	return std::max(1.0f, range);
}

void HwaSimIR::LogActiveIRSensorProfile(int protocolBand, const char* reason, bool forceLog)
{
	if (!forceLog && protocolBand == m_lastLoggedSensorProtocolBand)
	{
		return;
	}
	const IRSensorProfile& profile = m_irSensorProfiles.profileForProtocolBand(protocolBand);
	IRBand band = IRBandFromProtocol(protocolBand);
	IRBandRange modelRange = IRDefaultRangeForBand(band);
	std::cout << "[Stage1] Sensor profile (" << reason << "): protocolBand=" << protocolBand
		<< ", band=" << IRBandName(band)
		<< ", sensorRange=" << profile.spectralLowUm << "-" << profile.spectralHighUm << "um"
		<< ", modelRange=" << modelRange.lowUm << "-" << modelRange.highUm << "um"
		<< ", profileSize=" << profile.width << "x" << profile.height
		<< ", ADC=" << profile.adcBits << "bit"
		<< ", display=" << profile.displayBits << "bit"
		<< ", NETD=" << profile.netdK << "K"
		<< ", FOV=" << profile.fovHDeg << "x" << profile.fovVDeg
		<< ", source=" << profile.sourcePath << std::endl;
	m_lastLoggedSensorProtocolBand = protocolBand;
}

// 动态更新红外状态
void HwaSimIR::UpdatePlatformIRStatus() {
	double current_time = ClockObject::get_global_clock()->get_frame_time();

	IRRuntimeEnvironment environment = m_irRadianceModel.environment();
	environment.band = IRBandFromProtocol(m_sensorParam.trackerSensorBand);
	LogActiveIRSensorProfile(m_sensorParam.trackerSensorBand, "runtime-band", false);
	environment.airTemperatureC = (m_initSceneData.trackingInit.envTemp > -80.0 && m_initSceneData.trackingInit.envTemp < 80.0)
		? m_initSceneData.trackingInit.envTemp : 25.0;
	environment.visibilityMeters = (m_initSceneData.trackingInit.envVisibility > 1.0)
		? m_initSceneData.trackingInit.envVisibility : 23000.0;
	environment.sunElevationDeg = 45.0;
	environment.sunStrength = (m_initSceneData.trackingInit.envSky == 0) ? 1.0 : 0.65;
	m_irRadianceModel.setEnvironment(environment);

	if (!m_skyNode.is_empty())
	{
		IRObjectRadianceOutput skyRadiance = EvaluateNodeRadiance("BM_AIR", m_skyNode, false, false, true, false, 0.0);
		ApplyRadianceInputs(m_skyNode, skyRadiance, 1);
		m_skyNode.set_shader_input("u_time", LVecBase2f((float)current_time, 0.0f));
	}

	for (size_t i = 0; i < m_cloudNodes.size(); ++i)
	{
		if (!m_cloudNodes[i].is_empty())
		{
			double density = 0.28 + 0.025 * static_cast<double>(i % 8);
			IRObjectRadianceOutput cloudRadiance = EvaluateNodeRadiance("BM_WATER", m_cloudNodes[i], false, false, false, true, density);
			ApplyRadianceInputs(m_cloudNodes[i], cloudRadiance, 2);
			m_cloudNodes[i].set_shader_input("u_cloud_density", LVecBase2f(static_cast<float>(density), 0.0f));
			m_cloudNodes[i].set_shader_input("u_time", LVecBase2f((float)current_time, 0.0f));
		}
	}

	// 更新飞机平台 (PlatParamPak)
	for (auto& pakPlat : m_pakPlatformList) {
		if (pakPlat.isExist) {
			// 将 double 强转为 float 后，装入 LVecBase2f
			pakPlat.nodePath.set_shader_input("u_time", LVecBase2f((float)current_time, 0.0f));
			IRObjectRadianceOutput radiance = EvaluateNodeRadiance(MaterialNameForPlatform(pakPlat.type), pakPlat.nodePath, true, false, false, false, 0.0);
			ApplyRadianceInputs(pakPlat.nodePath, radiance, 0);

			// 常开尾喷口亮斑
			pakPlat.nodePath.set_shader_input("u_hotspot_rear_en", LVecBase2i(1, 0));
		}
	}

	// 更新目标导弹平台 (TargetState)
	for (auto& targetPlat : m_targetPlatformList) {
		if (targetPlat.isExist) {
			targetPlat.nodePath.set_shader_input("u_time", LVecBase2f((float)current_time, 0.0f));
			bool damaged = (targetPlat.targetState.targetState == 0x02 || targetPlat.targetState.targetState == 0x03);
			IRObjectRadianceOutput radiance = EvaluateNodeRadiance(MaterialNameForPlatform(targetPlat.type), targetPlat.nodePath, targetPlat.targetState.engineState, damaged, false, false, 0.0);
			ApplyRadianceInputs(targetPlat.nodePath, radiance, 0);

			// 发动机开机状态映射为尾部红外亮斑
			int isEngineOn = targetPlat.targetState.engineState ? 1 : 0;
			targetPlat.nodePath.set_shader_input("u_hotspot_rear_en", LVecBase2i(isEngineOn, 0));

			// 开启亮斑
			targetPlat.nodePath.set_shader_input("u_brightspot_en", LVecBase2i(1, 0));
			// 设定亮斑位置（在模型的局部坐标系下）
			targetPlat.nodePath.set_shader_input("u_brightspot_pos", LVecBase3f(-0.3f, 0.5f, 0.0f));
			// 设定不规则圆形的半径
			targetPlat.nodePath.set_shader_input("u_brightspot_radius", LVecBase2f(0.2f, 0.0f));
			// 设定增量（1.0代表叠加纯白）
			targetPlat.nodePath.set_shader_input("u_brightspot_temp", LVecBase2f(1.0f, 0.0f));
		}
	}
}




void HwaSimIR::SetRenderMode(bool isSync, double targetFPS) {
	m_bSyncRenderMode = isSync;

	ClockObject* globalClock = ClockObject::get_global_clock();
	if (isSync) {
		// 同步模式：解除帧率限制，完全由 UDP 的到达频率驱动
		globalClock->set_mode(ClockObject::M_normal);
		std::cout << "切换至【同步渲染模式】，1组UDP数据渲染1帧" << std::endl;
	}
	else {
		// 异步模式：启用内部帧率限制
		if (targetFPS > 0.0) {
			globalClock->set_mode(ClockObject::M_limited);
			globalClock->set_frame_rate(targetFPS);
			std::cout << "切换至【异步渲染模式】，锁定帧率: " << targetFPS << " FPS" << std::endl;
		}
		else {
			globalClock->set_mode(ClockObject::M_normal);
			std::cout << "切换至【异步渲染模式】，不限帧（性能拉满）" << std::endl;
		}
	}
}







// HwaSimIR 的每帧更新任务回调
AsyncTask::DoneStatus HwaSimIR::shader_update_task(GenericAsyncTask* task, void* data) {
	HwaSimIR* self = static_cast<HwaSimIR*>(data);
	if (self->m_isSimRunning) {
		self->UpdatePlatformIRStatus();
	}
	return AsyncTask::DS_cont;
}

// ================= 新增：主线程执行的场景更新任务 =================
AsyncTask::DoneStatus HwaSimIR::scene_update_task(GenericAsyncTask* task, void* data) {
	HwaSimIR* self = static_cast<HwaSimIR*>(data);

	bool shouldUpdateScene = false;

	// 尽量缩短锁的占用时间，只用来读取标志位
	{
		std::lock_guard<std::mutex> lock(self->m_mtx);
		if (self->m_bHasNewData) {
			shouldUpdateScene = true;
			self->m_bHasNewData = false; // 重置标志位
		}
	}

	// 在安全的主线程环境中去更新所有的模型节点位置
	if (shouldUpdateScene) {
		self->ProcessRealSimSceneDrivenData();
	}

	return AsyncTask::DS_cont;
}

AsyncTask::DoneStatus HwaSimIR::capture_task(GenericAsyncTask* task, void* data) {
	HwaSimIR* self = static_cast<HwaSimIR*>(data);

	// 确保 TCP 线程已启动且纹理数据准备完毕
	if (self->m_pTcpThread && self->m_renderTex->has_ram_image()) {
		// 安全获取内存中的 RGB 像素点
		CPTA_uchar ram_image = self->m_renderTex->get_ram_image_as("RGB");
		if (ram_image) {
			int width = self->m_renderTex->get_x_size();
			int height = self->m_renderTex->get_y_size();

			// 将纯像素数据推送给 TCP 子线程（避免主线程做过多的耗时运算）
			self->m_pTcpThread->updateFrame(ram_image.p(), width, height);
		}
	}
	// 返回 DS_cont 让任务在下一帧继续执行
	return AsyncTask::DoneStatus::DS_cont;
}


// ========== main函数 ==========
int main(int argc, char *argv[]) {
	// 创建主应用实例
	HwaSimIR app(argc, argv);

	// 启动应用
	app.run();

	// 析构自动清理资源
	return 0;
}
