#pragma once

#include <cstdint>
#include <string>
#include <map>
#include "nodePath.h"
#include "CommonData.h"

//#define LOAD_TARGET_ID  100        //ROLE 挂载目标ID
//#define MODEL_TARGET_ID 101        //ROLE 目标模型ID

//#define SYMBOL_DEAL_ALLPARAM -6    //标志位 处理所有参数

//#define  LOADTARGET_CPACITY 10     //挂载目标容量

//#define PLAT_SENSORLEFT_ID 0       //左侦察平台（挂载左传感器）占用平台ID
//#define PLAT_SENSORRIGHT_ID 1      //右侦察平台（挂载右传感器）占用平台ID
//#define PLAT_SENSOR_DEFAULTMODEL "/01_boing732/a10s.flt"    //侦察平台（挂载传感器）默认平台

#define VP_HwaInfraredVisualSim_MARINE "ImgSensor_Marine"    //场景文件 海洋
#define VP_HwaInfraredVisualSim_LAND   "ImgSensor_Land"      //场景文件 陆地

//#define VP_SENSORWAVE_NEAR "Config/SensorWave/default_VIS-SWIR.json"  //传感器模型文件  近红外
//#define VP_SENSORWAVE_SHORT "Config/SensorWave/default_SWIR.json"     //传感器模型文件  短波红外
//#define VP_SENSORWAVE_MID "Config/SensorWave/default_MWIR.json"       //传感器模型文件  中波红外
//#define VP_SENSORWAVE_LONG "Config/SensorWave/default_LWIR.json"      //传感器模型文件  长波红外
//#define VP_SENSORWAVE_SHIMMER "Config/SensorWave/default_NVG.json"    //传感器模型文件  微光夜视

#define PLATFORM_MODEL_PATH "Config/TargetLib/"

#ifndef UMETA
#define UMETA(...)
#endif
enum PLATFORM_TYPE
{
	//飞机
	NONE = 0 UMETA(DisplayName = "NONE"),
	F35 = 1 UMETA(DisplayName = "F35"),
	J20 = 5 UMETA(DisplayName = "J20"),

	//导弹
	AIM120 = 2 UMETA(DisplayName = "AIM120"),
	AIM9 = 3 UMETA(DisplayName = "AIM9"),
	MMD = 4 UMETA(DisplayName = "MMD"),
};

// 平台资源路径结构体
struct PlatformResPath
{
	std::string modelPath;             // 模型文件路径，保持相对 Bin 工作目录，便于 Windows/Linux 共用
	std::string texturePath;           // 基础可见光纹理路径，用于 shader 的 p3d_Texture0
	std::string materialIdTexturePath; // 材质 ID 纹理路径，像素值对应材质映射表中的 MaterialId
	std::string materialMapPath;       // 材质映射表路径，当前优先读取 *_mat.tif.xml
	std::string assetDirectory;        // 资产目录，后续用于搜索同目录 mtl/纹理/附加热特征文件
	std::string displayName;           // 日志显示名称
	std::string defaultMaterialName;   // 缺少材质 ID 或映射失败时使用的默认红外材质
};

// 平台核心数据结构体
struct PakPlatformData
{
	PLATFORM_TYPE type;                  // 平台类型
	int platID;                          // 平台ID
	BYHWICD::PlatParamPak platParam;	 // 平台参数
	NodePath nodePath;                   // 节点路径
	bool isExist;                        // 是否存在标记
};
// 平台核心数据结构体
struct WeaponPlatformData
{
	PLATFORM_TYPE type;                  // 平台类型
	int platID;                          // 平台ID
	BYHWICD::WeaponState weaponState;    // 武器状态
	NodePath nodePath;                   // 节点路径
	bool isExist;                        // 是否存在标记
};

