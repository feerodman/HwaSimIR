#pragma once

#include "AnnotationConfig.h"
#include "AnnotationTypes.h"
#include "lens.h"
#include "nodePath.h"

class AnnotationProjector
{
public:
	bool buildTargetAnnotation(
		const TargetPlatformData& targetPlat,
		const AnnotationConfig& config,
		const NodePath& renderRoot,
		const NodePath& cameraNode,
		Lens* cameraLens,
		int width,
		int height,
		TargetAnnotation& outAnnotation);

	// bbox 构建统计只在标注模块内部使用，公开类型便于局部辅助函数复用。
	struct BBoxPixelAccumulator
	{
		bool hasPoint = false;
		int minX = 0;
		int minY = 0;
		int maxX = 0;
		int maxY = 0;
	};

	struct BBoxBuildStats
	{
		std::string mode;
		std::string fallback;
		int vertexCount = 0;
	};

private:
	bool projectWorldPointToPixel(
		const NodePath& renderRoot,
		const NodePath& cameraNode,
		Lens* cameraLens,
		const LPoint3f& worldPoint,
		int width,
		int height,
		int& outX,
		int& outY,
		bool& inViewport) const;

	bool getLocalBounds(const NodePath& targetNode, LPoint3f& localMin, LPoint3f& localMax) const;
	bool buildBoundingBox(
		const NodePath& targetNode,
		const NodePath& renderRoot,
		const NodePath& cameraNode,
		Lens* cameraLens,
		int width,
		int height,
		const TargetAnnotationConfig& targetConfig,
		const LPoint3f& localMin,
		const LPoint3f& localMax,
		AnnotationRect2D& outRect,
		BBoxBuildStats& stats);
	bool buildMeshBodyBoundingBox(
		const NodePath& targetNode,
		const NodePath& renderRoot,
		const NodePath& cameraNode,
		Lens* cameraLens,
		int width,
		int height,
		const TargetAnnotationConfig& targetConfig,
		AnnotationRect2D& outRect,
		BBoxBuildStats& stats);
	bool collectMeshBodyPixels(
		const NodePath& currentNode,
		bool parentExcluded,
		const NodePath& renderRoot,
		const NodePath& cameraNode,
		Lens* cameraLens,
		int width,
		int height,
		const TargetAnnotationConfig& targetConfig,
		BBoxPixelAccumulator& accumulator,
		BBoxBuildStats& stats);
	bool buildBodyTightBoundsBoundingBox(
		const NodePath& targetNode,
		const NodePath& renderRoot,
		const NodePath& cameraNode,
		Lens* cameraLens,
		int width,
		int height,
		const TargetAnnotationConfig& targetConfig,
		AnnotationRect2D& outRect,
		BBoxBuildStats& stats);
	bool collectBodyTightBounds(
		const NodePath& currentNode,
		const NodePath& targetNode,
		bool parentExcluded,
		const TargetAnnotationConfig& targetConfig,
		bool& hasBounds,
		LPoint3f& outMin,
		LPoint3f& outMax) const;
	bool buildBoundsFromLocalCorners(
		const NodePath& targetNode,
		const NodePath& renderRoot,
		const NodePath& cameraNode,
		Lens* cameraLens,
		int width,
		int height,
		const LPoint3f& localMin,
		const LPoint3f& localMax,
		const TargetAnnotationConfig& targetConfig,
		AnnotationRect2D& outRect,
		BBoxPixelAccumulator* accumulator = nullptr) const;
	bool makeRectFromAccumulator(const BBoxPixelAccumulator& accumulator, int width, int height, const TargetAnnotationConfig& targetConfig, AnnotationRect2D& outRect) const;
	bool isNodeExcluded(const NodePath& node, const TargetAnnotationConfig& targetConfig) const;

	LPoint3f estimateHeadPoint(const TargetAnnotationConfig& targetConfig, const LPoint3f& localMin, const LPoint3f& localMax) const;
	LPoint3f estimateMiddlePoint(const TargetAnnotationConfig& targetConfig, const LPoint3f& localMin, const LPoint3f& localMax) const;
	bool buildKeyPoint(
		const std::string& name,
		const LPoint3f& localPoint,
		const NodePath& targetNode,
		const NodePath& renderRoot,
		const NodePath& cameraNode,
		Lens* cameraLens,
		int width,
		int height,
		AnnotationPoint2D& outPoint);

	bool isKeyPointVisibleByRay(const NodePath& renderRoot, const LPoint3f& cameraWorldPoint, const LPoint3f& keyWorldPoint);

	int m_occlusionWarningCounter = 0;
	int m_bboxWarningCounter = 0;
	int m_bboxLogCounter = 0;
	int m_keyPointLogCounter = 0;
};
