#include "GeoTransform.h"
#include "CommonData.h"

GeoTransform::GeoTransform()
	: m_isInitialized(false), m_refLatRad(0), m_refLonRad(0),
	m_refEcefX(0), m_refEcefY(0), m_refEcefZ(0),
	m_sinLat0(0), m_cosLat0(0), m_sinLon0(0), m_cosLon0(0) {
}

double GeoTransform::DegToRad(double degrees) {
	return degrees * 3.14159265358979323846 / 180.0;
}

void GeoTransform::InitReferencePoint(double refLat, double refLon, double refAlt) {
	m_refLatRad = DegToRad(refLat);
	m_refLonRad = DegToRad(refLon);

	// 计算参考原点的 ECEF 坐标
	Wgs84ToEcef(m_refLatRad, m_refLonRad, refAlt, m_refEcefX, m_refEcefY, m_refEcefZ);

	// 预计算 ENU 转换矩阵需要的三角函数
	m_sinLat0 = std::sin(m_refLatRad);
	m_cosLat0 = std::cos(m_refLatRad);
	m_sinLon0 = std::sin(m_refLonRad);
	m_cosLon0 = std::cos(m_refLonRad);

	m_isInitialized = true;
}

void GeoTransform::Wgs84ToEcef(double latRad, double lonRad, double alt, double& x, double& y, double& z) const {
	double sinLat = std::sin(latRad);
	double cosLat = std::cos(latRad);
	double sinLon = std::sin(lonRad);
	double cosLon = std::cos(lonRad);

	double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

	x = (N + alt) * cosLat * cosLon;
	y = (N + alt) * cosLat * sinLon;
	z = (N * (1.0 - WGS84_E2) + alt) * sinLat;
}

void GeoTransform::Wgs84ToPandaXYZ(double lat, double lon, double alt, double& outX, double& outY, double& outZ) const {
	if (!m_isInitialized) {
		outX = 0; outY = 0; outZ = 0;
		return;
	}

	// 转为目标点的 ECEF 坐标
	double tgtLatRad = DegToRad(lat);
	double tgtLonRad = DegToRad(lon);
	double tgtEcefX, tgtEcefY, tgtEcefZ;

	Wgs84ToEcef(tgtLatRad, tgtLonRad, alt, tgtEcefX, tgtEcefY, tgtEcefZ);

	// 计算 ECEF 增量
	double dx = tgtEcefX - m_refEcefX;
	double dy = tgtEcefY - m_refEcefY;
	double dz = tgtEcefZ - m_refEcefZ;

	// ECEF 乘以旋转矩阵转换为 ENU 局部坐标系
	double east = -m_sinLon0 * dx + m_cosLon0 * dy;
	double north = -m_sinLat0 * m_cosLon0 * dx - m_sinLat0 * m_sinLon0 * dy + m_cosLat0 * dz;
	double up = m_cosLat0 * m_cosLon0 * dx + m_cosLat0 * m_sinLon0 * dy + m_sinLat0 * dz;

	// 映射到 HwaSimIR 坐标系 (X=East, Y=North, Z=Up)
	outX = east;
	outY = north;
	outZ = up;
}

void GeoTransform::Wgs84ToPandaXYZ(const BYHWICD::SpatialState& spatial, double& outX, double& outY, double& outZ) const {
	Wgs84ToPandaXYZ(spatial.lat, spatial.lon, spatial.alt, outX, outY, outZ);
}

void GeoTransform::AttitudeToPandaHPR(double yaw, double pitch, double roll, double& outH, double& outP, double& outR) const {
	// 协议定义：yaw(航向, 0-360°, 顺时针为正), pitch(俯仰, ±90°, 上为正), roll(滚转, ±180°, 右为正)
	// Panda3D定义：Heading(绕Z轴旋转, 逆时针为正), Pitch(绕X轴旋转, 上为正), Roll(绕Y轴旋转, 右偏为正)

	outH = -yaw;   // 逆时针与顺时针相反
	outP = pitch;  // 上为正一致
	outR = roll;   // 右侧翻转一致
}