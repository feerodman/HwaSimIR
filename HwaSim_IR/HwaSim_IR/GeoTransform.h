#pragma once

#include "math_algorithm.h" // 直接引入你们协议的官方算法库
#include "lmatrix.h"       // Panda3D 4x4 矩阵
#include "lvecBase3.h"

namespace BYHWICD {
	struct SpatialState;
}

class GeoTransform {
public:
	GeoTransform();

	// 初始化仿真中心原点
	void InitReferencePoint(double refLat, double refLon, double refAlt);

	// 利用 math_algorithm.h 核心算法，直接生成 0 误差的 HwaSimIR 4x4 变换矩阵
	LMatrix4f GetPandaMatrix(const BYHWICD::SpatialState& spatial) const;

	// 是否已经初始化原点
	bool IsInitialized() const;

private:
	bool m_isInitialized;

	// 缓存仿真中心原点的官方数据结构
	ICD::Position m_refPos;
	ICD::CartesianCoordinate m_refEcef;
};