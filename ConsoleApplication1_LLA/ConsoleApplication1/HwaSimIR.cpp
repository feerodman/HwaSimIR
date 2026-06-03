// ConsoleApplication1.cpp : 定义控制台应用程序的入口点。
//
//#include "stdafx.h"

#include "HwaSimIR.h"
#include "lvecBase4.h"
#include "pta_LVecBase4.h"
#include "pta_float.h"
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace
{
bool FileExists(const std::string& path)
{
	std::ifstream file(path.c_str());
	return file.good();
}

double ClampStage5Double(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}

LVecBase3f Stage5SunDirectionLocal(double sunAzimuthDeg, double sunElevationDeg)
{
	const double pi = 3.14159265358979323846;
	const double az = sunAzimuthDeg * pi / 180.0;
	const double el = ClampStage5Double(sunElevationDeg, -90.0, 90.0) * pi / 180.0;
	const double cosEl = std::cos(el);
	return LVecBase3f(
		static_cast<float>(std::sin(az) * cosEl),
		static_cast<float>(std::cos(az) * cosEl),
		static_cast<float>(std::max(0.0, std::sin(el))));
}

bool ReadWholeFile(const std::string& path, std::string& text)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
	{
		return false;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	text = buffer.str();
	return true;
}

bool IsReasonableAltitudeMeters(double altitudeMeters)
{
	return altitudeMeters > -500.0 && altitudeMeters < 100000.0;
}

bool ReadProcessEnvFlag(const char* name, bool defaultValue)
{
#ifdef _MSC_VER
	char valueBuffer[16] = {};
	size_t required = 0;
	if (getenv_s(&required, valueBuffer, sizeof(valueBuffer), name) != 0 || required == 0)
	{
		return defaultValue;
	}
	std::string value(valueBuffer);
#else
	const char* envValue = std::getenv(name);
	if (envValue == nullptr)
	{
		return defaultValue;
	}
	std::string value(envValue);
#endif
	return value == "1" || value == "true" || value == "TRUE" || value == "True";
}

std::string ReadProcessEnvString(const char* name, const std::string& defaultValue)
{
#ifdef _MSC_VER
	char valueBuffer[128] = {};
	size_t required = 0;
	if (getenv_s(&required, valueBuffer, sizeof(valueBuffer), name) != 0 || required == 0)
	{
		return defaultValue;
	}
	return std::string(valueBuffer);
#else
	const char* envValue = std::getenv(name);
	return envValue == nullptr ? defaultValue : std::string(envValue);
#endif
}

double ReadProcessEnvDouble(const char* name, double defaultValue)
{
	std::string value = ReadProcessEnvString(name, "");
	if (value.empty())
	{
		return defaultValue;
	}
	char* end = nullptr;
	const double parsed = std::strtod(value.c_str(), &end);
	return (end != value.c_str()) ? parsed : defaultValue;
}

int ReadProcessEnvInt(const char* name, int defaultValue)
{
	std::string value = ReadProcessEnvString(name, "");
	if (value.empty())
	{
		return defaultValue;
	}
	char* end = nullptr;
	const long parsed = std::strtol(value.c_str(), &end, 10);
	return (end != value.c_str()) ? static_cast<int>(parsed) : defaultValue;
}

std::string ToLowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	return value;
}

int ParseStage5DebugViewMode(const std::string& value)
{
	std::string lower = ToLowerAscii(value);
	if (lower == "bodyonly" || lower == "body")
	{
		return 1;
	}
	if (lower == "hotspotonly" || lower == "hotspot")
	{
		return 2;
	}
	if (lower == "brightspotonly" || lower == "brightspot")
	{
		return 3;
	}
	return 0;
}

const char* Stage5DebugViewModeName(int viewMode)
{
	switch (viewMode)
	{
	case 1: return "BodyOnly";
	case 2: return "HotspotOnly";
	case 3: return "BrightSpotOnly";
	case 0:
	default:
		return "Composite";
	}
}

IRStage5ToneMap ParseStage5ToneMap(const std::string& value)
{
	std::string lower = ToLowerAscii(value);
	if (lower == "linear")
	{
		return IRStage5ToneMap::Linear;
	}
	if (lower == "log")
	{
		return IRStage5ToneMap::Log;
	}
	return IRStage5ToneMap::Asinh;
}

const char* Stage5ToneMapName(IRStage5ToneMap toneMap)
{
	switch (toneMap)
	{
	case IRStage5ToneMap::Linear: return "linear";
	case IRStage5ToneMap::Log: return "log";
	case IRStage5ToneMap::Asinh:
	default:
		return "asinh";
	}
}

int Stage5BandIndex(IRBand band)
{
	const int index = static_cast<int>(band);
	return (index >= 0 && index < 5) ? index : static_cast<int>(IRBand::MidWaveInfrared);
}

double DefaultStage5SolarReflectanceWeight(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return 1.0;
	case IRBand::NearInfrared: return 0.8;
	case IRBand::ShortWaveInfrared: return 0.7;
	case IRBand::MidWaveInfrared: return 0.15;
	case IRBand::LongWaveInfrared: return 0.0;
	default:
		return 0.0;
	}
}

IRBand Stage5BandFromIndex(int index)
{
	switch (index)
	{
	case 0: return IRBand::Visible;
	case 1: return IRBand::NearInfrared;
	case 2: return IRBand::ShortWaveInfrared;
	case 3: return IRBand::MidWaveInfrared;
	case 4: return IRBand::LongWaveInfrared;
	default:
		return IRBand::MidWaveInfrared;
	}
}

int IRBandClassForShader(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible:
	case IRBand::NearInfrared:
	case IRBand::ShortWaveInfrared:
		return 0; // reflective: VIS/NIR/SWIR
	case IRBand::MidWaveInfrared:
		return 1; // mixed: MWIR
	case IRBand::LongWaveInfrared:
		return 2; // thermal: LWIR
	default:
		return 1;
	}
}

std::string MakeJsonToken(const std::string& key)
{
	return "\"" + key + "\"";
}

bool MatchJsonObject(const std::string& text, size_t openBrace, std::string& objectText)
{
	if (openBrace == std::string::npos || openBrace >= text.size() || text[openBrace] != '{')
	{
		return false;
	}

	bool inString = false;
	bool escaping = false;
	int depth = 0;
	for (size_t i = openBrace; i < text.size(); ++i)
	{
		char c = text[i];
		if (inString)
		{
			if (escaping)
			{
				escaping = false;
			}
			else if (c == '\\')
			{
				escaping = true;
			}
			else if (c == '"')
			{
				inString = false;
			}
			continue;
		}

		if (c == '"')
		{
			inString = true;
		}
		else if (c == '{')
		{
			++depth;
		}
		else if (c == '}')
		{
			--depth;
			if (depth == 0)
			{
				objectText = text.substr(openBrace, i - openBrace + 1);
				return true;
			}
		}
	}
	return false;
}

bool FindJsonObject(const std::string& text, const std::string& key, std::string& objectText)
{
	const std::string token = MakeJsonToken(key);
	size_t pos = text.find(token);
	while (pos != std::string::npos)
	{
		size_t colon = text.find(':', pos + token.size());
		if (colon == std::string::npos)
		{
			return false;
		}
		size_t brace = text.find('{', colon + 1);
		if (brace != std::string::npos && MatchJsonObject(text, brace, objectText))
		{
			return true;
		}
		pos = text.find(token, pos + token.size());
	}
	return false;
}

bool ExtractJsonNumber(const std::string& text, const std::string& key, double& value)
{
	const std::string token = MakeJsonToken(key);
	size_t pos = text.find(token);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos = text.find(':', pos + token.size());
	if (pos == std::string::npos)
	{
		return false;
	}
	size_t begin = text.find_first_of("-+0123456789.", pos + 1);
	if (begin == std::string::npos)
	{
		return false;
	}
	size_t end = begin;
	while (end < text.size())
	{
		char c = text[end];
		if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E'))
		{
			break;
		}
		++end;
	}
	char* parsedEnd = nullptr;
	const std::string numberText = text.substr(begin, end - begin);
	const double parsed = std::strtod(numberText.c_str(), &parsedEnd);
	if (parsedEnd == numberText.c_str())
	{
		return false;
	}
	value = parsed;
	return true;
}

bool ExtractJsonBool(const std::string& text, const std::string& key, bool& value)
{
	const std::string token = MakeJsonToken(key);
	size_t pos = text.find(token);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos = text.find(':', pos + token.size());
	if (pos == std::string::npos)
	{
		return false;
	}
	size_t begin = text.find_first_not_of(" \t\r\n", pos + 1);
	if (begin == std::string::npos)
	{
		return false;
	}
	if (text.compare(begin, 4, "true") == 0 || text.compare(begin, 1, "1") == 0)
	{
		value = true;
		return true;
	}
	if (text.compare(begin, 5, "false") == 0 || text.compare(begin, 1, "0") == 0)
	{
		value = false;
		return true;
	}
	return false;
}

bool ExtractJsonString(const std::string& text, const std::string& key, std::string& value)
{
	const std::string token = MakeJsonToken(key);
	size_t pos = text.find(token);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos = text.find(':', pos + token.size());
	if (pos == std::string::npos)
	{
		return false;
	}
	size_t begin = text.find('"', pos + 1);
	if (begin == std::string::npos)
	{
		return false;
	}
	size_t end = text.find('"', begin + 1);
	if (end == std::string::npos)
	{
		return false;
	}
	value = text.substr(begin + 1, end - begin - 1);
	return true;
}

void ApplyStage5DebugConfigObject(const std::string& objectText, IRRadianceModelV2DebugConfig& config, bool& useBaseTextureModulation)
{
	std::string toneMapText;
	if (ExtractJsonString(objectText, "Stage5DebugToneMap", toneMapText))
	{
		config.toneMap = ParseStage5ToneMap(toneMapText);
	}
	ExtractJsonNumber(objectText, "Stage5BodyRadianceScale", config.bodyRadianceScale);
	ExtractJsonNumber(objectText, "Stage5HotspotRadianceScale", config.hotspotRadianceScale);
	ExtractJsonNumber(objectText, "Stage5BrightspotRadianceScale", config.brightspotRadianceScale);
	ExtractJsonNumber(objectText, "Stage5DebugMinBodyGray", config.minBodyGray);
	ExtractJsonNumber(objectText, "Stage5SolarReflectanceWeight", config.solarReflectanceWeight);
	ExtractJsonNumber(objectText, "Stage5BodyDisplayGain", config.bodyDisplayGain);
	ExtractJsonNumber(objectText, "Stage5ReflectedDisplayGain", config.reflectedDisplayGain);
	ExtractJsonNumber(objectText, "Stage5HotspotDisplayGain", config.hotspotDisplayGain);
	ExtractJsonNumber(objectText, "Stage5BrightspotDisplayGain", config.brightspotDisplayGain);
	ExtractJsonNumber(objectText, "Stage5CompositeMinGray", config.compositeMinGray);
	ExtractJsonNumber(objectText, "Stage5CompositeMaxGray", config.compositeMaxGray);
	ExtractJsonBool(objectText, "Stage5UseBaseTextureModulation", useBaseTextureModulation);
}

bool LoadStage5DebugDisplayConfig(
	const std::string& path,
	IRRadianceModelV2DebugConfig configs[5],
	bool useBaseTextureModulationByBand[5])
{
	for (int i = 0; i < 5; ++i)
	{
		configs[i] = IRRadianceModelV2DebugConfig();
		configs[i].solarReflectanceWeight = DefaultStage5SolarReflectanceWeight(Stage5BandFromIndex(i));
		useBaseTextureModulationByBand[i] = false;
	}

	std::string text;
	if (!ReadWholeFile(path, text))
	{
		return false;
	}

	IRRadianceModelV2DebugConfig defaultConfig;
	bool defaultUseBaseTextureModulation = false;
	std::string defaultsText;
	if (FindJsonObject(text, "defaults", defaultsText))
	{
		ApplyStage5DebugConfigObject(defaultsText, defaultConfig, defaultUseBaseTextureModulation);
	}
	for (int i = 0; i < 5; ++i)
	{
		configs[i] = defaultConfig;
		configs[i].solarReflectanceWeight = DefaultStage5SolarReflectanceWeight(Stage5BandFromIndex(i));
		useBaseTextureModulationByBand[i] = defaultUseBaseTextureModulation;
	}

	std::string bandsText;
	if (!FindJsonObject(text, "bands", bandsText))
	{
		return true;
	}

	const IRBand bands[5] = {
		IRBand::Visible,
		IRBand::NearInfrared,
		IRBand::ShortWaveInfrared,
		IRBand::MidWaveInfrared,
		IRBand::LongWaveInfrared
	};
	for (int i = 0; i < 5; ++i)
	{
		std::string bandText;
		if (FindJsonObject(bandsText, IRBandName(bands[i]), bandText))
		{
			ApplyStage5DebugConfigObject(bandText, configs[Stage5BandIndex(bands[i])], useBaseTextureModulationByBand[Stage5BandIndex(bands[i])]);
		}
	}
	ExtractJsonNumber(text, "Stage5SolarReflectanceWeight_VIS", configs[Stage5BandIndex(IRBand::Visible)].solarReflectanceWeight);
	ExtractJsonNumber(text, "Stage5SolarReflectanceWeight_NIR", configs[Stage5BandIndex(IRBand::NearInfrared)].solarReflectanceWeight);
	ExtractJsonNumber(text, "Stage5SolarReflectanceWeight_SWIR", configs[Stage5BandIndex(IRBand::ShortWaveInfrared)].solarReflectanceWeight);
	ExtractJsonNumber(text, "Stage5SolarReflectanceWeight_MWIR", configs[Stage5BandIndex(IRBand::MidWaveInfrared)].solarReflectanceWeight);
	ExtractJsonNumber(text, "Stage5SolarReflectanceWeight_LWIR", configs[Stage5BandIndex(IRBand::LongWaveInfrared)].solarReflectanceWeight);
	return true;
}