// 平台核心数据结构体
struct TargetPlatformData
{
	PLATFORM_TYPE type;                  // 平台类型
	int platID;                          // 平台ID
	BYHWICD::TargetState targetState;	 // 目标状态
	NodePath nodePath;                   // 节点路径
	NodePath enginePlumeCoreNodePath;    // Stage5E EnginePlume core 节点，每个目标最多一个
	NodePath enginePlumeHaloNodePath;    // Stage5E EnginePlume halo 节点，每个目标最多一个
	bool isExist;                        // 是否存在标记
};


// 工作模式枚举
typedef enum _HWASIM_ENUMWORKMODEL
{
	WorkModelActiveControl,   //主动控制工作模式
	WorkModelStaicDriven,     //静态驱动工作模式
	WorkModelJointDriven,     //联合仿真工作模式
} EnumWorkModel;

//目标库类型枚举
typedef enum _HWASIM_ENUMWORKSTATE
{
	EXP_UNREADY = 0,   //初始状态
	EXP_READY,         //就绪状态
	EXP_RUN,           //运行状态
	EXP_PAUSE,         //暂停状态
} EnumWorkState;

const std::map<EnumWorkState, std::string> NameMap_EnumWorkState = {
	{ EXP_UNREADY, "初始状态" },
	{ EXP_READY,   "就绪状态" },
	{ EXP_RUN,     "运行状态" },
	{ EXP_PAUSE,   "暂停状态" },
};

#pragma region 位姿信息
//定义位姿信息
typedef struct _HWASIM_POSITIONINFO
{
	double dLon;                //经度
	double dLat;                //纬度
	double dAlt;                //高度
	double dPitch;              //俯仰
	double dRoll;               //翻滚
	double dHeading;            //偏航
	double dSeneorHeading;      //传感器偏航角
	double dSeneorPitch;        //传感器俯仰角
} PositionInfo;

#pragma endregion

#pragma region 目标
//定义目标库信息
typedef struct _HWASIM_TARGETMODELINFO
{
	int modelId;                //模型Id
	std::string modelName;      //模型名称
	std::string modelTypeName;  //模型类型名称
	std::string modelFilePath;  //模型文件地址
} TargetModelInfo;

typedef struct _HWASIM_TARGETLOADINFO
{
	int loadId;                 //载体Id
	std::string loadName;       //载体名称（替换QString为std::string）
	PositionInfo loadPos;       //载体位姿信息
	TargetModelInfo modelInfo;  //模型信息

	bool IsMaster = false;      //是否是载机（C++11及以上支持就地初始化）
} TargetLoadInfo;

#pragma endregion

#pragma region 环境参数

enum eWeather
{
	eWeather_None = -1,
	eWeather_Sun,    //晴
	eWeather_Rain,   //雨
	eWeather_Snow,   //雪
					 //eWeather_Max
};

struct stWeather
{
	double dRainNum;
	double dSnowNum;
	eWeather WeatherType;

	stWeather()
	{
		dRainNum = 5000.00;
		dSnowNum = 5000.00;
		WeatherType = eWeather_Sun;
	}
};

struct stCloud
{
	double dCloudCoverage;
	double dCloudTopHeight;
	double dCloudBottomHeight;

	stCloud()
	{
		dCloudCoverage = 0.25;
		dCloudTopHeight = 6000.00;
		dCloudBottomHeight = 5000.00;
	}
};

struct stFog
{
	double dFogVisiable;

	stFog()
	{
		dFogVisiable = 15000.00;
	}
};

struct stWind
{
	double dWindSped;
	double dWindDirecX;
	double dWindDirecY;

	stWind()
	{
		dWindSped = 5.00;
		dWindDirecX = 0.00;
		dWindDirecY = 1.00;
	}
};

struct stLightTime
{
	double dLightTime;

	stLightTime()
	{
		dLightTime = 9.0;
	}
};

enum enumUpdateParamEnvType
{
	NotifyUpdateAllEnvParam,  //更新所有环境参数
	NotifyUpdateWeather,      //更新天气参数
	NotifyUpdateCloud,        //更新云层参数
	NotifyUpdateFog,          //更新雾参数
	NotifyUpdateWind,         //更新风层参数
	NotifyUpdateLightTime,    //更新光照时间参数
};

