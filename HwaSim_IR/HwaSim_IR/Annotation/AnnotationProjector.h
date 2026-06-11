#pragma once

#include "AnnotationConfig.h"
#include "AnnotationTypes.h"
#include "collisionHandlerQueue.h"
#include "collisionNode.h"
#include "collisionRay.h"
#include "collisionTraverser.h"
#include "lens.h"
#include "nodePath.h"

#include <map>

class PandaNode;

class AnnotationProjector
{
public:
	struct PerfStats
	{
		int targets = 0;
		int keypoints = 0;
		double bboxMs = 0.0;
		double collisionBuildMs = 0.0;
		double surfaceMs = 0.0;
		double occlusionMs = 0.0;
		int collisionBuilds = 0;
		int collisionTriangles = 0;
		int collisionSolids = 0;
	};

	void beginFrame(
		unsigned long long frameIndex,
		const std::vector<TargetPlatformData>& allTargets,
		const AnnotationConfig& config,
		const NodePath& renderRoot);
	const PerfStats& perfStats() const;

	bool buildTargetAnnotation(
		const TargetPlatformData& targetPlat,
		const std::vector<TargetPlatformData>& allTargets,
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
	struct MeshCollisionStats
	{
		bool built = false;
		bool available = false;
		int triangles = 0;
		int solids = 0;
	};

	struct MeshOcclusionHitInfo
	{
		bool hit = false;
		int targetID = -1;
		bool selfHit = false;
		float hitDistanceM = 0.0f;
		float keyDistanceM = 0.0f;
	};

	struct SurfaceSnapResult
	{
		bool snapped = false;
		LPoint3f surfaceLocal = LPoint3f(0.0f, 0.0f, 0.0f);
		int vertexCount = 0;
		PandaNode* sourceNode = nullptr;
	};

	struct CollisionMeshCache
	{
		NodePath sourceNodePath;
		PandaNode* sourceNode = nullptr;
		NodePath collisionPath;
		int targetID = -1;
		int targetPlatID = 0;
		int targetType = 0;
		int triangles = 0;
		int solids = 0;
	};

	struct CollisionCandidate
	{
		std::string key;
		int targetID = -1;
		int targetPlatID = 0;
		int targetType = 0;
		NodePath collisionPath;
	};

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
		const AnnotationKeyPointConfig& pointConfig,
		const TargetPlatformData& targetPlat,
		const std::vector<TargetPlatformData>& allTargets,
		const AnnotationConfig& config,
		const NodePath& targetNode,
		const NodePath& renderRoot,
		const NodePath& cameraNode,
		Lens* cameraLens,
		int width,
		int height,
		AnnotationPoint2D& outPoint);

	bool isKeyPointVisibleByRay(
		const std::string& pointName,
		const TargetPlatformData& targetPlat,
		const std::vector<TargetPlatformData>& allTargets,
		const AnnotationConfig& config,
		const NodePath& renderRoot,
		const LPoint3f& cameraWorldPoint,
		const LPoint3f& keyWorldPoint,
		int keyPixelX,
		int keyPixelY);
	bool findOcclusionByMeshCollision(
		const TargetPlatformData& targetPlat,
		const std::vector<TargetPlatformData>& allTargets,
		const AnnotationConfig& config,
		const NodePath& renderRoot,
		const LPoint3f& cameraWorldPoint,
		const LPoint3f& keyWorldPoint,
		int keyPixelX,
		int keyPixelY,
		MeshOcclusionHitInfo& outHit);
	bool ensureCollisionMeshForTarget(
		const TargetPlatformData& targetPlat,
		const AnnotationConfig& config,
		const NodePath& renderRoot,
		NodePath& outCollisionPath,
		MeshCollisionStats& stats);
	bool collectMeshCollisionPolygons(
		const NodePath& currentNode,
		const NodePath& targetNode,
		bool parentExcluded,
		const TargetAnnotationConfig& targetConfig,
		CollisionNode* collisionNode,
		int& triangleCount) const;
	bool snapKeyPointToMeshSurface(
		const std::string& name,
		const LPoint3f& seedLocal,
		const AnnotationKeyPointConfig& pointConfig,
		const TargetPlatformData& targetPlat,
		const AnnotationConfig& config,
		SurfaceSnapResult& outResult);
	bool collectNearestSurfaceVertex(
		const NodePath& currentNode,
		const NodePath& targetNode,
		bool parentExcluded,
		const TargetAnnotationConfig& targetConfig,
		const LPoint3f& seedLocal,
		float& bestDistanceSq,
		SurfaceSnapResult& outResult) const;
	void setCollisionMeshActive(const NodePath& targetNode, const AnnotationConfig& config, bool active) const;
	void setCollisionPathActive(const NodePath& collisionPath, const AnnotationConfig& config, bool active) const;
	bool ensureCollisionRoot(const NodePath& renderRoot);
	bool ensureReusableRayCollider(const AnnotationConfig& config);
	bool keyPointMayOverlapTargetBBox(const std::string& targetKey, int keyPixelX, int keyPixelY) const;
	void registerFrameBBox(const TargetPlatformData& targetPlat, const AnnotationRect2D& bbox);

	int m_occlusionWarningCounter = 0;
	int m_occlusionLogCounter = 0;
	int m_collisionLogCounter = 0;
	int m_surfaceLogCounter = 0;
	int m_bboxWarningCounter = 0;
	int m_bboxLogCounter = 0;
	int m_keyPointLogCounter = 0;
	unsigned long long m_frameIndex = 0;
	PerfStats m_perfStats;
	NodePath m_collisionRoot;
	NodePath m_rayPath;
	PT(CollisionRay) m_reusableRay;
	PT(CollisionNode) m_reusableRayNode;
	PT(CollisionHandlerQueue) m_reusableQueue;
	CollisionTraverser m_reusableTraverser;
	std::map<std::string, CollisionMeshCache> m_collisionCache;
	std::map<std::string, SurfaceSnapResult> m_surfaceSnapCache;
	std::map<std::string, AnnotationRect2D> m_frameBBoxCache;
	std::vector<CollisionCandidate> m_collisionCandidates;
};
