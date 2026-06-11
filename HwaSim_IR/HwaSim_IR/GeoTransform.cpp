#include "GeoTransform.h"
#include "CommonData.h"

GeoTransform::GeoTransform() : m_isInitialized(false) {
}

void GeoTransform::InitReferencePoint(double refLat, double refLon, double refAlt) {
	m_refPos.lat = refLat;
	m_refPos.lon = refLon;
	m_refPos.alt = refAlt;

	// 使用官方算法计算原点的 ECEF 坐标并缓存
	m_refEcef = LLA2ECEF(m_refPos);

	m_isInitialized = true;
}

bool GeoTransform::IsInitialized() const {
	return m_isInitialized;
}

LMatrix4f GeoTransform::GetPandaMatrix(const BYHWICD::SpatialState& spatial) const {
	if (!m_isInitialized) {
		return LMatrix4f::ident_mat();
	}

	// 构造目标的状态体
	ICD::Position tgtPos;
	tgtPos.lat = spatial.lat;
	tgtPos.lon = spatial.lon;
	tgtPos.alt = spatial.alt;

	ICD::Euler tgtEuler;
	tgtEuler.yaw = spatial.yaw;
	tgtEuler.pitch = spatial.pitch;
	tgtEuler.roll = spatial.roll;

	// ==========================================
	// 计算绝对位置 (平移部分)
	// ==========================================
	ICD::CartesianCoordinate tgtEcef = LLA2ECEF(tgtPos);
	// 目标在中心原点 ENU 坐标系下的绝对坐标
	ICD::CartesianCoordinate tgtEnu = ECEF2ENU(m_refEcef, m_refPos, tgtEcef);

	// ==========================================
	// 计算精确姿态 (旋转基向量部分)
	// ==========================================
	// 根据官方算法，机体坐标为 FRD(前右下)。而 HwaSimIR 局部为 RFU(右前上)
	// 定义 HwaSimIR 的三个坐标轴在机体内的单位向量：
	ICD::CartesianCoordinate pandaRightInBody = { 0.0, 1.0,  0.0 }; // HwaSimIR 的 X轴 (右) = 机体 Y轴
	ICD::CartesianCoordinate pandaForwardInBody = { 1.0, 0.0,  0.0 }; // HwaSimIR 的 Y轴 (前) = 机体 X轴
	ICD::CartesianCoordinate pandaUpInBody = { 0.0, 0.0, -1.0 }; // HwaSimIR 的 Z轴 (上) = 机体 -Z轴

	// 将 HwaSimIR 的三个轴，从机体转换到【目标当地的 ENU】
	ICD::CartesianCoordinate axisX_tgtENU = BodyCoordinate2ENU(pandaRightInBody, tgtEuler);
	ICD::CartesianCoordinate axisY_tgtENU = BodyCoordinate2ENU(pandaForwardInBody, tgtEuler);
	ICD::CartesianCoordinate axisZ_tgtENU = BodyCoordinate2ENU(pandaUpInBody, tgtEuler);

	// 补偿地球曲率：将【目标当地 ENU】的纯向量转换到【ECEF】向量
	// 传入的 origin(ECEFSelf) 为 {0,0,0}，这样提取出来的就是纯旋转，没有平移
	ICD::CartesianCoordinate zeroEcef = { 0.0, 0.0, 0.0 };
	ICD::CartesianCoordinate axisX_ECEF = ENU2ECEF(zeroEcef, tgtPos, axisX_tgtENU);
	ICD::CartesianCoordinate axisY_ECEF = ENU2ECEF(zeroEcef, tgtPos, axisY_tgtENU);
	ICD::CartesianCoordinate axisZ_ECEF = ENU2ECEF(zeroEcef, tgtPos, axisZ_tgtENU);

	// 将【ECEF】的纯向量，转换到【中心原点当地的 ENU】向量
	ICD::CartesianCoordinate axisX_refENU = ECEF2ENU(zeroEcef, m_refPos, axisX_ECEF);
	ICD::CartesianCoordinate axisY_refENU = ECEF2ENU(zeroEcef, m_refPos, axisY_ECEF);
	ICD::CartesianCoordinate axisZ_refENU = ECEF2ENU(zeroEcef, m_refPos, axisZ_ECEF);

	// ==========================================
	// 组装 HwaSimIR 4x4 变换矩阵
	// ==========================================
	// 前三行对应 X, Y, Z 三个轴在空间中的朝向 (方向余弦)，最后一行对应 X, Y, Z 坐标
	LMatrix4f finalMat(
		axisX_refENU.x, axisX_refENU.y, axisX_refENU.z, 0.0,
		axisY_refENU.x, axisY_refENU.y, axisY_refENU.z, 0.0,
		axisZ_refENU.x, axisZ_refENU.y, axisZ_refENU.z, 0.0,
		tgtEnu.x, tgtEnu.y, tgtEnu.z, 1.0
	);

	return finalMat;
}