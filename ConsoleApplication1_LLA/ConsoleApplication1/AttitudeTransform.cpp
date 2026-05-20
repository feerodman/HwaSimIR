//#include "AttitudeTransform.h"
//
//#include "CommonData.h"
//
//
//AttitudeTransform::AttitudeTransform()
//{
//}
//void AttitudeTransform::llhToEcef(double lat, double lon, double alt, double& X, double& Y, double& Z) {
//	double sinLat = std::sin(lat);
//	double cosLat = std::cos(lat);
//	double N = WGS84_A / std::sqrt(1 - WGS84_E2 * sinLat * sinLat);
//	X = (N + alt) * cosLat * std::cos(lon);
//	Y = (N + alt) * cosLat * std::sin(lon);
//	Z = (N * (1 - WGS84_E2) + alt) * sinLat;
//}
//
//void AttitudeTransform::computeRelativePosition(double lat0_deg, double lon0_deg, double alt0,
//	double roll_deg, double pitch_deg, double yaw_deg,
//	double latT_deg, double lonT_deg, double altT,
//	double& range, double& rel_pitch_deg, double& rel_yaw_deg) {
//	// 转换为弧度
//	double lat0 = deg2rad(lat0_deg);
//	double lon0 = deg2rad(lon0_deg);
//	double latT = deg2rad(latT_deg);
//	double lonT = deg2rad(lonT_deg);
//	double roll = deg2rad(roll_deg);
//	double pitch = deg2rad(pitch_deg);
//	double yaw = deg2rad(yaw_deg);
//
//	// 1. 计算本机与目标的 ECEF 坐标
//	double X0, Y0, Z0, XT, YT, ZT;
//	llhToEcef(lat0, lon0, alt0, X0, Y0, Z0);
//	llhToEcef(latT, lonT, altT, XT, YT, ZT);
//
//	// 2. 差值向量 (目标 -> 本机)
//	double dX = XT - X0;
//	double dY = YT - Y0;
//	double dZ = ZT - Z0;
//
//	// 径向距离
//	range = std::sqrt(dX * dX + dY * dY + dZ * dZ);
//
//	// 3. 转换到本机 NED 坐标系 (北东地)
//	double sinLat0 = std::sin(lat0);
//	double cosLat0 = std::cos(lat0);
//	double sinLon0 = std::sin(lon0);
//	double cosLon0 = std::cos(lon0);
//
//	double n = -sinLat0 * cosLon0 * dX - sinLat0 * sinLon0 * dY + cosLat0 * dZ;
//	double e = -sinLon0 * dX + cosLon0 * dY;
//	double d = -cosLat0 * cosLon0 * dX - cosLat0 * sinLon0 * dY - sinLat0 * dZ;
//
//	// 4. 构造从 NED 到机体坐标系的旋转矩阵
//	double sφ = std::sin(roll);
//	double cφ = std::cos(roll);
//	double sθ = std::sin(pitch);
//	double cθ = std::cos(pitch);
//	double sψ = std::sin(yaw);
//	double cψ = std::cos(yaw);
//
//	// 矩阵 R (NED -> 机体)
//	double R[3][3] = {
//		{ cθ * cψ,           cθ * sψ,           -sθ },
//		{ sφ * sθ * cψ - cφ * sψ, sφ * sθ * sψ + cφ * cψ, sφ * cθ },
//		{ cφ * sθ * cψ + sφ * sψ, cφ * sθ * sψ - sφ * cψ, cφ * cθ }
//	};
//
//	// 机体坐标系下的向量 (x: 前, y: 右, z: 下)
//	double x_b = R[0][0] * n + R[0][1] * e + R[0][2] * d;
//	double y_b = R[1][0] * n + R[1][1] * e + R[1][2] * d;
//	double z_b = R[2][0] * n + R[2][1] * e + R[2][2] * d;
//
//	// 5. 计算相对角度
//	// 俯仰角: 目标在飞机上方为正 (基于 x-y 平面)
//	double horiz_dist = std::sqrt(x_b * x_b + y_b * y_b);
//	double pitch_rel = std::atan2(-z_b, horiz_dist);  // -z_b 向上为正
//
//													  // 偏航角: 目标在飞机右侧为正
//	double yaw_rel = std::atan2(y_b, x_b);
//
//	// 转换为度
//	rel_pitch_deg = rad2deg(pitch_rel);
//	rel_yaw_deg = rad2deg(yaw_rel);
//}