typedef struct _HWASIM_PARAMENV
{
	stWeather Weather;
	stCloud  Cloud;
	stFog     Fog;
	stWind    Wind;
	stLightTime LightTime;
} ParamEnv;

#pragma endregion

#pragma region 传感器参数
//图像分辨率
struct stImageResolution
{
	int nHorizontalResolution; // 水平分辨率
	int nVerticalResolution;   // 垂直分辨率
};

//视场角
struct stFOV
{
	double dHorizontalFOV; // 水平视场角
	double dVerticalFOV;   // 垂直视场角
};

//孔径大小
enum eApertureSizeType
{
	eApertureSizeType_Large = 0,      //大
	eApertureSizeType_Medium = 1,     //中
	eApertureSizeType_Small = 2,      //小
};


const std::map<eApertureSizeType, std::string> NameMap_eApertureSizeType = {
	{ eApertureSizeType_Large,  "大" },
	{ eApertureSizeType_Medium, "中" },
	{ eApertureSizeType_Small,  "小" },
};

//孔径形状
enum eApertureShapeType
{
	eApertureShapeType_Round = 0,      //圆形
	eApertureShapeType_Square = 1,     //方形
};

const std::map<eApertureShapeType, std::string> NameMap_eApertureShapeType = {
	{ eApertureShapeType_Round,  "圆形" },
	{ eApertureShapeType_Square, "方形" },
};

// 传感器细微参数
struct stSensorDetailParam
{
	double dFNumber;            //F数
	double dFNoise;             //1/f噪声
	double dDetectorPitch;      //探测器间距
	double dDetectionTemp;      //探测器温度
	double dContrastRatio;      //对比度
	double dBrightRatio;        //亮度
	bool   dAutoGain;           //是否开启自动增益
	bool   dNonUniformity;      //是否开启非均匀性
	double dPixelSize;
	double dFocalLength;
	eApertureSizeType ApertureSizeType;
	eApertureShapeType ApertureShapeType;
};

//激光参数
struct stLaserParam
{
	double dPower;              //功率
	double dDivergenceAngle;    //发散角
	double dWavelength;         //波长
	double dBeamDiameter;       //光束直径
	double dMaximumLength;      //光束长度
	double dLaserTime;          //照射时间
};

//图像类型
enum eBWHotColorType
{
	eBWHotType_WhiteHot = 0,      //白热
	eBWHotType_BlackHot = 1,      //黑热
	eColorType_SignalColor = 2,   //单色
	eColorType_MultiColor = 3,    //彩色
};

const std::map<eBWHotColorType, std::string> NameMap_eBWHotColorType = {
	{ eBWHotType_WhiteHot,    "白热" },
	{ eBWHotType_BlackHot,    "黑热" },
	{ eColorType_SignalColor, "单色" },
	{ eColorType_MultiColor,  "彩色" },
};

//传感器类型
enum eWaveType
{
	eWaveType_VisibleLight = 0,        //可见光
	eWaveType_NearInfrared,          //近红外
	eWaveType_ShortInfrared,         //短波红外
	eWaveType_MidWaveInfrared,       //中波红外
	eWaveType_LongWaveInfrared,      //长波红外
	eWaveType_ShimmerWaveInfrared,   //微光夜视
};

const std::map<eWaveType, std::string> NameMap_eWaveType = {
	{ eWaveType_VisibleLight,      "可见光传感器" },
	{ eWaveType_NearInfrared,      "近红外传感器" },
	{ eWaveType_ShortInfrared,     "短波红外传感器" },
	{ eWaveType_MidWaveInfrared,   "中波红外传感器" },
	{ eWaveType_LongWaveInfrared,  "长波红外传感器" },
	{ eWaveType_ShimmerWaveInfrared,"微光夜视传感器" },
};

