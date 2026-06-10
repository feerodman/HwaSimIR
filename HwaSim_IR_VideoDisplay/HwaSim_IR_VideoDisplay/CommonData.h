//
// Created by kahn1 on 2026/1/15.
//
//该头文件定义激励数据软件（甲方）与边缘端红外仿真软件（乙方）的通信协议:
//交互流程为：
//（1）激励数据软件发送复位 ControlP2cX1ObjTrackingCmd 至边缘端红外仿真软件
//（2）激励数据软件发送成像初始化 InitP2cObjectTrackingCmd 至边缘端红外仿真软件
//（3）边缘端红外仿真软件发送初始化应答 InitAckC2pObjectTrackingCmd 至激励数据软件
//（4）激励数据软件发送开始 ControlP2cX1ObjTrackingCmd 至边缘端红外仿真软件
//（5）激励数据软件发送实时成像数据包 DisplayC2cObjTrackingData 至边缘端红外仿真软件
//（6）周期性进行（5），进行本回合仿真，期间激励数据软件按需改变实时成像数据包中的控制变量；同时，边缘端红外仿真软件实时输出视频数据和标注数据
//（7）激励数据软件发送停止 ControlP2cX1ObjTrackingCmd 至边缘端红外仿真软件，本回合仿真结束
//（8）重复（1）至（7），进行下一回合仿真

#ifndef EDGESIDEIRSIM_COMMON_H
#define EDGESIDEIRSIM_COMMON_H

#pragma once
#pragma pack (1)

namespace BYHWICD {
    //直角坐标系
    struct CartesianCoordinate {
        double x;   //米
        double y;   //米
        double z;   //米
    };
    //位置信息
    struct Position {
        double lat; //纬度，北为正
        double lon; //经度，东为正
        double alt; //海拔，米
    };
    //速度信息
    struct SpeedENU {
        double SpeedE;  //m/s
        double SpeedN;  //m/s
        double SpeedU;  //m/s
    };
    //加速度信息
    struct AccelerENU {
        double AccelerE;  //m/s²
        double AccelerN;  //m/s²
        double AccelerU;  //m/s²
    };
    //姿态信息
    struct Euler {
        double yaw;     //航向，0-360°，顺时针为正
        double pitch;   //俯仰，±90°，上为正
        double roll;    //滚转，±180°，右为正
    };
    //位置姿态信息
    struct PosAtti: public Position, public Euler {};
    //空间状态信息
    struct SpatialState: public Position, public Euler {
        double speed;   //Km/h
    };
    //方位角和俯仰角
    struct relAngular {
        double pitch;
        double azimuth;
    };
    //成像初始化参数
    struct trackerSensorParam {
        int index;  //传感器编号
        bool coarseTrackEn; //是否开启粗跟
        bool preciseTrackEn;    //是否开启精跟
        bool h264En;    //是否开启压缩
        double coarseTrackResolution;   //粗角分辨率
        double preciseTrackResolution;   //精角分辨率
        bool noiseEn;   //是否开启模糊和噪声
        double trackerSensorNoise;  //噪声系数
        bool realtimeAnnotation; //是否实时标注
        bool saveMP4En; //是否保存mp4文件
        int trackerSensorBand; //成像波段 0-短波红外 2-中波红外
        int trackerSensorWidth;   //成像宽度
        int trackerSensorHeight;  //成像高度
        int trackerSensorViewMin; //成像最小距离
        int trackerSensorViewMax; //成像最大距离
        double trackerSensorPixelAngle; //角分辨率
        //跟瞄器安装参数，位置（米）
        double trackerX;
        double trackerY;
        double trackerZ;
        //跟瞄器安装参数，角度（°）
        double trackerPitch;
        double trackerYaw;
        double trackerRoll;
        //跟瞄器转动范围，角度（°）
        double trackerElMin; //最小仰角
        double trackerElMax; //最大仰角
        double trackerAzMin; //最小偏角
        double trackerAzMax; //最大偏角
        //照明器安装参数，位置（米）
        double illuminatorX;
        double illuminatorY;
        double illuminatorZ;
        //照明器安装参数，角度（°）
        double illuminatorPitch;
        double illuminatorYaw;
        double illuminatorRoll;
        //照明器转动范围，角度（°）
        double illuminatorElMin; //最小仰角
        double illuminatorElMax; //最大仰角
        double illuminatorAzMin; //最小偏角
        double illuminatorAzMax; //最大偏角
        //照明器参数
        double illuminatorAngle; //张角（mrad）
        double illuminatorSpotRad; //光斑强度
        //发射器安装参数，位置（米）
        double emitterX;
        double emitterY;
        double emitterZ;
        //发射器安装参数，角度（°）
        double emitterPitch;
        double emitterYaw;
        double emitterRoll;
        //发射器转动范围，角度（°）
        double emitterElMin; //最小仰角
        double emitterElMax; //最大仰角
        double emitterAzMin; //最小偏角
        double emitterAzMax; //最大偏角
        //发射器光参
        int emitterSpotRadius; //半径（像素）
        double emitterSpotRad; //亮度
    };
    //成像初始化参数2
    struct InitObjectTrackingParam {
        bool enable;//是否挂载
        int envTerrain; //地形 0-戈壁 1-山区 2-海面
        int envSky; //天气 0-晴 1-云 2-雨 3-雪 4-雾 5-阴
        double envMaxHeightRain; //最大降雨高度（m）
        double envTransHeightRain; //降雨过渡高度（m）
        double envMaxHeightSnow; //最大降雪高度（m）
        double envTransHeightSnow; //降雪过渡高度（m）
        double envRainSnowSpeedScale; //雨雪相对速度系数
        double envRadScaleTerrain;  //地形亮度调整系数
        double envRadScaleSky;  //天空亮度调整系数
        double envTemp; //温度（°）
        double envHumidity; //湿度（%）
        double envVisibility; //能见度（米）
        double envWindV; //风速（m/s）
        double envWindDir; //风向（°），北偏东为正
        int videoFps; //帧率
        trackerSensorParam trackerSensor[1];
    };
    //回合控制信号
    struct ControlP2cX1ObjTrackingCmd {
        int flag = 0x41;
        int JB;//军别 1-红方 2-蓝方
        int platID;//挂载平台ID
        int simCommand;// 1-复位 2-开始 3-停止
        int roundCut;//总回合数
        int currentRound;//当前仿真回合数
    };