void ApplyStage5EnvOverrides(IRRadianceModelV2DebugConfig configs[5], bool useBaseTextureModulationByBand[5])
{
	for (int i = 0; i < 5; ++i)
	{
		configs[i].toneMap = ParseStage5ToneMap(ReadProcessEnvString("Stage5DebugToneMap", Stage5ToneMapName(configs[i].toneMap)));
		configs[i].bodyRadianceScale = ReadProcessEnvDouble("Stage5BodyRadianceScale", configs[i].bodyRadianceScale);
		configs[i].hotspotRadianceScale = ReadProcessEnvDouble("Stage5HotspotRadianceScale", configs[i].hotspotRadianceScale);
		configs[i].brightspotRadianceScale = ReadProcessEnvDouble("Stage5BrightspotRadianceScale", configs[i].brightspotRadianceScale);
		configs[i].minBodyGray = ReadProcessEnvDouble("Stage5DebugMinBodyGray", configs[i].minBodyGray);
		configs[i].solarReflectanceWeight = ReadProcessEnvDouble("Stage5SolarReflectanceWeight", configs[i].solarReflectanceWeight);
		configs[i].bodyDisplayGain = ReadProcessEnvDouble("Stage5BodyDisplayGain", configs[i].bodyDisplayGain);
		configs[i].reflectedDisplayGain = ReadProcessEnvDouble("Stage5ReflectedDisplayGain", configs[i].reflectedDisplayGain);
		configs[i].hotspotDisplayGain = ReadProcessEnvDouble("Stage5HotspotDisplayGain", configs[i].hotspotDisplayGain);
		configs[i].brightspotDisplayGain = ReadProcessEnvDouble("Stage5BrightspotDisplayGain", configs[i].brightspotDisplayGain);
		configs[i].compositeMinGray = ReadProcessEnvDouble("Stage5CompositeMinGray", configs[i].compositeMinGray);
		configs[i].compositeMaxGray = ReadProcessEnvDouble("Stage5CompositeMaxGray", configs[i].compositeMaxGray);
		useBaseTextureModulationByBand[i] = ReadProcessEnvFlag("Stage5UseBaseTextureModulation", useBaseTextureModulationByBand[i]);
	}
	configs[Stage5BandIndex(IRBand::Visible)].solarReflectanceWeight = ReadProcessEnvDouble("Stage5SolarReflectanceWeight_VIS", configs[Stage5BandIndex(IRBand::Visible)].solarReflectanceWeight);
	configs[Stage5BandIndex(IRBand::NearInfrared)].solarReflectanceWeight = ReadProcessEnvDouble("Stage5SolarReflectanceWeight_NIR", configs[Stage5BandIndex(IRBand::NearInfrared)].solarReflectanceWeight);
	configs[Stage5BandIndex(IRBand::ShortWaveInfrared)].solarReflectanceWeight = ReadProcessEnvDouble("Stage5SolarReflectanceWeight_SWIR", configs[Stage5BandIndex(IRBand::ShortWaveInfrared)].solarReflectanceWeight);
	configs[Stage5BandIndex(IRBand::MidWaveInfrared)].solarReflectanceWeight = ReadProcessEnvDouble("Stage5SolarReflectanceWeight_MWIR", configs[Stage5BandIndex(IRBand::MidWaveInfrared)].solarReflectanceWeight);
	configs[Stage5BandIndex(IRBand::LongWaveInfrared)].solarReflectanceWeight = ReadProcessEnvDouble("Stage5SolarReflectanceWeight_LWIR", configs[Stage5BandIndex(IRBand::LongWaveInfrared)].solarReflectanceWeight);
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

double WeatherSunStrength(int weatherCode, double sunElevationDeg)
{
	if (sunElevationDeg <= 0.0)
	{
		return 0.0;
	}
	switch (weatherCode)
	{
	case 0: return 1.0;   // 晴天：太阳直射完整保留
	case 1: return 0.65;  // 云：削弱直射，保留部分散射
	case 2: return 0.45;  // 雨：直射和能见度同时下降
	case 3: return 0.55;  // 雪：直射下降，但地表反照可后续增强
	case 4: return 0.30;  // 雾：太阳项大幅衰减，程辐射增强
	case 5: return 0.42;  // 阴：无明显直射，以漫射近似
	default: return 0.75;
	}
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

	const WindowProperties old_props = graphics_win->get_properties();
	const int old_width = old_props.get_x_size();
	const int old_height = old_props.get_y_size();

	// 复制当前窗口属性，仅修改尺寸
	WindowProperties new_props = graphics_win->get_properties();
	new_props.set_size(new_width, new_height);

	graphics_win->request_properties(new_props);
	m_pMainWindow->adjust_dimensions();

	if (m_renderTex != nullptr) {
		m_renderTex->setup_2d_texture(new_width, new_height, Texture::T_unsigned_byte, Texture::F_rgb);
	}

	std::cout << "[Stage6 Resize]"
		<< " oldWindow=" << old_width << "x" << old_height
		<< " newWindow=" << new_width << "x" << new_height
		<< " renderTexture=" << new_width << "x" << new_height
		<< " resizeRequest=1"
		<< std::endl;
}

void HwaSimIR::ApplySensorOutputConfig(const IRSensorDisplayConfig& config, const char* reason)
{
	m_sensorDisplayConfig = config;
	m_sensorDisplayConfigReady = true;
	m_stage6FarClipWarningLogged = false;
	m_stage6CaptureLogCounter = 0;

	if (m_cameraLens != nullptr) {
		m_cameraLens->set_fov(config.horizontalFovDeg, config.verticalFovDeg);
		m_cameraLens->set_near_far(config.nearClipM, config.farClipM);
	}

	resize_window(config.width, config.height);
	LogStage6SensorGeometry(config, reason);
}

void HwaSimIR::LogStage6SensorGeometry(const IRSensorDisplayConfig& config, const char* reason) const
{
	const char* safeReason = (reason != nullptr) ? reason : "unknown";
	std::cout << "[Stage6 SensorGeometry]"
		<< " reason=" << safeReason
		<< " width=" << config.width
		<< " height=" << config.height
		<< " viewMin=" << config.viewMinM
		<< " viewMax=" << config.viewMaxM
		<< " pixelAngleUrad=" << config.pixelAngleUrad
		<< " horizontalFovDeg=" << config.horizontalFovDeg
		<< " verticalFovDeg=" << config.verticalFovDeg
		<< " nearClipM=" << config.nearClipM
		<< " farClipM=" << config.farClipM
		<< " fov_source=" << config.fovSource
		<< std::endl;

	if (config.widthFallback || config.heightFallback || config.nearFallback ||
		config.farFallback || config.pixelAngleFallback || config.pixelAngleRangeWarning) {
		std::cout << "[Stage6 SensorGeometry][WARN]"
			<< " requestedWidth=" << config.requestedWidth
			<< " requestedHeight=" << config.requestedHeight
			<< " requestedViewMin=" << config.requestedViewMinM
			<< " requestedViewMax=" << config.requestedViewMaxM
			<< " requestedPixelAngleUrad=" << config.requestedPixelAngleUrad
			<< " widthFallback=" << (config.widthFallback ? "1" : "0")
			<< " heightFallback=" << (config.heightFallback ? "1" : "0")
			<< " nearFallback=" << (config.nearFallback ? "1" : "0")
			<< " farFallback=" << (config.farFallback ? "1" : "0")
			<< " pixelAngleFallback=" << (config.pixelAngleFallback ? "1" : "0")
			<< " pixelAngleRangeWarning=" << (config.pixelAngleRangeWarning ? "1" : "0")
			<< " pixelAngleClamped=" << (config.pixelAngleClamped ? "1" : "0")
			<< std::endl;
	}
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
	// 飞机：协议 0x11 当前仍默认绑定 F35；F22 已入库但暂未接协议枚举，避免改变现有测试语义。
	m_platformResMap[F35] = {
		"Config/TargetLib/models/f35/F35C.obj",
		"Config/TargetLib/models/f35/f35c.jpg",
		"Config/TargetLib/models/f35/f35c_mat.tif",
		"Config/TargetLib/models/f35/f35c_mat.tif.xml",
		"Config/TargetLib/models/f35",
		"F35",
		"BM_METAL-ALUMINIUM"
	};

	// 导弹：资源目录使用新加入的 AIM9X/AIM120D 资产，保持协议类型 0x33/0x22 不变。
	m_platformResMap[AIM9] = {
		"Config/TargetLib/models/aim9x/aim9x.obj",
		"Config/TargetLib/models/aim9x/TX_AIM9X_Diffuse.png",
		"Config/TargetLib/models/aim9x/TX_AIM9X_Diffuse_mat.tif",
		"Config/TargetLib/models/aim9x/TX_AIM9X_Diffuse_mat.tif.xml",
		"Config/TargetLib/models/aim9x",
		"AIM9X",
		"IR_CERAMIC"
	};
	m_platformResMap[AIM120] = {
		"Config/TargetLib/models/aim120/AIM120.obj",
		"Config/TargetLib/models/aim120/aim120.jpg",
		"Config/TargetLib/models/aim120/aim120_mat.tif",
		"Config/TargetLib/models/aim120/aim120_mat.tif.xml",
		"Config/TargetLib/models/aim120",
		"AIM120D",
		"BM_METAL-STEEL"
	};

	std::cout << "平台模型路径初始化完成，共加载" << m_platformResMap.size() << "种平台资源" << std::endl;


	// 旧的单模型调试代码保留为参考，路径已迁移到新的 AIM9X 资产目录。
#if 0

	Filename aim9_path = Filename::from_os_specific("Config/TargetLib/models/aim9x/aim9x.obj");
	Filename aim9_texture_path = Filename::from_os_specific("Config/TargetLib/models/aim9x/TX_AIM9X_Diffuse.png");

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
	m_cameraLens->set_near_far(1.0, 100000.0);


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
	IRSensorDisplayConfig sensorDisplayConfig = m_irSensorModel.BuildSensorDisplayConfig(
		m_sensorParam.trackerSensorWidth,
		m_sensorParam.trackerSensorHeight,
		m_sensorParam.trackerSensorViewMin,
		m_sensorParam.trackerSensorViewMax,
		m_sensorParam.trackerSensorPixelAngle);
	ApplySensorOutputConfig(sensorDisplayConfig, "init-command");

	// 设置增删标记为"增加"
	m_isAddPlatform = true;
	m_irTemperatureModel.resetRuntime();
	m_lastStage4UpdateTime = -1.0;

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

	// TargetState[5] 最多携带5组目标数据；targetNumValid只控制前N个是否参与显示，不再限制状态更新。
	const int visibleTargetNum = std::max(0, std::min(currentData.targetNumValid, 5));
	for (auto& targetPlat : m_targetPlatformList)
	{
		if (targetPlat.isExist)
		{
			targetPlat.nodePath.hide();
		}
	}

	TargetPlatformData* lookAtTarget = nullptr;
	for (int i = 0; i < 5; ++i)
	{
		const BYHWICD::TargetState& targetState = currentData.targetState[i];
		if (!IsValidTargetStateKey(targetState))
		{
			continue;
		}

		TargetPlatformData* targetPlat = FindOrMapTargetPlatform(targetState, i);
		if (targetPlat == nullptr)
		{
			continue;
		}

		// 通过 targetType + targetPlatID + targetID 唯一键更新同一个目标，避免不同挂载平台目标ID冲突。
		targetPlat->targetState = targetState;
		targetPlat->platID = targetState.targetID;

		const BYHWICD::SpatialState& spatial = targetState.targetLoc;
		LMatrix4f exactTransform = m_geoTrans.GetPandaMatrix(spatial);
		targetPlat->nodePath.set_mat(LMatrix4(exactTransform));
		const float targetRangeM = EstimateRangeToCamera(targetPlat->nodePath);
		if (m_sensorDisplayConfigReady &&
			targetRangeM > static_cast<float>(m_sensorDisplayConfig.farClipM) &&
			!m_stage6FarClipWarningLogged)
		{
			std::cout << "STAGE6_TARGET_BEYOND_FAR_CLIP"
				<< " targetType=0x" << std::hex << targetState.targetType << std::dec
				<< " targetPlatID=" << targetState.targetPlatID
				<< " targetID=" << targetState.targetID
				<< " rangeM=" << targetRangeM
				<< " farClipM=" << m_sensorDisplayConfig.farClipM
				<< std::endl;
			m_stage6FarClipWarningLogged = true;
		}

		bool renderVisible = (i < visibleTargetNum) && targetState.viewValid;
		if (WeaponTargetKeyMatches(currentData.weaponState, *targetPlat) && !currentData.weaponState.viewValid)
		{
			renderVisible = false;
		}
		if (renderVisible)
		{
			targetPlat->nodePath.show();
		}
		else
		{
			targetPlat->nodePath.hide();
		}

		if (WeaponTargetKeyMatches(currentData.weaponState, *targetPlat))
		{
			lookAtTarget = targetPlat;
		}

		if (m_stage0DisplayFrameCount <= 3 || (m_stage0DisplayFrameCount % 60) == 0)
		{
			std::cout << "[TargetMapping]"
				<< " packet=" << m_stage0DisplayFrameCount
				<< " index=" << i
				<< " targetType=0x" << std::hex << targetState.targetType << std::dec
				<< " targetPlatID=" << targetState.targetPlatID
				<< " targetID=" << targetState.targetID
				<< " visibleByTargetNum=" << (i < visibleTargetNum ? "1" : "0")
				<< " targetViewValid=" << (targetState.viewValid ? "1" : "0")
				<< " weaponViewValid=" << (currentData.weaponState.viewValid ? "1" : "0")
				<< " renderVisible=" << (renderVisible ? "1" : "0")
				<< std::endl;
		}
	}
	m_isInitTargetPlatID = true;

	ApplyWeaponCameraControl(currentData, lookAtTarget);

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


PLATFORM_TYPE HwaSimIR::TargetTypeToPlatformType(int targetType) const
{
	switch (targetType)
	{
	case 0x00: return NONE;
	case 0x11: return F35; // 飞机类型暂时默认F35
	case 0x22: return AIM120;
	case 0x33: return AIM9;
	case 0x44: return MMD;
	default: return NONE;
	}
}

NodePath HwaSimIR::LoadPlatformAssetNode(PLATFORM_TYPE type, const PlatformResPath& res)
{
	Filename modelPath = Filename::from_os_specific(res.modelPath);
	NodePath modelNode = m_pMainWindow->load_model(m_renderRoot, modelPath);
	if (modelNode.is_empty())
	{
		std::cerr << "模型加载失败：" << res.modelPath << " 类型=" << type << std::endl;
		return modelNode;
	}

	// OBJ/MTL 可能带有旧机器的绝对贴图路径，这里显式绑定基础纹理保证 Windows 和 RK3588 路径一致。
	if (!res.texturePath.empty())
	{
		Filename texturePath = Filename::from_os_specific(res.texturePath);
		PT(Texture) texture = TexturePool::load_texture(texturePath);
		if (texture != nullptr)
		{
			modelNode.set_texture(texture);
		}
		else
		{
			std::cerr << "基础纹理加载失败：" << res.texturePath << " 类型=" << type << std::endl;
		}
	}

	// 先挂红外 shader，再绑定材质 ID 纹理和材质参数数组，确保 shader input 已声明可用。
	ApplyInfraredShader(modelNode, false);
	m_irSceneMaterialMapper.bindPlatformNode(modelNode, res, m_irMaterialDatabase);
	return modelNode;
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
			// 阶段2统一入口：加载模型、基础纹理，并绑定材质ID纹理/材质参数。
			NodePath modelNode = LoadPlatformAssetNode(platType, resIter->second);
			if (modelNode.is_empty())
			{
				continue;
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

			std::cout << "PlatParamPak平台生成成功：类型=" << platType << " ID=" << platParam.id << std::endl;
			// ========== 绑定相机到第一个平台 ==========
			if (i==0 && !m_isCameraAttached) {
				//m_cameraNode = m_pMainWindow->get_camera_group();
				//m_camera = m_pMainWindow->get_camera();
				m_cameraNode.reparent_to(modelNode);
				m_cameraNode.set_pos(0, 0, 0); // 往后15单位，往上8单位
				//m_cameraNode.look_at(modelNode);
												 // 标记相机已绑定
				m_isCameraAttached = true;
				std::cout << "相机已绑定到第一个PlatParamPak平台（ID=" << platParam.id << "），偏移：(0, 0, -8)" << std::endl;
				if (m_sensorDisplayConfigReady) {
					std::cout << "相机视场角(Stage6 SensorGeometry)："
						<< m_sensorDisplayConfig.horizontalFovDeg << ","
						<< m_sensorDisplayConfig.verticalFovDeg << std::endl;
				}
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

			// 阶段2统一入口：加载模型、基础纹理，并绑定材质ID纹理/材质参数。
			NodePath modelNode = LoadPlatformAssetNode(platType, resIter->second);
			if (modelNode.is_empty()) continue;

			// 初始化TargetPlatformData
			TargetPlatformData newTargetPlat;
			newTargetPlat.type = platType;
			newTargetPlat.platID = -1; // 初始化时尚未绑定协议目标ID，Display包到达后按三元组映射
			newTargetPlat.targetState.targetType = 0x22;
			newTargetPlat.targetState.targetPlatID = -1;
			newTargetPlat.targetState.targetID = -1;
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
			modelNode.hide(); // 未收到有效TargetState前不渲染

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

			// 阶段2统一入口：加载模型、基础纹理，并绑定材质ID纹理/材质参数。
			NodePath modelNode = LoadPlatformAssetNode(platType, resIter->second);
			if (modelNode.is_empty()) continue;

			// 初始化TargetPlatformData
			TargetPlatformData newTargetPlat;
			newTargetPlat.type = platType;
			newTargetPlat.platID = -1; // 初始化时尚未绑定协议目标ID，Display包到达后按三元组映射
			newTargetPlat.targetState.targetType = 0x33;
			newTargetPlat.targetState.targetPlatID = -1;
			newTargetPlat.targetState.targetID = -1;
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
			modelNode.hide(); // 未收到有效TargetState前不渲染

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

			// 阶段2统一入口：MMD 暂无资产时不会进入加载；后续补模型后沿用同一材质绑定流程。
			NodePath modelNode = LoadPlatformAssetNode(platType, resIter->second);
			if (modelNode.is_empty()) continue;

			// 初始化TargetPlatformData
			TargetPlatformData newTargetPlat;
			newTargetPlat.type = platType;
			newTargetPlat.platID = -1; // 初始化时尚未绑定协议目标ID，Display包到达后按三元组映射
			newTargetPlat.targetState.targetType = 0x44;
			newTargetPlat.targetState.targetPlatID = -1;
			newTargetPlat.targetState.targetID = -1;
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
			modelNode.hide(); // 未收到有效TargetState前不渲染

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
		m_sensorDisplayConfigReady = false;
		m_stage6FarClipWarningLogged = false;
		m_stage6CaptureLogCounter = 0;

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
		<< ", viewMinMax=" << sensor.trackerSensorViewMin << "/" << sensor.trackerSensorViewMax
		<< ", pixelAngleUrad=" << sensor.trackerSensorPixelAngle
		<< ", legacyResolution=" << sensor.coarseTrackResolution << "x" << sensor.preciseTrackResolution
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

	// 阶段3：初始化后立即合成一次环境状态，保证 UDP 环境参数优先级生效。
	IRRuntimeEnvironment initEnvironment = BuildRuntimeEnvironment();
	m_irRadianceModel.setEnvironment(initEnvironment);
	LogActiveIREnvironment(initEnvironment, "init-command", true);




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
	const int visibleTargetNumForLog = std::max(0, std::min(data.targetNumValid, 5));
	int stage4LogTargetCount = 0;
	bool anyStage4InputSignal = data.weaponState.strikeFlag || data.weaponState.strikePart != 0;
	for (int i = 0; i < 5; ++i)
	{
		const auto& target = data.targetState[i];
		if (i >= visibleTargetNumForLog && TargetTypeToPlatformType(target.targetType) == NONE)
		{
			continue;
		}
		++stage4LogTargetCount;
		anyStage4InputSignal = anyStage4InputSignal || target.engineState;
		std::cout << "[Stage4 Input]"
			<< " packet=" << m_stage0DisplayFrameCount
			<< " targetIndex=" << i
			<< " targetID=" << target.targetID
			<< " targetPlatID=" << target.targetPlatID
			<< " platform=" << Stage4PlatformName(TargetTypeToPlatformType(target.targetType))
			<< " engineState=" << (target.engineState ? "1" : "0")
			<< " targetViewValid=" << (target.viewValid ? "1" : "0")
			<< " visibleByTargetNum=" << (i < visibleTargetNumForLog ? "1" : "0")
			<< " strikeFlag=" << (data.weaponState.strikeFlag ? "1" : "0")
			<< " strikePart=" << data.weaponState.strikePart
			<< " weaponTargetType=0x" << std::hex << data.weaponState.targetType << std::dec
			<< " weaponTargetID=" << data.weaponState.targetID
			<< " weaponTargetPlatID=" << data.weaponState.targetPlatID
			<< " weaponViewValid=" << (data.weaponState.viewValid ? "1" : "0")
			<< " sourcePacketTimeMs=" << data.time
			<< std::endl;
	}
	if (stage4LogTargetCount == 0 || (!anyStage4InputSignal && (m_stage0DisplayFrameCount % 100) == 0))
	{
		std::cout << "[Stage4 Input][WARN] STAGE4_INPUT_NOT_UPDATED"
			<< " packet=" << m_stage0DisplayFrameCount
			<< " targetNumValid=" << data.targetNumValid
			<< " strikeFlag=" << (data.weaponState.strikeFlag ? "1" : "0")
			<< " strikePart=" << data.weaponState.strikePart
			<< " reason=" << (stage4LogTargetCount == 0 ? "no_valid_targets" : "no_engine_or_strike_signal")
			<< std::endl;
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
	std::string modtranBandLutPath = FirstExistingPath({
		"Config/Atmosphere/MODTRAN/processed/band_lut.csv",
		"../Bin/Config/Atmosphere/MODTRAN/processed/band_lut.csv",
		"ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/processed/band_lut.csv",
		"../ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/processed/band_lut.csv"
	});
	std::string weatherPath = FirstExistingPath({
		"temperatures/Temperatures_Yemen_Summer.csv",
		"../temperatures/Temperatures_Yemen_Summer.csv",
		"../../temperatures/Temperatures_Yemen_Summer.csv"
	});
	std::vector<std::string> sensorWaveDirs;
	sensorWaveDirs.push_back("Config/SensorWave");
	sensorWaveDirs.push_back("../Bin/Config/SensorWave");
	sensorWaveDirs.push_back("ConsoleApplication1_LLA/Bin/Config/SensorWave");
	sensorWaveDirs.push_back("../ConsoleApplication1_LLA/Bin/Config/SensorWave");
	std::vector<std::string> hotspotConfigPaths;
	hotspotConfigPaths.push_back("Config/IRHotspots/target_hotspots.json");
	hotspotConfigPaths.push_back("../Bin/Config/IRHotspots/target_hotspots.json");
	hotspotConfigPaths.push_back("ConsoleApplication1_LLA/Bin/Config/IRHotspots/target_hotspots.json");
	hotspotConfigPaths.push_back("../ConsoleApplication1_LLA/Bin/Config/IRHotspots/target_hotspots.json");
	std::vector<std::string> stage5DebugConfigPaths;
	stage5DebugConfigPaths.push_back("Config/IRRadiance/stage5_debug_display.json");
	stage5DebugConfigPaths.push_back("../Bin/Config/IRRadiance/stage5_debug_display.json");
	stage5DebugConfigPaths.push_back("ConsoleApplication1_LLA/Bin/Config/IRRadiance/stage5_debug_display.json");
	stage5DebugConfigPaths.push_back("../ConsoleApplication1_LLA/Bin/Config/IRRadiance/stage5_debug_display.json");

	m_irMaterialReady = m_irMaterialDatabase.load(materialPath);
	m_irAtmosphereReady = m_irAtmosphereModel.loadTransmissionTable(transmittancePath);
	bool modtranTauLutReady = m_irAtmosphereModel.loadModtranBandLut(modtranBandLutPath);
	bool enableModtranTauDebug = ReadProcessEnvFlag("EnableModtranTauDebug", false);
	bool useModtranTauForAtmosphere = ReadProcessEnvFlag("UseModtranTauForAtmosphere", false);
	m_enableStage4HotspotVisualDebug = ReadProcessEnvFlag("EnableStage4HotspotVisualDebug", false);
	m_forceStage4BrightSpotVisible = ReadProcessEnvFlag("ForceStage4BrightSpotVisible", false);
	m_forceStage4RearHotspotVisible = ReadProcessEnvFlag("ForceStage4RearHotspotVisible", false);
	m_stage5DebugDisplayConfigPath = FirstExistingPath(stage5DebugConfigPaths);
	m_stage5DebugDisplayConfigReady = LoadStage5DebugDisplayConfig(m_stage5DebugDisplayConfigPath, m_stage5DebugConfigs, m_stage5UseBaseTextureModulationByBand);
	ApplyStage5EnvOverrides(m_stage5DebugConfigs, m_stage5UseBaseTextureModulationByBand);
	m_enableStage5RadianceDebug = ReadProcessEnvFlag("EnableStage5RadianceDebug", false);
	m_stage5DebugViewMode = ParseStage5DebugViewMode(ReadProcessEnvString("Stage5DebugViewMode", "Composite"));
	m_stage5DebugViewModeName = Stage5DebugViewModeName(m_stage5DebugViewMode);
	m_stage5DebugConfig = m_stage5DebugConfigs[Stage5BandIndex(IRBand::MidWaveInfrared)];
	m_stage5DebugToneMapName = Stage5ToneMapName(m_stage5DebugConfig.toneMap);
	m_stage5UseBaseTextureModulation = m_stage5UseBaseTextureModulationByBand[Stage5BandIndex(IRBand::MidWaveInfrared)];
	m_stage5OutputFrameDumpEnabled = ReadProcessEnvFlag("Stage5OutputFrameDump", false);
	m_stage5OutputFrameDumpPath = ReadProcessEnvString("Stage5OutputFrameDumpPath", "");
	m_stage5OutputFrameDumpEvery = std::max(1, ReadProcessEnvInt("Stage5OutputFrameDumpEvery", 5));
	m_stage5OutputFrameCounter = 0;
	m_stage5OutputFrameDumpWrites = 0;
	m_stage5OutputFrameDumpFailureLogged = false;
	m_irAtmosphereModel.setModtranTauDebugEnabled(enableModtranTauDebug);
	m_irAtmosphereModel.setUseModtranTauForAtmosphere(useModtranTauForAtmosphere);
	m_irSensorProfilesReady = m_irSensorProfiles.loadFromDirectoryCandidates(sensorWaveDirs);
	m_irWeatherReady = m_irWeatherProfile.load(weatherPath);
	m_irTemperatureReady = m_irTemperatureModel.loadFromFileCandidates(hotspotConfigPaths);
	m_irRadianceModel.setMaterialDatabase(&m_irMaterialDatabase);
	m_irRadianceModel.setAtmosphereModel(&m_irAtmosphereModel);

	IRRuntimeEnvironment environment;
	environment.band = IRBandFromProtocol(2);
	if (m_irWeatherReady)
	{
		// 阶段3默认使用正午 profile，真实运行时由实时数据时间换算仿真小时。
		IRWeatherSample sample = m_irWeatherProfile.sampleForHour(12.0);
		environment.airTemperatureC = sample.airTemperatureC;
		environment.sunAzimuthDeg = sample.sunAzimuthDeg;
		environment.sunElevationDeg = sample.sunElevationDeg;
		environment.simulationHour = sample.hour;
	}
	else
	{
		environment.airTemperatureC = 25.0;
	}
	environment.visibilityMeters = 23000.0;
	environment.sunStrength = WeatherSunStrength(environment.weatherCode, environment.sunElevationDeg);
	m_irRadianceModel.setEnvironment(environment);

	std::cout << "红外全链路CPU模型初始化：材质库="
		<< (m_irMaterialReady ? "OK" : "未加载，使用默认材质")
		<< " 路径=" << materialPath
		<< "，MODTRAN透过率="
		<< (m_irAtmosphereReady ? "OK" : "未加载，使用经验透过率")
		<< " 路径=" << transmittancePath << std::endl;
	std::cout << "[Stage3] MODTRAN tau-only debug LUT="
		<< (modtranTauLutReady ? "OK" : "未加载，仅使用旧透过率表")
		<< " 路径=" << modtranBandLutPath
		<< " EnableModtranTauDebug=" << (enableModtranTauDebug ? "1" : "0")
		<< " UseModtranTauForAtmosphere=" << (useModtranTauForAtmosphere ? "1" : "0")
		<< "（默认仅日志对比；active=1 才返回 MODTRAN tau）" << std::endl;
	std::cout << "[Stage1] IR配置输入：SensorWave="
		<< (m_irSensorProfilesReady ? "OK" : "未加载，使用内置传感器默认值")
		<< " 路径=" << (m_irSensorProfilesReady ? m_irSensorProfiles.loadedDirectory() : "fallback")
		<< "，MaterialDatabase=" << (m_irMaterialReady ? materialPath : "fallback")
		<< "，Transmittance=" << (m_irAtmosphereReady ? transmittancePath : "fallback")
		<< "，WeatherProfile=" << (m_irWeatherReady ? weatherPath : "fallback") << std::endl;
	std::cout << "[Stage4] IRHotspots配置："
		<< (m_irTemperatureReady ? "OK" : "未加载，使用内置安全默认值")
		<< " 路径=" << (m_irTemperatureReady ? m_irTemperatureModel.loadedPath() : "fallback")
		<< "（ThermalHotspot与BrightSpot分离；本阶段只处理发动机热源与特殊亮斑）" << std::endl;
	std::cout << "[Stage4] Visual debug flags:"
		<< " EnableStage4HotspotVisualDebug=" << (m_enableStage4HotspotVisualDebug ? "1" : "0")
		<< " ForceStage4BrightSpotVisible=" << (m_forceStage4BrightSpotVisible ? "1" : "0")
		<< " ForceStage4RearHotspotVisible=" << (m_forceStage4RearHotspotVisible ? "1" : "0")
		<< "（默认全为0；仅用于可见性接线诊断）" << std::endl;
	std::cout << "[Stage5 Radiance] Stage5DebugDisplayConfig="
		<< (m_stage5DebugDisplayConfigReady ? "OK" : "fallback")
		<< " path=" << m_stage5DebugDisplayConfigPath
		<< "（只配置debug显示映射，不改变EnableStage5RadianceDebug默认关闭状态）" << std::endl;
	std::cout << "[Stage5 Radiance] EnableStage5RadianceDebug=" << (m_enableStage5RadianceDebug ? "1" : "0");
	if (!m_enableStage5RadianceDebug)
	{
		std::cout << " Stage5 debug disabled, legacy output may remain dark.";
	}
	else
	{
		std::cout << " 最小目标自身辐射链路启用：bodyRadiance + rear ThermalHotspot + BrightSpot；path/sky/solar radiance disabled."
			<< " Stage5DebugViewMode=" << m_stage5DebugViewModeName
			<< " Stage5DebugToneMap=" << m_stage5DebugToneMapName
			<< " Stage5BodyRadianceScale=" << m_stage5DebugConfig.bodyRadianceScale
			<< " Stage5HotspotRadianceScale=" << m_stage5DebugConfig.hotspotRadianceScale
			<< " Stage5BrightspotRadianceScale=" << m_stage5DebugConfig.brightspotRadianceScale
			<< " Stage5SolarReflectanceWeight=" << m_stage5DebugConfig.solarReflectanceWeight
			<< " Stage5DebugMinBodyGray=" << m_stage5DebugConfig.minBodyGray
			<< " Stage5UseBaseTextureModulation=" << (m_stage5UseBaseTextureModulation ? "1" : "0")
			<< " Stage5BodyDisplayGain=" << m_stage5DebugConfig.bodyDisplayGain
			<< " Stage5ReflectedDisplayGain=" << m_stage5DebugConfig.reflectedDisplayGain
			<< " Stage5HotspotDisplayGain=" << m_stage5DebugConfig.hotspotDisplayGain
			<< " Stage5BrightspotDisplayGain=" << m_stage5DebugConfig.brightspotDisplayGain
			<< " Stage5CompositeMinGray=" << m_stage5DebugConfig.compositeMinGray
			<< " Stage5CompositeMaxGray=" << m_stage5DebugConfig.compositeMaxGray;
	}
	std::cout << std::endl;
	std::cout << "[Stage5 OutputCapture] Stage5OutputFrameDump="
		<< ((m_stage5OutputFrameDumpEnabled && m_enableStage5RadianceDebug) ? "1" : "0")
		<< " path=" << (m_stage5OutputFrameDumpPath.empty() ? "NA" : m_stage5OutputFrameDumpPath)
		<< " every=" << m_stage5OutputFrameDumpEvery
		<< "（smoke-only render texture dump; TCP/JPEG protocol unchanged）" << std::endl;
	LogActiveIRSensorProfile(2, "startup-default", true);
	LogActiveIREnvironment(environment, "startup-default", true);
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
    attribute vec3 p3d_Normal;
    attribute vec2 p3d_MultiTexCoord0;
    
    varying vec2 texcoord;
    varying vec3 v_local_pos; // 传递模型局部坐标系下的三维坐标
    varying vec3 v_stage5_normal;

    void main() {
        gl_Position = p3d_ModelViewProjectionMatrix * p3d_Vertex;
        texcoord = p3d_MultiTexCoord0;
        v_local_pos = p3d_Vertex.xyz; // 提取局部坐标
        v_stage5_normal = p3d_Normal;
    }
    )";

	// 片段着色器：实现白热模式、中/长波切换、一头一尾独立热源亮斑
	std::string fragment_shader = R"(
    #version 100
    precision mediump float;

    uniform sampler2D p3d_Texture0;
    
    uniform int u_is_background;
    uniform int u_object_kind;     // 0:目标 1:天空 2:粒子云
    uniform int u_wave_band;       // deprecated compatibility uniform; prefer u_ir_band_index/class
    uniform int u_ir_band_index;   // 0 VIS, 1 NIR, 2 SWIR, 3 MWIR, 4 LWIR
    uniform int u_ir_band_class;   // 0 reflective, 1 mixed, 2 thermal
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
    uniform sampler2D p3d_Texture1;
    uniform int u_material_id_ready;
    uniform int u_debug_material_id;
    uniform int u_material_param_count;
    uniform float u_material_ids[8];
    uniform vec4 u_material_params[8];
    uniform int u_stage4_visual_debug; // 阶段4可视化诊断：默认0，只在排查Hotspot/BrightSpot接线时打开
    uniform int u_stage5_radiance_debug_en; // Stage5A minimal radiance debug switch, default 0
    uniform int u_stage5_debug_view_mode;   // 0 Composite, 1 BodyOnly, 2 HotspotOnly, 3 BrightSpotOnly
    uniform int u_stage5_use_base_texture_modulation;
    uniform float u_material_temp_K;
    uniform float u_material_emissivity;
    uniform float u_body_radiance_scale;
    uniform float u_stage5_body_gray;
    uniform float u_stage5_reflected_radiance;
    uniform float u_stage5_reflected_gray;
    uniform float u_stage5_reflectance_band;
    uniform float u_stage5_solar_weight;
    uniform vec3 u_stage5_sun_dir_local;
    uniform float u_stage5_hotspot_gray;
    uniform float u_stage5_brightspot_gray;
    uniform float u_stage5_final_gray_debug;
    uniform float u_stage5_body_display_gray;
    uniform float u_stage5_reflected_display_gray;
    uniform float u_stage5_hotspot_display_gray;
    uniform float u_stage5_brightspot_display_gray;
    uniform float u_stage5_composite_min_gray;
    uniform float u_stage5_composite_max_gray;
    uniform int u_stage5_display_fallback_applied;

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
    uniform float u_brightspot_temp;   // legacy命名：阶段4按亮斑intensity使用，不代表Kelvin温度
    // ======================================================

    varying vec2 texcoord;
    varying vec3 v_local_pos;
    varying vec3 v_stage5_normal;

    void main() {
        vec4 texColor = texture2D(p3d_Texture0, texcoord);

        if (u_object_kind == 1 || u_is_background == 1) {
            float sky_tint = (u_ir_band_class == 0) ? 0.18 : 0.055;
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

        vec4 surface_param = vec4(u_emissivity, u_reflectance, 0.0, 0.5);
        float material_id = texture2D(p3d_Texture1, texcoord).r;
        if (u_material_id_ready == 1) {
            if (u_debug_material_id == 1) {
                gl_FragColor = vec4(material_id, material_id, material_id, texColor.a);
                return;
            }
            for (int i = 0; i < 8; ++i) {
                if (i < u_material_param_count) {
                    float hit = 1.0 - step(0.006, abs(material_id - u_material_ids[i]));
                    surface_param = mix(surface_param, u_material_params[i], hit);
                }
            }
        }

        float surface_emissivity = clamp(surface_param.x, 0.01, 1.0);
        float surface_reflectance = clamp(surface_param.y, 0.02, 0.95);
		// 计算基础热辐射与范围热源
        float current_temp = u_base_temperature;
        float stage4_debug_mask = 0.0;
        float stage5_rear_mask = 0.0;
        float stage5_bright_mask = 0.0;

        if (u_hotspot_front_en == 1) {
            float dist_front = distance(v_local_pos, u_hotspot_front_pos);
            float factor_front = 1.0 - smoothstep(0.0, u_hotspot_front_radius, dist_front);
            current_temp += factor_front * u_hotspot_front_temp;
        }

        if (u_hotspot_rear_en == 1) {
            float dist_rear = distance(v_local_pos, u_hotspot_rear_pos);
            float flicker = 0.85 + 0.15 * sin(u_time * 15.0);
            float factor_rear = 1.0 - smoothstep(0.0, u_hotspot_rear_radius, dist_rear);
            stage4_debug_mask = max(stage4_debug_mask, factor_rear);
            stage5_rear_mask = max(stage5_rear_mask, factor_rear);
            current_temp += factor_rear * (u_hotspot_rear_temp * flicker);
        }

        float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));

        // Band class semantics: reflective VIS/NIR/SWIR keep texture detail, MWIR is mixed, LWIR is thermal dominant.
        float detail_weight = (u_ir_band_class == 0) ? 0.7 : ((u_ir_band_class == 1) ? 0.2 : 0.05);
        float temp_weight   = (u_ir_band_class == 0) ? 0.3 : 1.0;

        float final_intensity = current_temp * temp_weight * (0.75 + 0.25 * surface_emissivity)
                              + luminance * detail_weight * (0.70 + 0.30 * surface_reflectance);
        float chain_intensity = u_ir_radiance * surface_emissivity * u_display_gain + u_path_radiance + u_display_offset;
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
            stage4_debug_mask = max(stage4_debug_mask, factor_bs);
            stage5_bright_mask = max(stage5_bright_mask, factor_bs);
            brightspot_intensity = factor_bs * u_brightspot_temp;
        }

        if (u_stage5_radiance_debug_en == 1) {
            // Stage5 debug uses normalized body/reflection/hotspot/brightspot components only.
            // Stage5B reflection is a direct empirical term; it intentionally does not use MODTRAN solar irradiance tables.
            float body_debug = clamp(u_stage5_body_display_gray, 0.0, 1.0);
            if (u_stage5_use_base_texture_modulation == 1) {
                body_debug *= clamp(0.65 + 0.35 * luminance, 0.0, 1.0);
            }
            vec3 stage5_normal_raw = v_stage5_normal;
            vec3 stage5_normal = (dot(stage5_normal_raw, stage5_normal_raw) < 0.001)
                ? vec3(0.0, 0.0, 1.0)
                : normalize(stage5_normal_raw);
            vec3 stage5_sun_raw = u_stage5_sun_dir_local;
            vec3 stage5_sun_dir = (dot(stage5_sun_raw, stage5_sun_raw) < 0.001)
                ? vec3(0.0, 0.0, 1.0)
                : normalize(stage5_sun_raw);
            float ndotl_shader = clamp(dot(stage5_normal, stage5_sun_dir), 0.0, 1.0);
            float texture_luma_shader = clamp(luminance, 0.0, 1.0);
            float reflected_debug = clamp(u_stage5_reflected_display_gray, 0.0, 1.0) * ndotl_shader * texture_luma_shader;
            float rear_hotspot_debug = stage5_rear_mask * clamp(u_stage5_hotspot_display_gray, 0.0, 1.0);
            float brightspot_debug = stage5_bright_mask * clamp(u_stage5_brightspot_display_gray, 0.0, 1.0);
            float stage5_intensity = 0.0;
            if (u_stage5_debug_view_mode == 1) {
                stage5_intensity = body_debug;
            } else if (u_stage5_debug_view_mode == 2) {
                stage5_intensity = rear_hotspot_debug;
            } else if (u_stage5_debug_view_mode == 3) {
                stage5_intensity = brightspot_debug;
            } else {
                float composite_min = clamp(u_stage5_composite_min_gray, 0.0, 1.0);
                float composite_max = max(composite_min, clamp(u_stage5_composite_max_gray, 0.0, 1.0));
                stage5_intensity = clamp(body_debug + reflected_debug + rear_hotspot_debug + brightspot_debug, composite_min, composite_max);
            }
            if (u_stage4_visual_debug == 1 && stage4_debug_mask > 0.0) {
                stage5_intensity = max(stage5_intensity, clamp(0.25 + stage4_debug_mask * 0.75, 0.0, 1.0));
            }
            gl_FragColor = vec4(stage5_intensity, stage5_intensity, stage5_intensity, texColor.a);
            return;
        }

        // 合成（限制在纯白 1.0 以内）
        final_intensity = clamp(final_intensity + brightspot_intensity, 0.0, 1.0);
        if (u_stage4_visual_debug == 1 && stage4_debug_mask > 0.0) {
            // 只在显式打开诊断开关时抬亮mask区域，用于确认uniform已经进入可见像素链路。
            final_intensity = max(final_intensity, clamp(0.25 + stage4_debug_mask * 0.75, 0.0, 1.0));
        }
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
	node.set_shader_input("u_wave_band", LVecBase2i(3, 0));       // deprecated compatibility: internal IRBand index
	node.set_shader_input("u_ir_band_index", LVecBase2i(3, 0));   // 默认 MWIR
	node.set_shader_input("u_ir_band_class", LVecBase2i(1, 0));   // 默认 mixed
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

	// 阶段2材质映射默认值：没有材质ID纹理时仍按整目标默认材质稳定渲染。
	PTA_float defaultMaterialIds;
	PTA_LVecBase4f defaultMaterialParams;
	for (int i = 0; i < 8; ++i)
	{
		defaultMaterialIds.push_back(0.0f);
		defaultMaterialParams.push_back(LVecBase4f(0.85f, 0.15f, 0.40f, 0.50f));
	}
	node.set_shader_input("u_material_id_ready", LVecBase2i(0, 0));
	node.set_shader_input("u_debug_material_id", LVecBase2i(0, 0));
	node.set_shader_input("u_material_param_count", LVecBase2i(0, 0));
	node.set_shader_input("u_material_ids", defaultMaterialIds);
	node.set_shader_input("u_material_params", defaultMaterialParams);
	node.set_shader_input("u_stage4_visual_debug", LVecBase2i(0, 0)); // 默认关闭阶段4可视化诊断
	node.set_shader_input("u_stage5_radiance_debug_en", LVecBase2i(0, 0)); // 默认关闭Stage5A最小辐射debug
	node.set_shader_input("u_stage5_debug_view_mode", LVecBase2i(0, 0));
	node.set_shader_input("u_stage5_use_base_texture_modulation", LVecBase2i(0, 0));
	node.set_shader_input("u_material_temp_K", LVecBase2f(300.0f, 0.0f));
	node.set_shader_input("u_material_emissivity", LVecBase2f(0.85f, 0.0f));
	node.set_shader_input("u_body_radiance_scale", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_body_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflected_radiance", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflected_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflectance_band", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_solar_weight", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_sun_dir_local", LVecBase3f(0.0f, 0.0f, 1.0f));
	node.set_shader_input("u_stage5_hotspot_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_brightspot_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_final_gray_debug", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_body_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflected_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_hotspot_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_brightspot_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_composite_min_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_composite_max_gray", LVecBase2f(1.0f, 0.0f));
	node.set_shader_input("u_stage5_display_fallback_applied", LVecBase2i(0, 0));

	// 头部亮斑默认配置
	node.set_shader_input("u_hotspot_front_en", LVecBase2i(0, 0));
	node.set_shader_input("u_hotspot_front_pos", LVecBase3f(-0.3f, 0.5f, 0.0f));
	node.set_shader_input("u_hotspot_front_radius", LVecBase2f(0.5f, 0.0f));
	node.set_shader_input("u_hotspot_front_temp", LVecBase2f(1.0f, 0.0f));

	// 阶段4 ThermalHotspot 默认关闭；运行时仅由 TargetState.engineState 控制发动机/尾喷热源。
	node.set_shader_input("u_hotspot_rear_en", LVecBase2i(0, 0));
	node.set_shader_input("u_hotspot_rear_pos", LVecBase3f(0.0f, 0.0f, 0.0f));
	node.set_shader_input("u_hotspot_rear_radius", LVecBase2f(2.0f, 0.0f));
	node.set_shader_input("u_hotspot_rear_temp", LVecBase2f(1.2f, 0.0f));

	// 阶段4 BrightSpot 默认关闭；运行时仅由 WeaponState.strikeFlag/strikePart 控制。
	node.set_shader_input("u_brightspot_en", LVecBase2i(0, 0)); // 默认关闭
	// 初始位置
	node.set_shader_input("u_brightspot_pos", LVecBase3f(0.0f, 0.0f, 2.0f));
	node.set_shader_input("u_brightspot_radius", LVecBase2f(1.0f, 0.0f)); // 亮斑大小
	node.set_shader_input("u_brightspot_temp", LVecBase2f(0.0f, 0.0f));   // legacy命名：传入intensity，不是Kelvin温度
}

void HwaSimIR::ApplyRadianceInputs(NodePath& node, const IRObjectRadianceOutput& radiance, int objectKind)
{
	if (node.is_empty())
	{
		return;
	}

	node.set_shader_input("u_object_kind", LVecBase2i(objectKind, 0));
	const IRBand shaderBand = static_cast<IRBand>(static_cast<int>(radiance.bandIndex));
	node.set_shader_input("u_wave_band", LVecBase2i(static_cast<int>(radiance.bandIndex), 0)); // deprecated compatibility
	node.set_shader_input("u_ir_band_index", LVecBase2i(static_cast<int>(shaderBand), 0));
	node.set_shader_input("u_ir_band_class", LVecBase2i(IRBandClassForShader(shaderBand), 0));
	node.set_shader_input("u_ir_radiance", LVecBase2f(radiance.baseRadiance, 0.0f));
	node.set_shader_input("u_emissivity", LVecBase2f(radiance.emissivity, 0.0f));
	node.set_shader_input("u_reflectance", LVecBase2f(radiance.reflectance, 0.0f));
	node.set_shader_input("u_tau_up", LVecBase2f(radiance.tauUp, 0.0f));
	node.set_shader_input("u_path_radiance", LVecBase2f(radiance.pathRadiance, 0.0f));
	node.set_shader_input("u_sky_radiance", LVecBase2f(radiance.skyRadiance, 0.0f));
	node.set_shader_input("u_display_gain", LVecBase2f(radiance.displayGain, 0.0f));
	node.set_shader_input("u_display_offset", LVecBase2f(radiance.displayOffset, 0.0f));
	node.set_shader_input("u_base_temperature", LVecBase2f(radiance.baseRadiance, 0.0f));
	const int stage5BandIndex = Stage5BandIndex(shaderBand);
	node.set_shader_input("u_stage5_radiance_debug_en", LVecBase2i(m_enableStage5RadianceDebug ? 1 : 0, 0));
	node.set_shader_input("u_stage5_debug_view_mode", LVecBase2i(m_stage5DebugViewMode, 0));
	node.set_shader_input("u_stage5_use_base_texture_modulation", LVecBase2i(m_stage5UseBaseTextureModulationByBand[stage5BandIndex] ? 1 : 0, 0));
	node.set_shader_input("u_material_temp_K", LVecBase2f(radiance.temperatureK, 0.0f));
	node.set_shader_input("u_material_emissivity", LVecBase2f(radiance.emissivity, 0.0f));
	node.set_shader_input("u_body_radiance_scale", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_body_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflected_radiance", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflected_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflectance_band", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_solar_weight", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_sun_dir_local", LVecBase3f(0.0f, 0.0f, 1.0f));
	node.set_shader_input("u_stage5_hotspot_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_brightspot_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_final_gray_debug", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_body_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflected_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_hotspot_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_brightspot_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_composite_min_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_composite_max_gray", LVecBase2f(1.0f, 0.0f));
	node.set_shader_input("u_stage5_display_fallback_applied", LVecBase2i(0, 0));
}

IRObjectRadianceOutput HwaSimIR::EvaluateNodeRadiance(const std::string& materialName, const NodePath& node, bool engineOn, bool damaged, bool isSky, bool isCloud, double cloudDensity, double targetAltitudeMeters)
{
	IRObjectRadianceInput input;
	input.materialName = materialName;
	input.rangeMeters = isSky ? 5000.0 : static_cast<double>(EstimateRangeToCamera(node));
	input.engineOn = engineOn;
	input.damaged = damaged;
	input.isSky = isSky;
	input.isCloud = isCloud;
	input.cloudDensity = cloudDensity;

	if (m_stage0DisplayFrameCount > 0 && IsReasonableAltitudeMeters(m_realTimeSceneData.platLoc.alt))
	{
		input.observerAltitudeMeters = m_realTimeSceneData.platLoc.alt;
		input.hasObserverAltitude = true;
	}
	else if (m_isAddPlatform && IsReasonableAltitudeMeters(m_initSceneData.platParam[0].spatial.alt))
	{
		input.observerAltitudeMeters = m_initSceneData.platParam[0].spatial.alt;
		input.hasObserverAltitude = true;
	}

	if (IsReasonableAltitudeMeters(targetAltitudeMeters))
	{
		input.targetAltitudeMeters = targetAltitudeMeters;
		input.hasTargetAltitude = true;
	}
	else if (!isSky && !node.is_empty() && !m_renderRoot.is_empty() && input.hasObserverAltitude)
	{
		LPoint3f nodePos = node.get_pos(m_renderRoot);
		double estimatedTargetAltitude = input.observerAltitudeMeters + static_cast<double>(nodePos.get_z());
		if (IsReasonableAltitudeMeters(estimatedTargetAltitude))
		{
			input.targetAltitudeMeters = estimatedTargetAltitude;
			input.hasTargetAltitude = true;
		}
	}

	return m_irRadianceModel.evaluate(input);
}

std::string HwaSimIR::MaterialNameForPlatform(PLATFORM_TYPE type) const
{
	// CPU 辐亮度模型当前仍按平台默认材质计算；像素级差异由阶段2 shader 材质ID纹理补充。
	std::map<PLATFORM_TYPE, PlatformResPath>::const_iterator resIter = m_platformResMap.find(type);
	if (resIter != m_platformResMap.end() && !resIter->second.defaultMaterialName.empty())
	{
		return resIter->second.defaultMaterialName;
	}

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

bool HwaSimIR::IsValidTargetStateKey(const BYHWICD::TargetState& targetState) const
{
	return TargetTypeToPlatformType(targetState.targetType) != NONE;
}

bool HwaSimIR::TargetKeyMatches(const BYHWICD::TargetState& targetState, const TargetPlatformData& targetPlat) const
{
	if (!targetPlat.isExist || !IsValidTargetStateKey(targetState))
	{
		return false;
	}
	// TargetState 的唯一目标键为 targetType + targetPlatID + targetID，不能只看 targetID。
	return targetPlat.type == TargetTypeToPlatformType(targetState.targetType)
		&& targetPlat.targetState.targetType == targetState.targetType
		&& targetPlat.targetState.targetPlatID == targetState.targetPlatID
		&& targetPlat.targetState.targetID == targetState.targetID;
}

bool HwaSimIR::WeaponTargetKeyMatches(const BYHWICD::WeaponState& weaponState, const TargetPlatformData& targetPlat) const
{
	if (!targetPlat.isExist || weaponState.targetType == 0x00)
	{
		return false;
	}
	// WeaponState 同样使用三元组定位目标；strike/lookat/viewValid 都不能只按 targetID 匹配。
	return targetPlat.type == TargetTypeToPlatformType(weaponState.targetType)
		&& targetPlat.targetState.targetType == weaponState.targetType
		&& targetPlat.targetState.targetPlatID == weaponState.targetPlatID
		&& targetPlat.targetState.targetID == weaponState.targetID;
}

TargetPlatformData* HwaSimIR::FindTargetPlatformByTargetState(const BYHWICD::TargetState& targetState)
{
	for (auto& targetPlat : m_targetPlatformList)
	{
		if (TargetKeyMatches(targetState, targetPlat))
		{
			return &targetPlat;
		}
	}
	return nullptr;
}

TargetPlatformData* HwaSimIR::FindTargetPlatformByWeaponState(const BYHWICD::WeaponState& weaponState)
{
	for (auto& targetPlat : m_targetPlatformList)
	{
		if (WeaponTargetKeyMatches(weaponState, targetPlat))
		{
			return &targetPlat;
		}
	}
	return nullptr;
}

TargetPlatformData* HwaSimIR::FindOrMapTargetPlatform(const BYHWICD::TargetState& targetState, int targetStateIndex)
{
	TargetPlatformData* mappedTarget = FindTargetPlatformByTargetState(targetState);
	if (mappedTarget != nullptr)
	{
		return mappedTarget;
	}

	const PLATFORM_TYPE platType = TargetTypeToPlatformType(targetState.targetType);
	for (size_t platIdx = 0; platIdx < m_targetPlatformList.size(); ++platIdx)
	{
		TargetPlatformData& targetPlat = m_targetPlatformList[platIdx];
		const bool unmapped = targetPlat.targetState.targetID < 0;
		if (targetPlat.isExist && targetPlat.type == platType && unmapped)
		{
			targetPlat.platID = targetState.targetID;
			targetPlat.targetState.targetType = targetState.targetType;
			targetPlat.targetState.targetPlatID = targetState.targetPlatID;
			targetPlat.targetState.targetID = targetState.targetID;
			targetPlat.nodePath.hide();
			std::cout << "[TargetMapping] bind"
				<< " index=" << targetStateIndex
				<< " platformIndex=" << platIdx
				<< " targetType=0x" << std::hex << targetState.targetType << std::dec
				<< " targetPlatID=" << targetState.targetPlatID
				<< " targetID=" << targetState.targetID
				<< std::endl;
			return &targetPlat;
		}
	}

	if (m_stage0DisplayFrameCount <= 3 || (m_stage0DisplayFrameCount % 60) == 0)
	{
		std::cout << "[TargetMapping][WARN] no_free_target_platform"
			<< " index=" << targetStateIndex
			<< " targetType=0x" << std::hex << targetState.targetType << std::dec
			<< " targetPlatID=" << targetState.targetPlatID
			<< " targetID=" << targetState.targetID
			<< std::endl;
	}
	return nullptr;
}

void HwaSimIR::ApplyWeaponCameraControl(BYHWICD::DisplayC2cObjTrackingData& currentData, TargetPlatformData* lookAtTarget)
{
	if (m_cameraNode.is_empty())
	{
		return;
	}

	BYHWICD::WeaponState& weaponState = currentData.weaponState;
	if (weaponState.lookatEn)
	{
		if (lookAtTarget == nullptr)
		{
			std::cout << "[CameraControl][WARN] lookat_target_not_found"
				<< " targetType=0x" << std::hex << weaponState.targetType << std::dec
				<< " targetPlatID=" << weaponState.targetPlatID
				<< " targetID=" << weaponState.targetID
				<< std::endl;
			return;
		}

		const BYHWICD::SpatialState& platSpatial = currentData.platLoc;
		const BYHWICD::SpatialState& targetSpatial = lookAtTarget->targetState.targetLoc;
		double range = 0.0;
		double relPitch = 0.0;
		double relYaw = 0.0;
		m_attitudeTrans.computeRelativePosition(
			platSpatial.lat, platSpatial.lon, platSpatial.alt,
			platSpatial.roll, platSpatial.pitch, platSpatial.yaw,
			targetSpatial.lat, targetSpatial.lon, targetSpatial.alt,
			range, relPitch, relYaw);

		m_cameraNode.set_hpr(-relYaw, relPitch, 0.0);
		weaponState.offsetAng[0] = relPitch;
		weaponState.offsetAng[1] = relYaw;
		if (m_stage0DisplayFrameCount <= 3 || (m_stage0DisplayFrameCount % 60) == 0)
		{
			std::cout << "[CameraControl]"
				<< " mode=lookat"
				<< " targetPlatID=" << weaponState.targetPlatID
				<< " targetID=" << weaponState.targetID
				<< " relPitch=" << relPitch
				<< " relYaw=" << relYaw
				<< " range=" << range
				<< std::endl;
		}
		return;
	}

	// 手动角模式：xxOutAng[0] 是方位角，xxOutAng[1] 是俯仰角；同步写入 offsetAng[pitch,yaw]。
	const double yaw = weaponState.xxOutAng[0];
	const double pitch = weaponState.xxOutAng[1];
	weaponState.offsetAng[0] = pitch;
	weaponState.offsetAng[1] = yaw;
	m_cameraNode.set_hpr(-yaw, pitch, 0.0);
	if (m_stage0DisplayFrameCount <= 3 || (m_stage0DisplayFrameCount % 60) == 0)
	{
		std::cout << "[CameraControl]"
			<< " mode=angle"
			<< " xxYaw=" << yaw
			<< " xxPitch=" << pitch
			<< " offsetPitch=" << weaponState.offsetAng[0]
			<< " offsetYaw=" << weaponState.offsetAng[1]
			<< std::endl;
	}
}

std::string HwaSimIR::Stage4PlatformName(PLATFORM_TYPE type) const
{
	std::map<PLATFORM_TYPE, PlatformResPath>::const_iterator resIter = m_platformResMap.find(type);
	if (resIter != m_platformResMap.end() && !resIter->second.displayName.empty())
	{
		return resIter->second.displayName;
	}

	switch (type)
	{
	case F35: return "F35";
	case AIM120: return "AIM120";
	case AIM9: return "AIM9X";
	case MMD: return "MMD";
	default: return "default";
	}
}

bool HwaSimIR::Stage4WeaponAppliesToTarget(const BYHWICD::WeaponState& weaponState, const TargetPlatformData& targetPlat) const
{
	if (!weaponState.strikeFlag)
	{
		return false;
	}
	return WeaponTargetKeyMatches(weaponState, targetPlat);
}

void HwaSimIR::ApplyStage4TargetState(TargetPlatformData& targetPlat, const BYHWICD::WeaponState& weaponState, float dtSec, float ambientTempK, const IRObjectRadianceOutput& radiance)
{
	if (targetPlat.nodePath.is_empty())
	{
		return;
	}

	const std::string platformName = Stage4PlatformName(targetPlat.type);
	const std::string runtimeKey = platformName + "#plat" + std::to_string(targetPlat.targetState.targetPlatID)
		+ "#target" + std::to_string(targetPlat.targetState.targetID);
	const bool engineState = targetPlat.targetState.engineState;
	IRHotspotState rearHotspot = m_irTemperatureModel.updateEngineRear(platformName, runtimeKey, engineState, dtSec, ambientTempK);
	float tempSpan = std::max(1.0f, rearHotspot.targetTempK - rearHotspot.ambientTempK);
	float normalizedTemp = std::max(0.0f, std::min(1.0f, (rearHotspot.currentTempK - rearHotspot.ambientTempK) / tempSpan));
	float rearIntensity = normalizedTemp * rearHotspot.intensity;
	float rearRadius = std::max(rearHotspot.size.x, std::max(rearHotspot.size.y, rearHotspot.size.z));
	bool rearEnabledForShader = rearHotspot.enabled;
	IRStage4Vec3 rearPosForShader = rearHotspot.localPos;
	float rearRadiusForShader = rearRadius;
	float rearIntensityForShader = rearIntensity;

	if (m_enableStage4HotspotVisualDebug && m_forceStage4RearHotspotVisible)
	{
		// 诊断模式：覆盖到大半径、强强度，专门确认尾部热源uniform是否能进入像素输出。
		rearEnabledForShader = true;
		rearPosForShader = IRStage4Vec3(0.0f, 0.0f, 0.0f);
		rearRadiusForShader = std::max(rearRadiusForShader, 1000.0f);
		rearIntensityForShader = std::max(rearIntensityForShader, 1.0f);
	}

	// 阶段4 ThermalHotspot：只由 engineState 控制发动机/尾喷热源，不读取 strikeFlag。
	targetPlat.nodePath.set_shader_input("u_stage4_visual_debug", LVecBase2i(m_enableStage4HotspotVisualDebug ? 1 : 0, 0));
	targetPlat.nodePath.set_shader_input("u_hotspot_rear_en", LVecBase2i(rearEnabledForShader ? 1 : 0, 0));
	targetPlat.nodePath.set_shader_input("u_hotspot_rear_pos", LVecBase3f(rearPosForShader.x, rearPosForShader.y, rearPosForShader.z));
	targetPlat.nodePath.set_shader_input("u_hotspot_rear_radius", LVecBase2f(rearRadiusForShader, 0.0f));
	targetPlat.nodePath.set_shader_input("u_hotspot_rear_temp", LVecBase2f(rearIntensityForShader, 0.0f));
	std::cout << "[Stage4 ThermalHotspot]"
		<< " targetPlatID=" << targetPlat.targetState.targetPlatID
		<< " targetID=" << targetPlat.targetState.targetID
		<< " platform=" << platformName
		<< " engineState=" << (engineState ? "1" : "0")
		<< " currentTempK=" << rearHotspot.currentTempK
		<< " targetTempK=" << rearHotspot.targetTempK
		<< " enabled=" << (rearEnabledForShader ? "1" : "0")
		<< std::endl;

	const bool brightSpotApplies = Stage4WeaponAppliesToTarget(weaponState, targetPlat);
	bool invalidStrikePart = false;
	IRBrightSpotState brightSpot = m_irTemperatureModel.resolveBrightSpot(
		platformName,
		brightSpotApplies,
		weaponState.strikePart,
		&invalidStrikePart);
	if (m_enableStage4HotspotVisualDebug && m_forceStage4BrightSpotVisible)
	{
		// 诊断模式：与协议解耦，强制生成一个覆盖面足够大的亮斑，排查shader/坐标/输出映射。
		brightSpot.enabled = true;
		brightSpot.part = IRBrightSpotPart::Head;
		brightSpot.localPos = IRStage4Vec3(0.0f, 0.0f, 0.0f);
		brightSpot.radius = std::max(brightSpot.radius, 1000.0f);
		brightSpot.intensity = std::max(brightSpot.intensity, 1.0f);
	}

	// 阶段4 BrightSpot：只由 WeaponState.strikeFlag/strikePart 控制；u_brightspot_temp 传 intensity，不是温度。
	targetPlat.nodePath.set_shader_input("u_brightspot_en", LVecBase2i(brightSpot.enabled ? 1 : 0, 0));
	targetPlat.nodePath.set_shader_input("u_brightspot_pos", LVecBase3f(brightSpot.localPos.x, brightSpot.localPos.y, brightSpot.localPos.z));
	targetPlat.nodePath.set_shader_input("u_brightspot_radius", LVecBase2f(brightSpot.radius, 0.0f));
	targetPlat.nodePath.set_shader_input("u_brightspot_temp", LVecBase2f(brightSpot.intensity, 0.0f));
	std::cout << "[Stage4 BrightSpot]"
		<< " targetPlatID=" << targetPlat.targetState.targetPlatID
		<< " targetID=" << targetPlat.targetState.targetID
		<< " strikeFlag=" << (weaponState.strikeFlag ? "1" : "0")
		<< " strikePart=" << weaponState.strikePart
		<< " part=" << IRBrightSpotPartName(brightSpot.part)
		<< " localPos=(" << brightSpot.localPos.x << "," << brightSpot.localPos.y << "," << brightSpot.localPos.z << ")"
		<< " radius=" << brightSpot.radius
		<< " intensity=" << brightSpot.intensity
		<< " enabled=" << (brightSpot.enabled ? "1" : "0")
		<< std::endl;

	ApplyStage5RadianceDebug(targetPlat, radiance, rearHotspot, brightSpot, rearEnabledForShader, rearIntensityForShader, runtimeKey);

	const bool hasShader = (targetPlat.nodePath.get_shader() != nullptr);
	const std::map<PLATFORM_TYPE, PlatformResPath>::const_iterator resIter = m_platformResMap.find(targetPlat.type);
	bool baseTextureAvailable = false;
	bool materialIdTexBound = false;
	if (resIter != m_platformResMap.end())
	{
		baseTextureAvailable = !resIter->second.texturePath.empty() && FileExists(resIter->second.texturePath);
		materialIdTexBound = !resIter->second.materialIdTexturePath.empty() && FileExists(resIter->second.materialIdTexturePath);
	}
	const bool logUniform = rearEnabledForShader || brightSpot.enabled || m_enableStage4HotspotVisualDebug
		|| m_stage0DisplayFrameCount <= 3 || (m_stage0DisplayFrameCount % 60) == 0;
	if (logUniform)
	{
		std::cout << "[Stage4 Uniform]"
			<< " targetPlatID=" << targetPlat.targetState.targetPlatID
			<< " targetID=" << targetPlat.targetState.targetID
			<< " nodeName=" << targetPlat.nodePath.get_name()
			<< " hasShader=" << (hasShader ? "1" : "0")
			<< " baseTextureBound=" << (baseTextureAvailable ? "1" : "0")
			<< " materialIdTexBound=" << (materialIdTexBound ? "1" : "0")
			<< " hotspotRearEn=" << (rearEnabledForShader ? "1" : "0")
			<< " brightspotEn=" << (brightSpot.enabled ? "1" : "0")
			<< " brightspotPos=(" << brightSpot.localPos.x << "," << brightSpot.localPos.y << "," << brightSpot.localPos.z << ")"
			<< " brightspotRadius=" << brightSpot.radius
			<< " brightspotIntensity=" << brightSpot.intensity
			<< std::endl;
		if (!hasShader)
		{
			std::cout << "[Stage4 Uniform][WARN] STAGE4_SHADER_NOT_BOUND"
				<< " targetPlatID=" << targetPlat.targetState.targetPlatID
				<< " targetID=" << targetPlat.targetState.targetID
				<< " nodeName=" << targetPlat.nodePath.get_name()
				<< std::endl;
		}
		if (!baseTextureAvailable || !materialIdTexBound)
		{
			std::cout << "[Stage4 Uniform][WARN] STAGE4_TEXTURE_MISSING"
				<< " targetPlatID=" << targetPlat.targetState.targetPlatID
				<< " targetID=" << targetPlat.targetState.targetID
				<< " baseTextureBound=" << (baseTextureAvailable ? "1" : "0")
				<< " materialIdTexBound=" << (materialIdTexBound ? "1" : "0")
				<< std::endl;
		}
	}
	if (m_enableStage4HotspotVisualDebug && logUniform)
	{
		std::cout << "[Stage4 VisualDebug]"
			<< " targetPlatID=" << targetPlat.targetState.targetPlatID
			<< " targetID=" << targetPlat.targetState.targetID
			<< " enabled=1"
			<< " forceRear=" << (m_forceStage4RearHotspotVisible ? "1" : "0")
			<< " forceBright=" << (m_forceStage4BrightSpotVisible ? "1" : "0")
			<< " rearRadius=" << rearRadiusForShader
			<< " rearIntensity=" << rearIntensityForShader
			<< " brightRadius=" << brightSpot.radius
			<< " brightIntensity=" << brightSpot.intensity
			<< std::endl;
	}
}

void HwaSimIR::ApplyStage5RadianceDebug(TargetPlatformData& targetPlat, const IRObjectRadianceOutput& radiance, const IRHotspotState& rearHotspot, const IRBrightSpotState& brightSpot, bool rearEnabledForShader, float rearIntensityForShader, const std::string& targetKey)
{
	if (targetPlat.nodePath.is_empty())
	{
		return;
	}

	const IRBand stage5Band = static_cast<IRBand>(static_cast<int>(radiance.bandIndex));
	const int stage5BandIndex = Stage5BandIndex(stage5Band);
	const bool stage5UseBaseTextureModulation = m_stage5UseBaseTextureModulationByBand[stage5BandIndex];
	targetPlat.nodePath.set_shader_input("u_stage5_radiance_debug_en", LVecBase2i(m_enableStage5RadianceDebug ? 1 : 0, 0));
	targetPlat.nodePath.set_shader_input("u_stage5_debug_view_mode", LVecBase2i(m_stage5DebugViewMode, 0));
	targetPlat.nodePath.set_shader_input("u_stage5_use_base_texture_modulation", LVecBase2i(stage5UseBaseTextureModulation ? 1 : 0, 0));
	if (!m_enableStage5RadianceDebug)
	{
		return;
	}

	IRRadianceModelV2Input stage5Input;
	stage5Input.band = stage5Band;
	stage5Input.materialTemperatureK = radiance.temperatureK;
	stage5Input.materialEmissivity = radiance.emissivity;
	stage5Input.materialReflectance = radiance.reflectance;
	stage5Input.tauUp = radiance.tauUp;
	const IRRuntimeEnvironment environment = m_irRadianceModel.environment();
	stage5Input.solarStrength = environment.sunStrength;
	stage5Input.ndotl = 1.0;
	stage5Input.textureLuma = 1.0;
	stage5Input.solarReflectanceWeight = m_stage5DebugConfigs[stage5BandIndex].solarReflectanceWeight;
	stage5Input.hotspotTemperatureK = rearEnabledForShader ? rearHotspot.currentTempK : radiance.temperatureK;
	stage5Input.hotspotIntensity = rearEnabledForShader ? std::max(0.0f, rearHotspot.intensity) : 0.0f;
	stage5Input.brightspotIntensity = brightSpot.enabled ? std::max(0.0f, brightSpot.intensity) : 0.0f;
	stage5Input.enableDebugFloor = true;
	stage5Input.debugConfig = m_stage5DebugConfigs[stage5BandIndex];

	IRRadianceModelV2Output stage5 = m_irRadianceModelV2.evaluate(stage5Input);
	const IRRadianceModelV2DebugConfig& displayConfig = m_stage5DebugConfigs[stage5BandIndex];
	const auto clamp01 = [](double value) -> double {
		return std::max(0.0, std::min(1.0, value));
	};
	double compositeMinGray = clamp01(displayConfig.compositeMinGray);
	double compositeMaxGray = clamp01(displayConfig.compositeMaxGray);
	if (compositeMaxGray < compositeMinGray)
	{
		compositeMaxGray = compositeMinGray;
	}
	const double bodyGrayDisplay = clamp01(stage5.bodyGrayAfterFloor * std::max(0.0, displayConfig.bodyDisplayGain));
	const double reflectedGrayDisplay = clamp01(stage5.reflectedGray * std::max(0.0, displayConfig.reflectedDisplayGain));
	const double hotspotGrayRawDisplay = clamp01(stage5.hotspotGray * std::max(0.0, displayConfig.hotspotDisplayGain));
	const double brightspotGrayRawDisplay = clamp01(stage5.brightspotGray * std::max(0.0, displayConfig.brightspotDisplayGain));
	const double legacyRearHotspotDisplay = rearEnabledForShader
		? clamp01(std::max(0.0f, rearIntensityForShader) * std::max(0.0, displayConfig.hotspotDisplayGain))
		: 0.0;
	const double legacyBrightspotDisplay = brightSpot.enabled
		? clamp01(std::max(0.0f, brightSpot.intensity) * std::max(0.0, displayConfig.brightspotDisplayGain))
		: 0.0;
	const double hotspotGrayDisplay = std::max(hotspotGrayRawDisplay, legacyRearHotspotDisplay);
	const double brightspotGrayDisplay = std::max(brightspotGrayRawDisplay, legacyBrightspotDisplay);
	const bool stage5DisplayFallbackApplied =
		(hotspotGrayDisplay > hotspotGrayRawDisplay + 1.0e-6) ||
		(brightspotGrayDisplay > brightspotGrayRawDisplay + 1.0e-6);
	const double finalGrayDebugDisplay = clamp01(
		std::max(compositeMinGray, std::min(compositeMaxGray,
			bodyGrayDisplay + reflectedGrayDisplay + hotspotGrayDisplay + brightspotGrayDisplay)));

	targetPlat.nodePath.set_shader_input("u_material_temp_K", LVecBase2f(static_cast<float>(stage5Input.materialTemperatureK), 0.0f));
	targetPlat.nodePath.set_shader_input("u_material_emissivity", LVecBase2f(static_cast<float>(stage5Input.materialEmissivity), 0.0f));
	targetPlat.nodePath.set_shader_input("u_body_radiance_scale", LVecBase2f(static_cast<float>(stage5.bodyGrayBeforeFloor), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_body_gray", LVecBase2f(static_cast<float>(stage5.bodyGrayAfterFloor), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_reflected_radiance", LVecBase2f(static_cast<float>(stage5.reflectedRadiance), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_reflected_gray", LVecBase2f(static_cast<float>(stage5.reflectedGray), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_reflectance_band", LVecBase2f(static_cast<float>(stage5Input.materialReflectance), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_solar_weight", LVecBase2f(static_cast<float>(stage5Input.solarReflectanceWeight), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_sun_dir_local", Stage5SunDirectionLocal(environment.sunAzimuthDeg, environment.sunElevationDeg));
	targetPlat.nodePath.set_shader_input("u_stage5_hotspot_gray", LVecBase2f(static_cast<float>(stage5.hotspotGray), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_brightspot_gray", LVecBase2f(static_cast<float>(stage5.brightspotGray), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_final_gray_debug", LVecBase2f(static_cast<float>(stage5.finalGrayDebug), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_body_display_gray", LVecBase2f(static_cast<float>(bodyGrayDisplay), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_reflected_display_gray", LVecBase2f(static_cast<float>(reflectedGrayDisplay), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_hotspot_display_gray", LVecBase2f(static_cast<float>(hotspotGrayDisplay), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_brightspot_display_gray", LVecBase2f(static_cast<float>(brightspotGrayDisplay), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_composite_min_gray", LVecBase2f(static_cast<float>(compositeMinGray), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_composite_max_gray", LVecBase2f(static_cast<float>(compositeMaxGray), 0.0f));
	targetPlat.nodePath.set_shader_input("u_stage5_display_fallback_applied", LVecBase2i(stage5DisplayFallbackApplied ? 1 : 0, 0));

	const std::map<PLATFORM_TYPE, PlatformResPath>::const_iterator resIter = m_platformResMap.find(targetPlat.type);
	bool baseTextureAvailable = false;
	if (resIter != m_platformResMap.end())
	{
		baseTextureAvailable = !resIter->second.texturePath.empty() && FileExists(resIter->second.texturePath);
	}

	std::cout << "[Stage5 Radiance]"
		<< " targetKey=" << targetKey
		<< " band=" << IRBandName(stage5Input.band)
		<< " debugViewMode=" << m_stage5DebugViewModeName
		<< " toneMap=" << Stage5ToneMapName(stage5Input.debugConfig.toneMap)
		<< " useBaseTextureModulation=" << (stage5UseBaseTextureModulation ? "1" : "0")
		<< " materialTempK=" << stage5Input.materialTemperatureK
		<< " emissivity=" << stage5Input.materialEmissivity
		<< " tauUp=" << stage5Input.tauUp
		<< " sunElevation=" << environment.sunElevationDeg
		<< " sunAzimuth=" << environment.sunAzimuthDeg
		<< " ndotl=" << stage5Input.ndotl
		<< " textureLuma=" << stage5Input.textureLuma
		<< " reflectanceBand=" << stage5Input.materialReflectance
		<< " solarWeight=" << stage5Input.solarReflectanceWeight
		<< " bodyRadiance=" << stage5.bodyRadiance
		<< " reflectedRadiance=" << stage5.reflectedRadiance
		<< " hotspotRadiance=" << stage5.hotspotRadiance
		<< " brightspotRadiance=" << stage5.brightspotRadiance
		<< " bodyGrayBeforeFloor=" << stage5.bodyGrayBeforeFloor
		<< " bodyGrayAfterFloor=" << stage5.bodyGrayAfterFloor
		<< " reflectedGray=" << stage5.reflectedGray
		<< " hotspotGray=" << stage5.hotspotGray
		<< " brightspotGray=" << stage5.brightspotGray
		<< " finalGrayDebug=" << stage5.finalGrayDebug
		<< " debugFloorApplied=" << (stage5.debugFloorApplied ? "1" : "0")
		<< " stage5DisplayFallbackApplied=" << (stage5DisplayFallbackApplied ? "1" : "0")
		<< std::endl;

	std::cout << "[Stage5C VisualCalib]"
		<< " band=" << IRBandName(stage5Input.band)
		<< " bodyGray=" << bodyGrayDisplay
		<< " reflectedGray=" << reflectedGrayDisplay
		<< " hotspotGrayRaw=" << stage5.hotspotGray
		<< " hotspotGrayDisplay=" << hotspotGrayDisplay
		<< " brightspotGrayRaw=" << stage5.brightspotGray
		<< " brightspotGrayDisplay=" << brightspotGrayDisplay
		<< " finalGrayDebug=" << finalGrayDebugDisplay
		<< " fallbackApplied=" << (stage5DisplayFallbackApplied ? "1" : "0")
		<< " legacyRearHotspotIntensity=" << legacyRearHotspotDisplay
		<< " legacyBrightspotIntensity=" << legacyBrightspotDisplay
		<< std::endl;

	if (stage5.bodyRadiance > 0.0 && stage5.bodyGrayAfterFloor <= 0.0)
	{
		std::cout << "[Stage5 Radiance][WARN] STAGE5_BODY_GRAY_ZERO_AFTER_MAPPING"
			<< " targetKey=" << targetKey
			<< " band=" << IRBandName(stage5Input.band)
			<< " bodyRadiance=" << stage5.bodyRadiance
			<< " bodyGrayAfterFloor=" << stage5.bodyGrayAfterFloor
			<< std::endl;
	}
	if (!m_stage5BodyGrayPathHintLogged && stage5.bodyGrayAfterFloor > 0.1)
	{
		std::cout << "[Stage5 Radiance][WARN] CHECK_SHADER_BODY_GRAY_PATH_OR_FRAME_OUTPUT"
			<< " targetKey=" << targetKey
			<< " band=" << IRBandName(stage5Input.band)
			<< " bodyGrayAfterFloor=" << stage5.bodyGrayAfterFloor
			<< " debugViewMode=" << m_stage5DebugViewModeName
			<< std::endl;
		m_stage5BodyGrayPathHintLogged = true;
	}
	if (!m_stage5BaseTextureFallbackLogged && stage5Input.solarReflectanceWeight > 0.0 && !baseTextureAvailable)
	{
		std::cout << "[Stage5 Radiance][WARN] STAGE5_BASE_TEXTURE_FALLBACK"
			<< " targetKey=" << targetKey
			<< " band=" << IRBandName(stage5Input.band)
			<< " textureLumaFallback=1"
			<< std::endl;
		m_stage5BaseTextureFallbackLogged = true;
	}
	if (!m_stage5NormalFallbackHintLogged)
	{
		std::cout << "[Stage5 Radiance][WARN] STAGE5_NORMAL_FALLBACK"
			<< " mode=shader_constant_plus_z_if_p3d_Normal_missing"
			<< " targetKey=" << targetKey
			<< std::endl;
		m_stage5NormalFallbackHintLogged = true;
	}
	if ((stage5Input.band == IRBand::Visible || stage5Input.band == IRBand::NearInfrared || stage5Input.band == IRBand::ShortWaveInfrared)
		&& stage5Input.solarReflectanceWeight > 0.0
		&& stage5Input.solarStrength > 0.0
		&& stage5.reflectedGray <= 0.0)
	{
		++m_stage5ConsecutiveReflectedZeroFrames;
		if (m_stage5ConsecutiveReflectedZeroFrames >= 3)
		{
			std::cout << "[Stage5 Radiance][WARN] STAGE5_REFLECTED_GRAY_ZERO"
				<< " targetKey=" << targetKey
				<< " band=" << IRBandName(stage5Input.band)
				<< " solarWeight=" << stage5Input.solarReflectanceWeight
				<< " solarStrength=" << stage5Input.solarStrength
				<< " reflectedRadiance=" << stage5.reflectedRadiance
				<< std::endl;
		}
	}
	else if (stage5.reflectedGray > 0.0)
	{
		m_stage5ConsecutiveReflectedZeroFrames = 0;
	}

	if (stage5.finalGrayDebug <= 0.0)
	{
		++m_stage5ConsecutiveZeroFrames;
		if (m_stage5ConsecutiveZeroFrames >= 3)
		{
			std::cout << "[Stage5 Radiance][WARN] STAGE5_TARGET_BODY_RADIANCE_ZERO"
				<< " targetKey=" << targetKey
				<< " consecutiveFrames=" << m_stage5ConsecutiveZeroFrames
				<< std::endl;
		}
	}
	else
	{
		m_stage5ConsecutiveZeroFrames = 0;
	}
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

double HwaSimIR::CurrentSimulationHour() const
{
	if (m_stage0DisplayFrameCount > 0 && m_realTimeSceneData.time >= 0.0)
	{
		double hour = std::fmod(m_realTimeSceneData.time / 3600000.0, 24.0);
		return hour < 0.0 ? hour + 24.0 : hour;
	}
	// 没有实时帧时间时使用正午，便于在功能测试中获得稳定太阳照射。
	return 12.0;
}

IRRuntimeEnvironment HwaSimIR::BuildRuntimeEnvironment() const
{
	IRRuntimeEnvironment environment = m_irRadianceModel.environment();
	int protocolBand = (m_sensorParam.trackerSensorBand >= 0 && m_sensorParam.trackerSensorBand <= 4)
		? m_sensorParam.trackerSensorBand : 2;
	environment.band = IRBandFromProtocol(protocolBand);
	environment.simulationHour = CurrentSimulationHour();

	if (m_irWeatherReady)
	{
		// 场景 profile 提供地区/日期相关的太阳高度、方位角和逐小时温度。
		IRWeatherSample sample = m_irWeatherProfile.sampleForHour(environment.simulationHour);
		environment.airTemperatureC = sample.airTemperatureC;
		environment.sunAzimuthDeg = sample.sunAzimuthDeg;
		environment.sunElevationDeg = sample.sunElevationDeg;
	}

	if (m_isAddPlatform)
	{
		// 环境优先级：UDP 初始化参数 > 场景 profile > 内置默认值。
		if (m_initSceneData.trackingInit.envTemp > -80.0 && m_initSceneData.trackingInit.envTemp < 80.0)
		{
			environment.airTemperatureC = m_initSceneData.trackingInit.envTemp;
		}
		if (m_initSceneData.trackingInit.envVisibility > 1.0)
		{
			environment.visibilityMeters = m_initSceneData.trackingInit.envVisibility;
		}
		if (m_initSceneData.trackingInit.envHumidity >= 0.0 && m_initSceneData.trackingInit.envHumidity <= 100.0)
		{
			environment.humidityPercent = m_initSceneData.trackingInit.envHumidity;
		}
		if (m_initSceneData.trackingInit.envWindV >= 0.0 && m_initSceneData.trackingInit.envWindV < 120.0)
		{
			environment.windSpeedMps = m_initSceneData.trackingInit.envWindV;
		}
		if (m_initSceneData.trackingInit.envWindDir >= 0.0 && m_initSceneData.trackingInit.envWindDir <= 360.0)
		{
			environment.windDirectionDeg = m_initSceneData.trackingInit.envWindDir;
		}
		environment.weatherCode = m_initSceneData.trackingInit.envSky;
	}

	environment.sunStrength = WeatherSunStrength(environment.weatherCode, environment.sunElevationDeg);
	return environment;
}

void HwaSimIR::LogActiveIREnvironment(const IRRuntimeEnvironment& environment, const char* reason, bool forceLog)
{
	int hourKey = static_cast<int>(std::floor(environment.simulationHour));
	if (!forceLog && hourKey == m_lastLoggedEnvironmentHour && environment.weatherCode == m_lastLoggedEnvironmentWeather)
	{
		return;
	}
	std::cout << "[Stage3] Environment (" << reason << "): hour=" << environment.simulationHour
		<< ", airTempC=" << environment.airTemperatureC
		<< ", visibilityM=" << environment.visibilityMeters
		<< ", humidity=" << environment.humidityPercent
		<< ", wind=" << environment.windSpeedMps << "m/s@" << environment.windDirectionDeg << "deg"
		<< ", weatherCode=" << environment.weatherCode
		<< ", sunAzimuth=" << environment.sunAzimuthDeg
		<< ", sunElevation=" << environment.sunElevationDeg
		<< ", sunStrength=" << environment.sunStrength
		<< ", profile=" << (m_irWeatherReady ? m_irWeatherProfile.loadedPath() : "fallback")
		<< std::endl;
	m_lastLoggedEnvironmentHour = hourKey;
	m_lastLoggedEnvironmentWeather = environment.weatherCode;
}

// 动态更新红外状态
void HwaSimIR::UpdatePlatformIRStatus() {
	double current_time = ClockObject::get_global_clock()->get_frame_time();
	float stage4DtSec = 0.033f;
	if (m_lastStage4UpdateTime >= 0.0)
	{
		stage4DtSec = static_cast<float>(current_time - m_lastStage4UpdateTime);
	}
	m_lastStage4UpdateTime = current_time;

	IRRuntimeEnvironment environment = BuildRuntimeEnvironment();
	const float ambientTempK = static_cast<float>(environment.airTemperatureC + 273.15);
	int protocolBand = (m_sensorParam.trackerSensorBand >= 0 && m_sensorParam.trackerSensorBand <= 4)
		? m_sensorParam.trackerSensorBand : 2;
	LogActiveIRSensorProfile(protocolBand, "runtime-band", false);
	LogActiveIREnvironment(environment, "runtime", false);
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
			IRObjectRadianceOutput radiance = EvaluateNodeRadiance(MaterialNameForPlatform(pakPlat.type), pakPlat.nodePath, true, false, false, false, 0.0, pakPlat.platParam.spatial.alt);
			ApplyRadianceInputs(pakPlat.nodePath, radiance, 0);

			// 阶段4不为挂载平台恢复常开尾喷；飞机平台暂无协议engineState，默认保持关闭。
			pakPlat.nodePath.set_shader_input("u_hotspot_rear_en", LVecBase2i(0, 0));
		}
	}

	// 更新目标导弹平台 (TargetState)
	for (auto& targetPlat : m_targetPlatformList) {
		if (targetPlat.isExist) {
			if (targetPlat.targetState.targetID < 0)
			{
				targetPlat.nodePath.hide();
				continue;
			}
			targetPlat.nodePath.set_shader_input("u_time", LVecBase2f((float)current_time, 0.0f));
			bool damaged = (targetPlat.targetState.targetState == 0x02 || targetPlat.targetState.targetState == 0x03);
			IRObjectRadianceOutput radiance = EvaluateNodeRadiance(MaterialNameForPlatform(targetPlat.type), targetPlat.nodePath, targetPlat.targetState.engineState, damaged, false, false, 0.0, targetPlat.targetState.targetLoc.alt);
			ApplyRadianceInputs(targetPlat.nodePath, radiance, 0);

			IRObjectRadianceOutput stage5BaseRadiance = radiance;
			if (m_enableStage5RadianceDebug)
			{
				// Stage5A body radiance uses the material/environment baseline; engineState stays local to rear ThermalHotspot.
				stage5BaseRadiance = EvaluateNodeRadiance(MaterialNameForPlatform(targetPlat.type), targetPlat.nodePath, false, false, false, false, 0.0, targetPlat.targetState.targetLoc.alt);
			}
			ApplyStage4TargetState(targetPlat, m_realTimeSceneData.weaponState, stage4DtSec, ambientTempK, stage5BaseRadiance);
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
			int frameWidth = width;
			int frameHeight = height;
			const uchar* frameData = ram_image.p();
			std::vector<uchar> sensorSizedFrame;

			if (self->m_sensorDisplayConfigReady &&
				(width != self->m_sensorDisplayConfig.width || height != self->m_sensorDisplayConfig.height)) {
				cv::Mat rawFrame(height, width, CV_8UC3, const_cast<uchar*>(ram_image.p()));
				cv::Mat resizedFrame;
				cv::resize(rawFrame, resizedFrame, cv::Size(self->m_sensorDisplayConfig.width, self->m_sensorDisplayConfig.height));
				frameWidth = self->m_sensorDisplayConfig.width;
				frameHeight = self->m_sensorDisplayConfig.height;
				sensorSizedFrame.resize(static_cast<size_t>(frameWidth) * static_cast<size_t>(frameHeight) * 3u);
				memcpy(sensorSizedFrame.data(), resizedFrame.data, sensorSizedFrame.size());
				frameData = sensorSizedFrame.data();
			}

			// 将纯像素数据推送给 TCP 子线程（避免主线程做过多的耗时运算）
			self->m_pTcpThread->updateFrame(frameData, frameWidth, frameHeight);
			++self->m_stage6CaptureLogCounter;
			if (self->m_stage6CaptureLogCounter <= 3 || (self->m_stage6CaptureLogCounter % 120) == 0) {
				std::ostringstream captureLog;
				/*captureLog << "[Stage6 Capture]"
					<< " frameWidth=" << frameWidth
					<< " frameHeight=" << frameHeight
					<< " tcpWidth=" << frameWidth
					<< " tcpHeight=" << frameHeight
					<< " renderTextureWidth=" << width
					<< " renderTextureHeight=" << height
					<< " channels=RGB8";*/
				//std::cout << captureLog.str() << std::endl;
			}

			if (self->m_stage5OutputFrameDumpEnabled && self->m_enableStage5RadianceDebug && !self->m_stage5OutputFrameDumpPath.empty()) {
				++self->m_stage5OutputFrameCounter;
				if (self->m_stage5OutputFrameCounter % self->m_stage5OutputFrameDumpEvery == 0) {
					try {
						cv::Mat rawFrame(frameHeight, frameWidth, CV_8UC3, const_cast<uchar*>(frameData));
						cv::Mat flippedFrame;
						cv::flip(rawFrame, flippedFrame, 0);
						cv::Mat bgrFrame;
						cv::cvtColor(flippedFrame, bgrFrame, cv::COLOR_RGB2BGR);
						const std::string tempDumpPath = self->m_stage5OutputFrameDumpPath + ".tmp.png";
						const bool tempWriteOk = cv::imwrite(tempDumpPath, bgrFrame);
						bool finalWriteOk = false;
						if (tempWriteOk) {
							std::remove(self->m_stage5OutputFrameDumpPath.c_str());
							finalWriteOk = (std::rename(tempDumpPath.c_str(), self->m_stage5OutputFrameDumpPath.c_str()) == 0);
							if (!finalWriteOk) {
								std::remove(tempDumpPath.c_str());
							}
						}
						if (finalWriteOk) {
							++self->m_stage5OutputFrameDumpWrites;
							if (self->m_stage5OutputFrameDumpWrites <= 3) {
								std::cout << "[Stage5 OutputCapture] renderTextureDump=OK"
									<< " path=" << self->m_stage5OutputFrameDumpPath
									<< " width=" << frameWidth
									<< " height=" << frameHeight
									<< " writes=" << self->m_stage5OutputFrameDumpWrites
									<< std::endl;
							}
						}
						else if (!self->m_stage5OutputFrameDumpFailureLogged) {
							std::cout << "[Stage5 OutputCapture][WARN] renderTextureDump=FAILED"
								<< " reason=" << (tempWriteOk ? "rename_failed" : "imwrite_failed")
								<< " path=" << self->m_stage5OutputFrameDumpPath
								<< " width=" << frameWidth
								<< " height=" << frameHeight
								<< std::endl;
							self->m_stage5OutputFrameDumpFailureLogged = true;
						}
					}
					catch (const cv::Exception& e) {
						if (!self->m_stage5OutputFrameDumpFailureLogged) {
							std::cout << "[Stage5 OutputCapture][WARN] renderTextureDump=FAILED"
								<< " reason=cv_exception"
								<< " message=" << e.what()
								<< " path=" << self->m_stage5OutputFrameDumpPath
								<< std::endl;
							self->m_stage5OutputFrameDumpFailureLogged = true;
						}
					}
				}
			}
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