// 成像波段
struct stWaveParam
{
	eWaveType waveType;
	eBWHotColorType imgType;
};

typedef struct _HWASIM_SensorParam
{
	//朝向目标ID
	int _LookTargetId;
	//成像波段
	stWaveParam _WaveParam;
	//分辨率
	stImageResolution _Resolution;
	//视场角
	stFOV _FOV;
	// 传感器细微参数
	stSensorDetailParam _DetailParam;
	// 激光参数
	stLaserParam _LaserParam;
	// 传感器位姿信息
	PositionInfo _Position;
} SensorConfigInfo;

enum enumUpdateSensorParamType
{
	NotifyUpdateAllSensorParam,  //更新所有传感器参数
	NotiFyUpdateLook,            //更新侦察目标
	NotiFyUpdatePosition,        //更新位姿信息
	NotifyUpdateWave,            //更新成像波段参数
	NotifyUpdateWaveImgType,     //更新成像波段图像参数
	NotifyUpdateResolution,      //更新分辨率参数
	NotifyUpdateFOV,             //更新视场角参数
	NotifyUpdateDetail_FNum,     //更新传感器细微参数-F数
	NotifyUpdateDetail_FNoise,   //更新传感器细微参数-1/f噪声
	NotifyUpdateDetail_Detection,//更新传感器细微参数-探测器参数
	NotifyUpdateDetail_Gain,     //更新传感器细微参数-增益参数
	NotifyUpdateLaser_Power,     //更新激光参数-功率
	NotifyUpdateLaser_Angle,     //更新激光参数-发散角
	NotifyUpdateLaser_Wavelength,//更新激光参数-波长
	NotifyUpdateLaser_BeamDiameter,//更新激光参数-光束直径
	NotifyUpdateLaser_MaxLength, //更新激光参数-光束长度
	NotifyUpdateLaser_Time,      //更新激光参数-照射时间
	NotifyUpdateDetail_ApertureSize,//更新传感器细微参数-孔径大小
	NotifyUpdateDetail_ApertureShape,//更新传感器细微参数-孔径形状
	NotifyUpdateDetail_NonUniformity,//更新传感器细微参数-非均匀性
	NotifyUpdateDetail_PixelSize,    //更新传感器细微参数-像元大小
	NotifyUpdateDetail_FocalLength   //更新传感器细微参数-焦距
};

enum enumSensorType
{
	SensorLeft,         //更新左传感器参数
	SensorRight,        //更新右传感器参数
	SensorSynch,        //同步更新左右传感器参数
};
#pragma endregion

#pragma region 配置信息

//通讯配置信息
struct ConfigSensorSet
{
	bool isShowTwoSensor;
};

//通讯配置信息
struct ConfigCommunication
{
	std::string _LocalIp;    
	int _LocalPort;
	std::string _RemoteIp;   
	int _RemotePort;
};

enum enumAutoDeleteTime
{
	DeleteNever = 0,      //永不删除
	DeleteByMonth,        //按月删除
	DeleteByDay,          //按天删除
	DeleteByHour,         //按小时删除
};

//图像序列管理设置
struct ConfigSimImgSet
{
	bool isOpenRealTimeSaveImg;    //是否开启实时图像自动保存

	std::string _RealTime_SaveAddr;    //实时保存图像地址（替换QString）
	std::string _Single_SaveAddr;      //单次保存图像地址（替换QString）
	enumAutoDeleteTime _RealTime_AutoDelete;   //实时保存自动删除时间
	enumAutoDeleteTime _Single_AutoDelete;     //单次保存自动删除时间
};

#pragma endregion