	//实时Wg信息
	struct WeaponState {
		int targetType; //目标类型 0x00-无 0x11-飞机 0x22-雷达导弹 0x33-红外弹 0x44-MMD
		int targetPlatID; //被打击目标挂载平台ID
		int targetID; //目标ID
		double xxOutAng[2]; // [0]-目标与机轴的方位角（°） [1]-目标与机轴的俯仰角（°）
		bool lookatEn; //是否自动对准
		bool illuminatorEn; //是否开启照明器
		double offsetAng[2]; //角度偏移量 [0]-pitch [1]-yaw
		bool viewValid; //是否在视场（目标）
		int damageFlag; //目标是否毁伤 0-未毁 1-毁伤
		bool strikeFlag; //打击标志
		int strikePart; //打击部位 1-头 2-舱
	};
	//目标状态信息
	struct TargetState {
		int targetType; //目标类型 0x11-飞机 0x22-雷达导弹 0x33-红外弹 0x44-MMD
		int targetPlatID; //被打击目标挂载平台ID
		int targetID; //目标ID
		bool engineState; //发动机状态(0-关机 1-开机)
		bool viewValid; //是否在视场
		SpatialState targetLoc; //目标位置信息
		int targetState; //0x01-打击态 0x02-爆炸态 0x03-击毁态
	};
	struct PlatParamPak {
		int id;//飞机编号
		int type;//1-红方 2-蓝方
		SpatialState spatial;//空间状态
	};
    //成像初始化命令
    struct InitP2cObjectTrackingCmd {
        int flag = 0x36;
        int JB;
        int platID;//挂载平台ID
        int sensorID;//传感器ID 为255则全部接收
        int platNumValid;//有效平台数
        PlatParamPak platParam[2];//飞机初始参数
        InitObjectTrackingParam trackingInit;//传感器参数
		//WeaponState weaponState;
		//int targetNumValid; //有效目标数
		//TargetState targetState[5];
        int MissileMaxCount120;//可能成像导弹的最大个数
        int MissileMaxCount9;
        int MissileMaxCountMMD;
    };
    //实时成像数据
    struct DisplayC2cObjTrackingData {
        int flag = 0x38;
        int platID;//挂载平台ID
        int sensorID;//传感器ID
        double time;//当前时间（ms）
        SpatialState platLoc;//传感器挂载平台姿态信息
        WeaponState weaponState;
        int targetNumValid; //有效目标数
        TargetState targetState[5];
    };
    //初始化应答命令
    struct InitAckC2pObjectTrackingCmd {
        int flag = 0x37;
        int JB;//军别 1-红方 2-蓝方
        int platID;//挂载平台ID
        int sensorID;//传感器ID 为255则全部接收
        bool trackingReady; //0-没有准备好 1-准备好了
    };
    
}

#pragma pack ()
#endif //EDGESIDEIRSIM_COMMON_H


