#pragma once

#include <cmath>
#include <stdexcept>
#include "lmatrix.h"   // 引入 Panda3D 的矩阵数学库
#include "lvecBase3.h"  // 引入 Panda3D 的向量数学库

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// WGS84 椭球常数
const double WGS84_A_A = 6378137.0;                // 长半轴 [m]
const double WGS84_F_A = 1.0 / 298.257223563;      // 扁率
const double WGS84_E2_A = 2 * WGS84_F_A - WGS84_F_A * WGS84_F_A; // 第一偏心率平方

														 // 角度转弧度
inline double deg2rad(double deg) { return deg * M_PI / 180.0; }
inline double rad2deg(double rad) { return rad * 180.0 / M_PI; }

namespace BYHWICD {
	// 声明协议中的结构体，方便做接口适配
	struct SpatialState;
	struct Euler;
}

class AttitudeTransform {
public:
	// 构造函数
	AttitudeTransform();

	/**
	* 将大地坐标 (lat, lon, alt) 转换为 ECEF 坐标 (X, Y, Z)
	* @param lat 纬度 [rad]
	* @param lon 经度 [rad]
	* @param alt 椭球高 [m]
	* @param X, Y, Z 输出 ECEF 坐标 [m]
	*/
	void llhToEcef(double lat, double lon, double alt, double& X, double& Y, double& Z);

	/**
	* 计算目标点相对于本机的径向距离、俯仰角和偏航角（机体坐标系）
	* @param lat0_deg   本机纬度 [度]
	* @param lon0_deg   本机经度 [度]
	* @param alt0       本机高度 [m] (椭球高)
	* @param roll_deg   本机横滚角 [度] (右翼向下为正)
	* @param pitch_deg  本机俯仰角 [度] (机头向上为正)
	* @param yaw_deg    本机偏航角 [度] (从北顺时针为正)
	* @param latT_deg   目标纬度 [度]
	* @param lonT_deg   目标经度 [度]
	* @param altT       目标高度 [m] (椭球高)
	* @param range      输出径向距离 [m]
	* @param rel_pitch_deg 输出相对俯仰角 [度] (目标在飞机上方为正)
	* @param rel_yaw_deg   输出相对偏航角 [度] (目标在飞机右侧为正)
	*/
	void computeRelativePosition(double lat0_deg, double lon0_deg, double alt0,
		double roll_deg, double pitch_deg, double yaw_deg,
		double latT_deg, double lonT_deg, double altT,
		double& range, double& rel_pitch_deg, double& rel_yaw_deg);

};