#pragma region 传感器功能事件
enum eSensorFunctionEventType
{
	eSensorFunctionEventType_TrackingTarget = 0,        //跟踪
	eSensorFunctionEventType_UnTrackingTarget,          //取消跟踪
	eSensorFunctionEventType_TrackingSence,             //场景跟踪
	eSensorFunctionEventType_UnTrackingSence,           //取消场景跟踪
	eSensorFunctionEventType_TrackingMultiTarget,       //多目标跟踪开启
	eSensorFunctionEventType_UnTrackingMultiTarget,     //多目标跟踪关闭
	eSensorFunctionEventType_SingleRanging,             //单次测距
	eSensorFunctionEventType_ContinuousRanging,         //连续测距
	eSensorFunctionEventType_DoubleContinuousRanging,   //连续双机测距
	eSensorFunctionEventType_CancelRanging,             //取消测距
	eSensorFunctionEventType_LaserIrradiationOn,        //照射开
	eSensorFunctionEventType_LaserIrradiationOff,       //照射关
	eSensorFunctionEventType_LaserPowerOn,              //激光器上电
	eSensorFunctionEventType_LaserPowerOff,             //激光器下电
	eSensorFunctionEventType_LaserEnable,               //激光使能开
	eSensorFunctionEventType_LaserDisable,              //激光使能关
};
#pragma endregion

#pragma region 字符串叠加
struct stOverlayPositionInfo
{
	double dLon;        //经度
	double dLat;        //纬度
	double dAlt;        //高度
};

enum enumOverlayType
{
	enumOverlayType_AZ = 0,        //跟踪
	enumOverlayType_EL,
	enumOverlayType_SCAN,          //取消跟踪
	enumOverlayType_SensorMode,    //单次测距
	enumOverlayType_PP,            //连续测距
	enumOverlayType_WP,            //取消测距
	enumOverlayType_RealTime,      //照射开
	enumOverlayType_SR,            //照射关
	enumOverlayType_D,             //激光器上电
	enumOverlayType_LD,            //激光器下电
	enumOverlayType_LP,            //激光使能开
	enumOverlayType_SINS,          //激光使能关
	enumOverlayType_C,             //激光使能关
	enumOverlayType_View,          //视场大小
};

struct stOverlayParam
{
	double dAZ;                    //功率
	double dEL;                    //发散角
	unsigned char nSCAN;           //波长
	std::string strSensorMode;     //光束直径
	stOverlayPositionInfo _PP;     //光束长度
	stOverlayPositionInfo _WP;     //照射时间
	std::string strRealTime;      
	double dSR;
	double dD;
	char dLD;
	stOverlayPositionInfo _LP;
	std::string strSINS;          
	double dC;
	double strView;                //视场大小字符串
	bool Laser;
	//temp lp的参数
	float yawValue;
	float pitchValue;
	float rollValue;
};

#pragma endregion

#pragma region 传感器移动控制事件
struct stSearchPositionInfo
{
	double dLon;        //经度
	double dLat;        //纬度
	double dAlt;        //高度
};

enum enumSensorMoveControlEventType
{
	enumSensorMoveType_Lock = 0,          //锁定
	enumSensorMoveType_Scan,              //扫描
	enumSensorMoveType_ScanP,             //扫描+
	enumSensorMoveType_Search,            //小区搜索
	enumSensorMoveType_SearchP,           //小区搜索+/对焦凝视/无对焦凝视
	enumSensorMoveType_Manual,            //手动
	enumSensorMoveType_PointTrack,        //点选跟踪
	enumSensorMoveType_Enable,            //收起/放下
	enumSensorMoveType_None,              //取消标志位
};

struct stSensorMoveParam
{
	double Lock_AZ;
	double Lock_EL;
	double Scan_WIDTH;
	double Scan_VELOCITY;
	double X_COORDINATE;
	double Y_COORDINATE;
	//std::string AC_TYPE;//飞行器类型
	int AC_NO;          //飞行器编号
	double AngularVelocity;
	double Joystick_AZ; //操作杆方位控制参数
	double Joystick_EL; //操作杆俯仰参数
	int SearchType;
	stSearchPositionInfo SearchPosition;
	bool IsEnable;
};
#pragma endregion
