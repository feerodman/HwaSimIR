#pragma once

#include <cmath>
#include "lmatrix3.h"   // 引入 Panda3D 矩阵数学库
#include "lvecBase3.h"  // 引入 Panda3D 向量数学库

// WGS84 椭球体参数常量
constexpr double WGS84_A = 6378137.0;             // 长半轴 (米)
constexpr double WGS84_E2 = 0.00669437999014;     // 第一偏心率平方

namespace BYHWICD {
	struct SpatialState;
}

class GeoTransform {
public:
	GeoTransform();

	// 1. 初始化仿真中心原点（收到第一个平台位置时调用，设置其为 0,0,0）
	void InitReferencePoint(double refLat, double refLon, double refAlt);

	// 2. 将 WGS84 经纬高转换为 Panda3D 笛卡尔坐标 (East, North, Up)
	void Wgs84ToPandaXYZ(double lat, double lon, double alt, double& outX, double& outY, double& outZ) const;
	void Wgs84ToPandaXYZ(const BYHWICD::SpatialState& spatial, double& outX, double& outY, double& outZ) const;

	// 3. 将目标的局部姿态 (Yaw, Pitch, Roll) 补偿地球曲率后，转换为 Panda3D 的 HPR
	// 注意：必须传入目标当前的 lat 和 lon，才能计算其局部地平面与中心原点地平面的倾角差
	void AttitudeToPandaHPR(double lat, double lon, double yaw, double pitch, double roll, double& outH, double& outP, double& outR) const;
	void AttitudeToPandaHPR(const BYHWICD::SpatialState& spatial, double& outH, double& outP, double& outR) const;

	// 是否已经初始化原点
	bool IsInitialized() const;

private:
	static double DegToRad(double degrees);
	void Wgs84ToEcef(double latRad, double lonRad, double alt, double& x, double& y, double& z) const;

private:
	bool m_isInitialized;

	// 参考原点的弧度值
	double m_refLatRad;
	double m_refLonRad;

	// 参考原点的 ECEF (地心) 坐标
	double m_refEcefX;
	double m_refEcefY;
	double m_refEcefZ;

	// 预计算旋转矩阵所需的三角函数值
	double m_sinLat0;
	double m_cosLat0;
	double m_sinLon0;
	double m_cosLon0;
};