#include "AttitudeTransform.h"
#include "CommonData.h"

AttitudeTransform::AttitudeTransform()
{
}

void AttitudeTransform::llhToEcef(double lat, double lon, double alt, double& X, double& Y, double& Z) {
	double sinLat = std::sin(lat);
	double cosLat = std::cos(lat);
	double N = WGS84_A_A / std::sqrt(1.0 - WGS84_E2_A * sinLat * sinLat);
	X = (N + alt) * cosLat * std::cos(lon);
	Y = (N + alt) * cosLat * std::sin(lon);
	Z = (N * (1.0 - WGS84_E2_A) + alt) * sinLat;
}

void AttitudeTransform::computeRelativePosition(double lat0_deg, double lon0_deg, double alt0,
	double roll_deg, double pitch_deg, double yaw_deg,
	double latT_deg, double lonT_deg, double altT,
	double& range, double& rel_pitch_deg, double& rel_yaw_deg) {

	// 1. 转换为弧度
	double lat0 = deg2rad(lat0_deg);
	double lon0 = deg2rad(lon0_deg);
	double latT = deg2rad(latT_deg);
	double lonT = deg2rad(lonT_deg);

	// 2. 计算本机与目标的 ECEF 坐标 (地心笛卡尔)
	double X0, Y0, Z0, XT, YT, ZT;
	llhToEcef(lat0, lon0, alt0, X0, Y0, Z0);
	llhToEcef(latT, lonT, altT, XT, YT, ZT);

	// 3. ECEF 差值向量 (指向目标的绝对空间向量)
	double dX = XT - X0;
	double dY = YT - Y0;
	double dZ = ZT - Z0;

	// 径向距离绝对精确值
	range = std::sqrt(dX * dX + dY * dY + dZ * dZ);

	// 4. 将差值向量转换到本机所在位置的 ENU 坐标系 (东X、北Y、天Z)
	double sinLat0 = std::sin(lat0);
	double cosLat0 = std::cos(lat0);
	double sinLon0 = std::sin(lon0);
	double cosLon0 = std::cos(lon0);

	double E = -sinLon0 * dX + cosLon0 * dY;
	double N = -sinLat0 * cosLon0 * dX - sinLat0 * sinLon0 * dY + cosLat0 * dZ;
	double U = cosLat0 * cosLon0 * dX + cosLat0 * sinLon0 * dY + sinLat0 * dZ;

	LVecBase3d p_enu(E, N, U);

	// 5. 构造从 载荷相机(机体) 到 ENU 的旋转矩阵 
	// Panda3D 的 Heading 是逆时针正，协议 yaw 是顺时针正，故取反 -yaw_deg
	// Pitch 是绕 X 轴旋转，Roll 是绕 Y 轴旋转
	LMatrix3d matH = LMatrix3d::rotate_mat(-yaw_deg, LVecBase3d::unit_z());
	LMatrix3d matP = LMatrix3d::rotate_mat(pitch_deg, LVecBase3d::unit_x());
	LMatrix3d matR = LMatrix3d::rotate_mat(roll_deg, LVecBase3d::unit_y());

	// 按照 Panda3D 的行向量结合律 (v * M)，组合顺序为：先 Roll，再 Pitch，最后 Heading
	LMatrix3d matBodyToEnu = matR * matP * matH;

	// 求逆矩阵 (正交旋转矩阵的逆即为转置)，得到 ENU 到 Body 的转换矩阵
	LMatrix3d matEnuToBody = matBodyToEnu;
	matEnuToBody.invert_in_place();

	// 计算目标向量在机体/载荷局部坐标系下的表示
	// 使用 Panda3D 的 xform 方法代替乘号：相当于 p_enu * matEnuToBody
	LVecBase3d p_body = matEnuToBody.xform(p_enu);

	// 6. 提取相对于相机的偏航角（方位）和俯仰角（高度）
	// 相对偏航角：目标在水平(X-Y)面上的夹角。以正前(Y)为0°，偏右(X>0)为正
	double yaw_rel = std::atan2(p_body[0], p_body[1]);

	// 相对俯仰角：目标距水平(X-Y)面的仰角。往上(Z>0)为正
	double horiz_dist = std::sqrt(p_body[0] * p_body[0] + p_body[1] * p_body[1]);
	double pitch_rel = std::atan2(p_body[2], horiz_dist);

	// 转换为度数输出
	rel_pitch_deg = rad2deg(pitch_rel);
	rel_yaw_deg = rad2deg(yaw_rel);
}