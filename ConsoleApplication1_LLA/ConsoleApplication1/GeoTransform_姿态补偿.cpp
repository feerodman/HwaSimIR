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

	Wgs84ToEcef(m_refLatRad, m_refLonRad, refAlt, m_refEcefX, m_refEcefY, m_refEcefZ);

	m_sinLat0 = std::sin(m_refLatRad);
	m_cosLat0 = std::cos(m_refLatRad);
	m_sinLon0 = std::sin(m_refLonRad);
	m_cosLon0 = std::cos(m_refLonRad);

	m_isInitialized = true;
}

void GeoTransform::Wgs84ToEcef(double latRad, double lonRad, double alt, double& x, double& y, double& z) const {
	double sinLat = std::sin(latRad);
	double cosLat = std::cos(latRad);
	double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

	x = (N + alt) * cosLat * std::cos(lonRad);
	y = (N + alt) * cosLat * std::sin(lonRad);
	z = (N * (1.0 - WGS84_E2) + alt) * sinLat;
}

void GeoTransform::Wgs84ToPandaXYZ(double lat, double lon, double alt, double& outX, double& outY, double& outZ) const {
	if (!m_isInitialized) {
		outX = 0; outY = 0; outZ = 0;
		return;
	}

	// 1. 转为目标点的 ECEF 坐标
	double tgtLatRad = DegToRad(lat);
	double tgtLonRad = DegToRad(lon);
	double tgtEcefX, tgtEcefY, tgtEcefZ;
	Wgs84ToEcef(tgtLatRad, tgtLonRad, alt, tgtEcefX, tgtEcefY, tgtEcefZ);

	// 2. 计算 ECEF 增量
	double dx = tgtEcefX - m_refEcefX;
	double dy = tgtEcefY - m_refEcefY;
	double dz = tgtEcefZ - m_refEcefZ;

	// 3. ECEF 增量乘以旋转矩阵，转换为参考点所在的局部 ENU (East, North, Up)
	outX = -m_sinLon0 * dx + m_cosLon0 * dy;
	outY = -m_sinLat0 * m_cosLon0 * dx - m_sinLat0 * m_sinLon0 * dy + m_cosLat0 * dz;
	outZ = m_cosLat0 * m_cosLon0 * dx + m_cosLat0 * m_sinLon0 * dy + m_sinLat0 * dz;
}

void GeoTransform::Wgs84ToPandaXYZ(const BYHWICD::SpatialState& spatial, double& outX, double& outY, double& outZ) const {
	Wgs84ToPandaXYZ(spatial.lat, spatial.lon, spatial.alt, outX, outY, outZ);
}

void GeoTransform::AttitudeToPandaHPR(double lat, double lon, double yaw, double pitch, double roll, double& outH, double& outP, double& outR) const {
	if (!m_isInitialized) {
		outH = -yaw; outP = pitch; outR = roll;
		return;
	}

	// 1. 协议定义的本地姿态转换为 Panda3D 的 HPR 旋转矩阵 (Body 到 局部ENU)
	LMatrix3d matBodyToLocal;
	matBodyToLocal.set_hpr(LVecBase3d(-yaw, pitch, roll));

	// 2. 构造【目标局部 ENU】 到 【ECEF】的基底旋转矩阵 (按 Panda3D 行向量习惯)
	double latRad = DegToRad(lat);
	double lonRad = DegToRad(lon);
	double slat = std::sin(latRad), clat = std::cos(latRad);
	double slon = std::sin(lonRad), clon = std::cos(lonRad);

	LMatrix3d matLocalToEcef(
		-slon, clon, 0,
		-slat*clon, -slat*slon, clat,
		clat*clon, clat*slon, slat
	);

	// 3. 构造【参考点 ENU】 到 【ECEF】的旋转矩阵，并求逆（转置即可），得到 ECEF 到 Reference ENU
	LMatrix3d matRefToEcef(
		-m_sinLon0, m_cosLon0, 0,
		-m_sinLat0*m_cosLon0, -m_sinLat0*m_sinLon0, m_cosLat0,
		m_cosLat0*m_cosLon0, m_cosLat0*m_sinLon0, m_sinLat0
	);
	LMatrix3d matEcefToRef = matRefToEcef;
	matEcefToRef.transpose_in_place(); // 正交矩阵的转置即为逆矩阵

									   // 4. 计算总姿态：Body -> 目标局部ENU -> ECEF -> 中心点参考ENU
	LMatrix3d matBodyToRef = matBodyToLocal * matLocalToEcef * matEcefToRef;

	// 5. 从最终的补偿矩阵中提取 Panda3D 视角的 HPR
	LVecBase3d finalHpr = matBodyToRef.get_hpr();
	outH = finalHpr[0];
	outP = finalHpr[1];
	outR = finalHpr[2];
}

void GeoTransform::AttitudeToPandaHPR(const BYHWICD::SpatialState& spatial, double& outH, double& outP, double& outR) const {
	AttitudeToPandaHPR(spatial.lat, spatial.lon, spatial.yaw, spatial.pitch, spatial.roll, outH, outP, outR);
}