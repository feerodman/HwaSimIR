#pragma once

#include <string>
#include <vector>

// 单个关重部位在最终显示/输出图像左上角坐标系下的像素位置。
struct AnnotationPoint2D
{
	std::string name;
	int x = 0;
	int y = 0;
	bool visible = false;
};

// 目标三维包围盒投影到最终图像后的二维外接矩形。
struct AnnotationRect2D
{
	int x = 0;
	int y = 0;
	int width = 0;
	int height = 0;
	bool visible = false;
};

// 单目标标注快照，保留三元组中的 targetPlatID 便于后续防串号校验。
struct TargetAnnotation
{
	int targetType = 0;
	int targetPlatID = 0;
	int targetID = -1;
	std::string modelLabel;
	AnnotationRect2D bbox;
	std::vector<AnnotationPoint2D> keyPoints;
};

// 单帧内存标注记录；Stage 1 不落盘、不经网络发送。
struct AnnotationFrameRecord
{
	unsigned long long frameIndex = 0;
	double simTimeMs = 0.0;
	int sensorID = 0;
	int width = 0;
	int height = 0;
	std::vector<TargetAnnotation> targets;
};

// 标注绘制开关由运行配置控制；Stage 1.2 不改变标注计算和网络传输边界。
struct AnnotationDrawOptions
{
	bool debugOverlay = false;
	bool drawKeyPoints = true;
	bool drawModelLabel = true;
	bool drawBBox = true;
};
