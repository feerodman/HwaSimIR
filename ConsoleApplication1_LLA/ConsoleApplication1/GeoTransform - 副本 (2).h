#pragma once

#include <cmath>

// WGS84 椭球体参数常量
constexpr double WGS84_A = 6378137.0;             // 长半轴 (米)
constexpr double WGS84_E2 = 0.00669437999014;     // 第一偏心率平方

namespace BYHWICD {
	// 声明协议中的结构体，方便做接口适配
	struct SpatialState;
	struct Euler;
}

class GeoTransform {
public:
	// 构造函数
	GeoTransform();

	// 初始化仿真中心原点（在收到第一个平台位置时调用）
	// 参数：参考点的纬度(度)、经度(度)、海拔高度(米)
	void InitReferencePoint(double refLat, double refLon, double refAlt);

	// 将 WGS84 (纬度, 经度, 高度) 转换为 HwaSimIR 局部坐标 (X, Y, Z)
	// 输出：outX(东), outY(北), outZ(天)
	void Wgs84ToPandaXYZ(double lat, double lon, double alt, double& outX, double& outY, double& outZ) const;

	// 重载：直接使用协议的 SpatialState 进行转换
	void Wgs84ToPandaXYZ(const BYHWICD::SpatialState& spatial, double& outX, double& outY, double& outZ) const;

	// 将协议姿态 (Yaw, Pitch, Roll) 转换为 HwaSimIR 的 HPR
	// 输出：outH, outP, outR
	void AttitudeToPandaHPR(double yaw, double pitch, double roll, double& outH, double& outP, double& outR) const;

	// 判断是否已经初始化了参考原点
	bool IsInitialized() const;

private:
	// 经纬度转弧度辅助函数
	static double DegToRad(double degrees);

	// 内部函数：WGS84 转 ECEF
	void Wgs84ToEcef(double latRad, double lonRad, double alt, double& x, double& y, double& z) const;

private:
	bool m_isInitialized;

	// 参考原点的弧度值
	double m_refLatRad;
	double m_refLonRad;

	// 参考原点的 ECEF 坐标
	double m_refEcefX;
	double m_refEcefY;
	double m_refEcefZ;

	// 预计算旋转矩阵的三角函数值，节省计算量
	double m_sinLat0;
	double m_cosLat0;
	double m_sinLon0;
	double m_cosLon0;
};