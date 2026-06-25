#include "AnnotationProjector.h"

#include "bitMask.h"
#include "collisionEntry.h"
#include "collisionHandlerQueue.h"
#include "collisionNode.h"
#include "collisionPolygon.h"
#include "collisionRay.h"
#include "collisionTraverser.h"
#include "geom.h"
#include "geomNode.h"
#include "geomPrimitive.h"
#include "geomVertexData.h"
#include "geomVertexReader.h"
#include "internalName.h"
#include "nodePathCollection.h"
#include "pandaNode.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>

namespace
{
int ClampInt(int value, int low, int high)
{
	return std::max(low, std::min(high, value));
}

LPoint3f CenterOfBounds(const LPoint3f& localMin, const LPoint3f& localMax)
{
	return LPoint3f(
		(localMin.get_x() + localMax.get_x()) * 0.5f,
		(localMin.get_y() + localMax.get_y()) * 0.5f,
		(localMin.get_z() + localMax.get_z()) * 0.5f);
}

void AddPixelToAccumulator(AnnotationProjector::BBoxPixelAccumulator& accumulator, int x, int y)
{
	if (!accumulator.hasPoint)
	{
		accumulator.hasPoint = true;
		accumulator.minX = accumulator.maxX = x;
		accumulator.minY = accumulator.maxY = y;
		return;
	}

	accumulator.minX = std::min(accumulator.minX, x);
	accumulator.minY = std::min(accumulator.minY, y);
	accumulator.maxX = std::max(accumulator.maxX, x);
	accumulator.maxY = std::max(accumulator.maxY, y);
}

void MergeBounds(bool& hasBounds, LPoint3f& outMin, LPoint3f& outMax, const LPoint3f& localMin, const LPoint3f& localMax)
{
	if (!hasBounds)
	{
		hasBounds = true;
		outMin = localMin;
		outMax = localMax;
		return;
	}

	outMin.set_x(std::min(outMin.get_x(), localMin.get_x()));
	outMin.set_y(std::min(outMin.get_y(), localMin.get_y()));
	outMin.set_z(std::min(outMin.get_z(), localMin.get_z()));
	outMax.set_x(std::max(outMax.get_x(), localMax.get_x()));
	outMax.set_y(std::max(outMax.get_y(), localMax.get_y()));
	outMax.set_z(std::max(outMax.get_z(), localMax.get_z()));
}

std::string PointText(const LPoint3f& point)
{
	std::ostringstream text;
	text << "(" << point.get_x() << "," << point.get_y() << "," << point.get_z() << ")";
	return text.str();
}

std::string PixelText(const AnnotationPoint2D& point)
{
	if (!point.visible)
	{
		return "hidden";
	}
	std::ostringstream text;
	text << "(" << point.x << "," << point.y << ")";
	return text.str();
}

float Distance3(const LPoint3f& a, const LPoint3f& b)
{
	const float dx = a.get_x() - b.get_x();
	const float dy = a.get_y() - b.get_y();
	const float dz = a.get_z() - b.get_z();
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool SameProtocolTarget(const TargetPlatformData& a, const TargetPlatformData& b)
{
	return a.targetState.targetType == b.targetState.targetType &&
		a.targetState.targetPlatID == b.targetState.targetPlatID &&
		a.targetState.targetID == b.targetState.targetID;
}

std::string TargetKeyText(const TargetPlatformData& targetPlat)
{
	std::ostringstream key;
	key << targetPlat.targetState.targetType << "_"
		<< targetPlat.targetState.targetPlatID << "_"
		<< targetPlat.targetState.targetID;
	return key.str();
}

std::string CollisionNodeName()
{
	return "AnnotationCollision_Target";
}

CollideMask AnnotationCollisionMask(const AnnotationConfig& config)
{
	return BitMask32::bit(config.occlusion().collisionMaskBit);
}

float DistanceSquared3(const LPoint3f& a, const LPoint3f& b)
{
	const float dx = a.get_x() - b.get_x();
	const float dy = a.get_y() - b.get_y();
	const float dz = a.get_z() - b.get_z();
	return dx * dx + dy * dy + dz * dz;
}

double NowMs()
{
	return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

bool IsVisibleAnnotationTarget(const TargetPlatformData& targetPlat)
{
	return targetPlat.isExist && !targetPlat.nodePath.is_empty() &&
		!targetPlat.nodePath.is_hidden() && targetPlat.targetState.viewValid &&
		targetPlat.targetState.targetID >= 0;
}
}

void AnnotationProjector::beginFrame(
	unsigned long long frameIndex,
	const std::vector<TargetPlatformData>& allTargets,
	const AnnotationConfig& config,
	const NodePath& renderRoot,
	bool enableOcclusion)
{
	m_frameIndex = frameIndex;
	m_occlusionEnabledForFrame = enableOcclusion;
	m_perfStats = PerfStats();
	m_perfStats.targets = static_cast<int>(allTargets.size());
	m_collisionCandidates.clear();

	if (!m_occlusionEnabledForFrame || !config.occlusion().enabled || config.occlusion().mode != "mesh_collision" || renderRoot.is_empty())
	{
		return;
	}

	const double beginMs = NowMs();
	if (!ensureCollisionRoot(renderRoot))
	{
		m_perfStats.collisionBuildMs += NowMs() - beginMs;
		return;
	}

	for (std::map<std::string, CollisionMeshCache>::iterator it = m_collisionCache.begin(); it != m_collisionCache.end(); ++it)
	{
		setCollisionPathActive(it->second.collisionPath, config, false);
	}

	for (size_t i = 0; i < allTargets.size(); ++i)
	{
		const TargetPlatformData& targetPlat = allTargets[i];
		if (!IsVisibleAnnotationTarget(targetPlat))
		{
			continue;
		}

		NodePath collisionPath;
		MeshCollisionStats stats;
		if (!ensureCollisionMeshForTarget(targetPlat, config, renderRoot, collisionPath, stats))
		{
			++m_occlusionWarningCounter;
			if (m_occlusionWarningCounter <= 6 || (m_occlusionWarningCounter % 300) == 0)
			{
				std::cout << "[AnnotationOcclusion][WARN] mesh_collision_unavailable"
					<< " targetID=" << targetPlat.targetState.targetID
					<< " noAabbFallback=1"
					<< std::endl;
			}
			continue;
		}
		if (!stats.built)
		{
			++m_perfStats.collisionReused;
		}
		++m_collisionCacheLogCounter;
		if (stats.built || m_collisionCacheLogCounter <= 8 || (m_collisionCacheLogCounter % 120) == 0)
		{
			std::cout << "[AnnotationCollisionCache]"
				<< " platform=" << config.labelForTargetType(targetPlat.targetState.targetType)
				<< " targetID=" << targetPlat.targetState.targetID
				<< " built=" << (stats.built ? "1" : "0")
				<< " reused=" << (!stats.built ? "1" : "0")
				<< " triangles=" << stats.triangles
				<< " solids=" << stats.solids
				<< std::endl;
		}

		collisionPath.set_mat(targetPlat.nodePath.get_mat(renderRoot));
		setCollisionPathActive(collisionPath, config, false);
		CollisionCandidate candidate;
		candidate.key = TargetKeyText(targetPlat);
		candidate.targetID = targetPlat.targetState.targetID;
		candidate.targetPlatID = targetPlat.targetState.targetPlatID;
		candidate.targetType = targetPlat.targetState.targetType;
		candidate.collisionPath = collisionPath;
		m_collisionCandidates.push_back(candidate);
	}
	m_perfStats.collisionBuildMs += NowMs() - beginMs;
}

const AnnotationProjector::PerfStats& AnnotationProjector::perfStats() const
{
	return m_perfStats;
}

bool AnnotationProjector::buildTargetAnnotation(
	const TargetPlatformData& targetPlat,
	const std::vector<TargetPlatformData>& allTargets,
	const AnnotationConfig& config,
	const NodePath& renderRoot,
	const NodePath& cameraNode,
	Lens* cameraLens,
	int width,
	int height,
	TargetAnnotation& outAnnotation)
{
	if (!targetPlat.isExist || targetPlat.nodePath.is_empty() || cameraNode.is_empty() || cameraLens == nullptr ||
		width <= 0 || height <= 0 || targetPlat.targetState.targetID < 0 ||
		!targetPlat.targetState.viewValid || targetPlat.nodePath.is_hidden())
	{
		return false;
	}

	const TargetAnnotationConfig& targetConfig = config.configForPlatform(targetPlat.type);
	if (!targetConfig.enabled)
	{
		return false;
	}

	LPoint3f localMin;
	LPoint3f localMax;
	if (!getLocalBounds(targetPlat.nodePath, localMin, localMax))
	{
		return false;
	}

	BBoxBuildStats bboxStats;
	AnnotationRect2D bbox;
	const double bboxBeginMs = NowMs();
	if (!buildBoundingBox(targetPlat.nodePath, renderRoot, cameraNode, cameraLens, width, height, targetConfig, localMin, localMax, bbox, bboxStats))
	{
		m_perfStats.bboxMs += NowMs() - bboxBeginMs;
		return false;
	}
	m_perfStats.bboxMs += NowMs() - bboxBeginMs;
	registerFrameBBox(targetPlat, bbox);

	outAnnotation = TargetAnnotation();
	outAnnotation.targetType = targetPlat.targetState.targetType;
	outAnnotation.targetPlatID = targetPlat.targetState.targetPlatID;
	outAnnotation.targetID = targetPlat.targetState.targetID;
	outAnnotation.modelLabel = config.labelForTargetType(targetPlat.targetState.targetType);
	outAnnotation.bbox = bbox;

	const LPoint3f headLocal = estimateHeadPoint(targetConfig, localMin, localMax);
	const LPoint3f middleLocal = estimateMiddlePoint(targetConfig, localMin, localMax);
	AnnotationPoint2D headPoint;
	AnnotationPoint2D middlePoint;
	if (targetConfig.keyPointsEnabled && targetConfig.headPoint.visible &&
		buildKeyPoint("head", headLocal, targetConfig.headPoint, targetPlat, allTargets, config, targetPlat.nodePath, renderRoot, cameraNode, cameraLens, width, height, headPoint))
	{
		outAnnotation.keyPoints.push_back(headPoint);
	}

	if (targetConfig.keyPointsEnabled && targetConfig.middlePoint.visible &&
		buildKeyPoint("middle", middleLocal, targetConfig.middlePoint, targetPlat, allTargets, config, targetPlat.nodePath, renderRoot, cameraNode, cameraLens, width, height, middlePoint))
	{
		outAnnotation.keyPoints.push_back(middlePoint);
	}

	++m_bboxLogCounter;
	if (m_bboxLogCounter <= 6 || (m_bboxLogCounter % 120) == 0)
	{
		std::cout << "[AnnotationBBox]"
			<< " targetID=" << targetPlat.targetState.targetID
			<< " platform=" << outAnnotation.modelLabel
			<< " mode=" << bboxStats.mode
			<< " vertexCount=" << bboxStats.vertexCount
			<< " bbox=(" << bbox.x << "," << bbox.y << "," << bbox.width << "," << bbox.height << ")"
			<< std::endl;
	}
	if (!bboxStats.fallback.empty())
	{
		++m_bboxWarningCounter;
		if (m_bboxWarningCounter <= 6 || (m_bboxWarningCounter % 120) == 0)
		{
			std::cout << "[AnnotationBBox][WARN]"
				<< " targetID=" << targetPlat.targetState.targetID
				<< " platform=" << outAnnotation.modelLabel
				<< " fallback=" << bboxStats.fallback
				<< std::endl;
		}
	}

	++m_keyPointLogCounter;
	if (m_keyPointLogCounter <= 6 || (m_keyPointLogCounter % 120) == 0)
	{
		std::cout << "[AnnotationKeyPoint]"
			<< " targetID=" << targetPlat.targetState.targetID
			<< " platform=" << outAnnotation.modelLabel
			<< " headLocal=" << PointText(headLocal)
			<< " middleLocal=" << PointText(middleLocal)
			<< " headPixel=" << PixelText(headPoint)
			<< " middlePixel=" << PixelText(middlePoint)
			<< std::endl;
	}

	return true;
}

bool AnnotationProjector::projectWorldPointToPixel(
	const NodePath& renderRoot,
	const NodePath& cameraNode,
	Lens* cameraLens,
	const LPoint3f& worldPoint,
	int width,
	int height,
	int& outX,
	int& outY,
	bool& inViewport) const
{
	if (cameraLens == nullptr || cameraNode.is_empty() || width <= 0 || height <= 0)
	{
		return false;
	}

	// Panda3D Lens::project 需要相机坐标系下的点；输出坐标再换成最终图像左上角像素坐标。
	LPoint3f cameraPoint = cameraNode.get_relative_point(renderRoot, worldPoint);
	LPoint2f ndc;
	if (!cameraLens->project(cameraPoint, ndc))
	{
		return false;
	}

	const float pixelX = (ndc.get_x() + 1.0f) * 0.5f * static_cast<float>(width);
	const float pixelY = (1.0f - ndc.get_y()) * 0.5f * static_cast<float>(height);
	outX = static_cast<int>(std::floor(pixelX + 0.5f));
	outY = static_cast<int>(std::floor(pixelY + 0.5f));
	inViewport = outX >= 0 && outX < width && outY >= 0 && outY < height;
	return true;
}

bool AnnotationProjector::getLocalBounds(const NodePath& targetNode, LPoint3f& localMin, LPoint3f& localMax) const
{
	if (targetNode.is_empty())
	{
		return false;
	}

	if (!targetNode.calc_tight_bounds(localMin, localMax, targetNode))
	{
		return false;
	}

	const float dx = std::fabs(localMax.get_x() - localMin.get_x());
	const float dy = std::fabs(localMax.get_y() - localMin.get_y());
	const float dz = std::fabs(localMax.get_z() - localMin.get_z());
	return dx > 0.0001f || dy > 0.0001f || dz > 0.0001f;
}

bool AnnotationProjector::buildBoundingBox(
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
	BBoxBuildStats& stats)
{
	stats.mode = targetConfig.bbox.mode.empty() ? "mesh_body" : targetConfig.bbox.mode;
	if (stats.mode == "mesh_body" &&
		buildMeshBodyBoundingBox(targetNode, renderRoot, cameraNode, cameraLens, width, height, targetConfig, outRect, stats))
	{
		return true;
	}

	if (stats.mode == "mesh_body" &&
		buildBodyTightBoundsBoundingBox(targetNode, renderRoot, cameraNode, cameraLens, width, height, targetConfig, outRect, stats))
	{
		if (stats.fallback.empty())
		{
			stats.fallback = "body_tight_bounds";
		}
		return true;
	}

	stats.fallback = stats.fallback.empty() ? "legacy_tight_bounds" : stats.fallback + "+legacy_tight_bounds";
	stats.mode = "legacy_tight_bounds";
	return buildBoundsFromLocalCorners(targetNode, renderRoot, cameraNode, cameraLens, width, height, localMin, localMax, targetConfig, outRect);
}

bool AnnotationProjector::buildMeshBodyBoundingBox(
	const NodePath& targetNode,
	const NodePath& renderRoot,
	const NodePath& cameraNode,
	Lens* cameraLens,
	int width,
	int height,
	const TargetAnnotationConfig& targetConfig,
	AnnotationRect2D& outRect,
	BBoxBuildStats& stats)
{
	BBoxPixelAccumulator accumulator;
	if (!collectMeshBodyPixels(targetNode, false, renderRoot, cameraNode, cameraLens, width, height, targetConfig, accumulator, stats))
	{
		stats.fallback = "mesh_body_no_vertices";
		return false;
	}

	if (!makeRectFromAccumulator(accumulator, width, height, targetConfig, outRect))
	{
		stats.fallback = "mesh_body_outside_or_small";
		return false;
	}

	stats.mode = "mesh_body";
	return true;
}

bool AnnotationProjector::collectMeshBodyPixels(
	const NodePath& currentNode,
	bool parentExcluded,
	const NodePath& renderRoot,
	const NodePath& cameraNode,
	Lens* cameraLens,
	int width,
	int height,
	const TargetAnnotationConfig& targetConfig,
	BBoxPixelAccumulator& accumulator,
	BBoxBuildStats& stats)
{
	if (currentNode.is_empty())
	{
		return false;
	}

	const bool excluded = parentExcluded || isNodeExcluded(currentNode, targetConfig);
	if (excluded)
	{
		return false;
	}

	bool foundVertex = false;
	PandaNode* pandaNode = currentNode.node();
	if (pandaNode->is_of_type(GeomNode::get_class_type()))
	{
		GeomNode* geomNode = DCAST(GeomNode, pandaNode);
		for (int geomIndex = 0; geomIndex < geomNode->get_num_geoms(); ++geomIndex)
		{
			CPT(Geom) geom = geomNode->get_geom(geomIndex);
			if (geom == nullptr)
			{
				continue;
			}
			CPT(GeomVertexData) vertexData = geom->get_vertex_data();
			if (vertexData == nullptr)
			{
				continue;
			}

			GeomVertexReader reader(vertexData, InternalName::get_vertex());
			if (!reader.has_column())
			{
				continue;
			}
			while (!reader.is_at_end())
			{
				const LPoint3f localVertex = reader.get_data3f();
				const LPoint3f worldPoint = renderRoot.get_relative_point(currentNode, localVertex);
				int x = 0;
				int y = 0;
				bool inViewport = false;
				if (projectWorldPointToPixel(renderRoot, cameraNode, cameraLens, worldPoint, width, height, x, y, inViewport))
				{
					AddPixelToAccumulator(accumulator, x, y);
					foundVertex = true;
				}
				++stats.vertexCount;
			}
		}
	}

	NodePathCollection children = currentNode.get_children();
	for (int i = 0; i < children.get_num_paths(); ++i)
	{
		if (collectMeshBodyPixels(children.get_path(i), excluded, renderRoot, cameraNode, cameraLens, width, height, targetConfig, accumulator, stats))
		{
			foundVertex = true;
		}
	}
	return foundVertex;
}

bool AnnotationProjector::buildBodyTightBoundsBoundingBox(
	const NodePath& targetNode,
	const NodePath& renderRoot,
	const NodePath& cameraNode,
	Lens* cameraLens,
	int width,
	int height,
	const TargetAnnotationConfig& targetConfig,
	AnnotationRect2D& outRect,
	BBoxBuildStats& stats)
{
	bool hasBounds = false;
	LPoint3f localMin;
	LPoint3f localMax;
	if (!collectBodyTightBounds(targetNode, targetNode, false, targetConfig, hasBounds, localMin, localMax) || !hasBounds)
	{
		stats.fallback = stats.fallback.empty() ? "body_tight_bounds_failed" : stats.fallback + "+body_tight_bounds_failed";
		return false;
	}

	stats.mode = "body_tight_bounds";
	return buildBoundsFromLocalCorners(targetNode, renderRoot, cameraNode, cameraLens, width, height, localMin, localMax, targetConfig, outRect);
}

bool AnnotationProjector::collectBodyTightBounds(
	const NodePath& currentNode,
	const NodePath& targetNode,
	bool parentExcluded,
	const TargetAnnotationConfig& targetConfig,
	bool& hasBounds,
	LPoint3f& outMin,
	LPoint3f& outMax) const
{
	if (currentNode.is_empty())
	{
		return false;
	}

	const bool excluded = parentExcluded || isNodeExcluded(currentNode, targetConfig);
	if (excluded)
	{
		return false;
	}

	bool foundBounds = false;
	PandaNode* pandaNode = currentNode.node();
	if (pandaNode->is_of_type(GeomNode::get_class_type()))
	{
		LPoint3f localMin;
		LPoint3f localMax;
		if (currentNode.calc_tight_bounds(localMin, localMax, targetNode))
		{
			MergeBounds(hasBounds, outMin, outMax, localMin, localMax);
			foundBounds = true;
		}
	}

	NodePathCollection children = currentNode.get_children();
	for (int i = 0; i < children.get_num_paths(); ++i)
	{
		if (collectBodyTightBounds(children.get_path(i), targetNode, excluded, targetConfig, hasBounds, outMin, outMax))
		{
			foundBounds = true;
		}
	}
	return foundBounds;
}

bool AnnotationProjector::buildBoundsFromLocalCorners(
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
	BBoxPixelAccumulator* accumulator) const
{
	BBoxPixelAccumulator localAccumulator;
	BBoxPixelAccumulator& activeAccumulator = accumulator ? *accumulator : localAccumulator;
	const LPoint3f corners[8] = {
		LPoint3f(localMin.get_x(), localMin.get_y(), localMin.get_z()),
		LPoint3f(localMax.get_x(), localMin.get_y(), localMin.get_z()),
		LPoint3f(localMin.get_x(), localMax.get_y(), localMin.get_z()),
		LPoint3f(localMax.get_x(), localMax.get_y(), localMin.get_z()),
		LPoint3f(localMin.get_x(), localMin.get_y(), localMax.get_z()),
		LPoint3f(localMax.get_x(), localMin.get_y(), localMax.get_z()),
		LPoint3f(localMin.get_x(), localMax.get_y(), localMax.get_z()),
		LPoint3f(localMax.get_x(), localMax.get_y(), localMax.get_z())
	};

	for (int i = 0; i < 8; ++i)
	{
		const LPoint3f worldPoint = renderRoot.get_relative_point(targetNode, corners[i]);
		int x = 0;
		int y = 0;
		bool inViewport = false;
		if (!projectWorldPointToPixel(renderRoot, cameraNode, cameraLens, worldPoint, width, height, x, y, inViewport))
		{
			continue;
		}
		AddPixelToAccumulator(activeAccumulator, x, y);
	}

	return makeRectFromAccumulator(activeAccumulator, width, height, targetConfig, outRect);
}

bool AnnotationProjector::makeRectFromAccumulator(const BBoxPixelAccumulator& accumulator, int width, int height, const TargetAnnotationConfig& targetConfig, AnnotationRect2D& outRect) const
{
	if (!accumulator.hasPoint || width <= 0 || height <= 0)
	{
		return false;
	}

	const int margin = std::max(0, targetConfig.bbox.marginPx);
	const int expandedMinX = accumulator.minX - margin;
	const int expandedMinY = accumulator.minY - margin;
	const int expandedMaxX = accumulator.maxX + margin;
	const int expandedMaxY = accumulator.maxY + margin;
	if (expandedMaxX < 0 || expandedMaxY < 0 || expandedMinX >= width || expandedMinY >= height)
	{
		return false;
	}

	const int clippedMinX = ClampInt(expandedMinX, 0, width - 1);
	const int clippedMinY = ClampInt(expandedMinY, 0, height - 1);
	const int clippedMaxX = ClampInt(expandedMaxX, 0, width - 1);
	const int clippedMaxY = ClampInt(expandedMaxY, 0, height - 1);
	const int rectWidth = clippedMaxX - clippedMinX + 1;
	const int rectHeight = clippedMaxY - clippedMinY + 1;
	const int minSize = std::max(1, targetConfig.bbox.minSizePx);
	if (rectWidth < minSize || rectHeight < minSize)
	{
		return false;
	}

	outRect.x = clippedMinX;
	outRect.y = clippedMinY;
	outRect.width = rectWidth;
	outRect.height = rectHeight;
	outRect.visible = true;
	return true;
}

bool AnnotationProjector::isNodeExcluded(const NodePath& node, const TargetAnnotationConfig& targetConfig) const
{
	if (node.is_empty() || node.is_hidden())
	{
		return true;
	}

	const std::string name = node.get_name();
	for (size_t i = 0; i < targetConfig.bbox.excludeNodeNameContains.size(); ++i)
	{
		const std::string& needle = targetConfig.bbox.excludeNodeNameContains[i];
		if (!needle.empty() && name.find(needle) != std::string::npos)
		{
			return true;
		}
	}
	return false;
}

LPoint3f AnnotationProjector::estimateHeadPoint(const TargetAnnotationConfig& targetConfig, const LPoint3f& localMin, const LPoint3f& localMax) const
{
	if (targetConfig.headPoint.hasLocalPos)
	{
		return targetConfig.headPoint.localPos;
	}

	LPoint3f head = CenterOfBounds(localMin, localMax);
	switch (targetConfig.forwardAxis)
	{
	case AnnotationAxisPositiveX: head.set_x(localMax.get_x()); break;
	case AnnotationAxisNegativeX: head.set_x(localMin.get_x()); break;
	case AnnotationAxisPositiveY: head.set_y(localMax.get_y()); break;
	case AnnotationAxisNegativeY: head.set_y(localMin.get_y()); break;
	case AnnotationAxisPositiveZ: head.set_z(localMax.get_z()); break;
	case AnnotationAxisNegativeZ: head.set_z(localMin.get_z()); break;
	default: head.set_y(localMax.get_y()); break;
	}
	return head;
}

LPoint3f AnnotationProjector::estimateMiddlePoint(const TargetAnnotationConfig& targetConfig, const LPoint3f& localMin, const LPoint3f& localMax) const
{
	if (targetConfig.middlePoint.hasLocalPos)
	{
		return targetConfig.middlePoint.localPos;
	}
	return CenterOfBounds(localMin, localMax);
}

bool AnnotationProjector::buildKeyPoint(
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
	AnnotationPoint2D& outPoint)
{
	LPoint3f keyLocalPoint = localPoint;
	const bool useSurfacePoint = config.surfaceKeyPointEnabled() && pointConfig.surface;
	const bool useNearestSnap = useSurfacePoint && pointConfig.snapToMeshSurface &&
		config.surfaceSnapEnabled() && config.surfaceSnapMode() != "profile_surface";
	if (useNearestSnap)
	{
		const double surfaceBeginMs = NowMs();
		SurfaceSnapResult snapResult;
		if (!snapKeyPointToMeshSurface(name, localPoint, pointConfig, targetPlat, config, snapResult))
		{
			m_perfStats.surfaceMs += NowMs() - surfaceBeginMs;
			++m_occlusionWarningCounter;
			if (m_occlusionWarningCounter <= 6 || (m_occlusionWarningCounter % 300) == 0)
			{
				std::cout << "[AnnotationSurfacePoint][WARN]"
					<< " targetID=" << targetPlat.targetState.targetID
					<< " point=" << name
					<< " seedLocal=" << PointText(localPoint)
					<< " snapped=0"
					<< " reason=mesh_surface_unavailable"
					<< std::endl;
			}
		}
		else
		{
			keyLocalPoint = snapResult.surfaceLocal;
			m_perfStats.surfaceMs += NowMs() - surfaceBeginMs;
			++m_surfaceLogCounter;
			if (m_frameIndex <= 3 || (m_frameIndex % 120) == 0)
			{
				std::cout << "[AnnotationSurfacePoint]"
					<< " targetID=" << targetPlat.targetState.targetID
					<< " point=" << name
					<< " mode=nearest_mesh_vertex"
					<< " seedLocal=" << PointText(localPoint)
					<< " surfaceLocal=" << PointText(keyLocalPoint)
					<< " snapped=1"
					<< " vertexCount=" << snapResult.vertexCount
					<< std::endl;
			}
		}
	}

	const LPoint3f worldPoint = renderRoot.get_relative_point(targetNode, keyLocalPoint);
	int x = 0;
	int y = 0;
	bool inViewport = false;
	if (!projectWorldPointToPixel(renderRoot, cameraNode, cameraLens, worldPoint, width, height, x, y, inViewport) || !inViewport)
	{
		return false;
	}

	if (useSurfacePoint && !useNearestSnap)
	{
		++m_surfaceLogCounter;
		if (m_frameIndex <= 3 || (m_frameIndex % 120) == 0)
		{
			std::cout << "[AnnotationSurfacePoint]"
				<< " targetID=" << targetPlat.targetState.targetID
				<< " point=" << name
				<< " mode=profile_surface"
				<< " local=" << PointText(keyLocalPoint)
				<< " pixel=(" << x << "," << y << ")"
				<< std::endl;
		}
	}

	const LPoint3f cameraWorldPoint = renderRoot.get_relative_point(cameraNode, LPoint3f(0.0f, 0.0f, 0.0f));
	if (!isKeyPointVisibleByRay(name, targetPlat, allTargets, config, renderRoot, cameraWorldPoint, worldPoint, x, y))
	{
		return false;
	}

	++m_perfStats.keypoints;
	outPoint.name = name;
	outPoint.x = x;
	outPoint.y = y;
	outPoint.visible = true;
	return true;
}

bool AnnotationProjector::isKeyPointVisibleByRay(
	const std::string& pointName,
	const TargetPlatformData& targetPlat,
	const std::vector<TargetPlatformData>& allTargets,
	const AnnotationConfig& config,
	const NodePath& renderRoot,
	const LPoint3f& cameraWorldPoint,
	const LPoint3f& keyWorldPoint,
	int keyPixelX,
	int keyPixelY)
{
	const AnnotationOcclusionConfig& occlusion = config.occlusion();
	if (!m_occlusionEnabledForFrame || !occlusion.enabled || occlusion.mode == "projection_only")
	{
		return true;
	}
	if (occlusion.mode != "mesh_collision")
	{
		++m_occlusionWarningCounter;
		if (m_occlusionWarningCounter <= 3 || (m_occlusionWarningCounter % 300) == 0)
		{
			std::cout << "[AnnotationOcclusion][WARN]"
				<< " targetID=" << targetPlat.targetState.targetID
				<< " point=" << pointName
				<< " mode=" << occlusion.mode
				<< " fallback=projection_only"
				<< " reason=unsupported_mode"
				<< " noAabbFallback=1"
				<< std::endl;
		}
		return true;
	}

	MeshOcclusionHitInfo hitInfo;
	const double occlusionBeginMs = NowMs();
	const bool collisionReady = findOcclusionByMeshCollision(targetPlat, allTargets, config, renderRoot, cameraWorldPoint, keyWorldPoint, keyPixelX, keyPixelY, hitInfo);
	m_perfStats.occlusionMs += NowMs() - occlusionBeginMs;
	if (!collisionReady)
	{
		// mesh collision 不可用时不使用 AABB 兜底，避免把近似包围盒误判成真实遮挡。
		++m_occlusionWarningCounter;
		if (m_occlusionWarningCounter <= 6 || (m_occlusionWarningCounter % 300) == 0)
		{
			std::cout << "[AnnotationOcclusion][WARN] mesh_collision_unavailable"
				<< " targetID=" << targetPlat.targetState.targetID
				<< " point=" << pointName
				<< " fallback=projection_only"
				<< " noAabbFallback=1"
				<< std::endl;
		}
		return true;
	}

	const bool visible = !hitInfo.hit;
	++m_occlusionLogCounter;
	if (m_frameIndex <= 3 || (m_frameIndex % 120) == 0)
	{
		std::cout << "[AnnotationOcclusion]"
			<< " targetID=" << targetPlat.targetState.targetID
			<< " point=" << pointName
			<< " visible=" << (visible ? "1" : "0")
			<< " reason=" << (visible ? "no_hit" : "mesh_hit")
			<< " mode=mesh_collision"
			<< " hit=" << (hitInfo.hit ? "1" : "0")
			<< " hitTargetID=" << hitInfo.targetID
			<< " selfHit=" << (hitInfo.selfHit ? "1" : "0")
			<< " hitDistance=" << hitInfo.hitDistanceM
			<< " keyDistance=" << hitInfo.keyDistanceM
			<< std::endl;
	}
	return visible;
}

bool AnnotationProjector::findOcclusionByMeshCollision(
	const TargetPlatformData& targetPlat,
	const std::vector<TargetPlatformData>& allTargets,
	const AnnotationConfig& config,
	const NodePath& renderRoot,
	const LPoint3f& cameraWorldPoint,
	const LPoint3f& keyWorldPoint,
	int keyPixelX,
	int keyPixelY,
	MeshOcclusionHitInfo& outHit)
{
	outHit = MeshOcclusionHitInfo();
	const float keyDistance = Distance3(cameraWorldPoint, keyWorldPoint);
	outHit.keyDistanceM = keyDistance;
	if (keyDistance <= 0.0001f)
	{
		return false;
	}
	const float epsilon = std::max(0.0f, config.occlusion().epsilonM);
	(void)allTargets;
	if (m_collisionCandidates.empty() || !ensureReusableRayCollider(config))
	{
		return false;
	}

	const std::string selfKey = TargetKeyText(targetPlat);
	bool anyActiveCandidate = false;
	for (size_t i = 0; i < m_collisionCandidates.size(); ++i)
	{
		const CollisionCandidate& candidate = m_collisionCandidates[i];
		const bool selfCandidate = candidate.key == selfKey;
		const bool active = (!selfCandidate || config.occlusion().selfTarget) &&
			keyPointMayOverlapTargetBBox(candidate.key, keyPixelX, keyPixelY);
		setCollisionPathActive(candidate.collisionPath, config, active);
		anyActiveCandidate = anyActiveCandidate || active;
	}
	if (!anyActiveCandidate)
	{
		outHit.hit = false;
		outHit.targetID = -1;
		outHit.hitDistanceM = keyDistance;
		return true;
	}

	LVector3f direction = keyWorldPoint - cameraWorldPoint;
	direction.normalize();
	m_reusableRay->set_origin(cameraWorldPoint);
	m_reusableRay->set_direction(direction);
	m_reusableQueue->clear_entries();
	m_reusableTraverser.traverse(m_collisionRoot);
	m_reusableQueue->sort_entries();

	float nearestHitDistance = std::numeric_limits<float>::max();
	int nearestTargetID = -1;
	bool nearestSelfHit = false;
	for (int i = 0; i < m_reusableQueue->get_num_entries(); ++i)
	{
		CollisionEntry* entry = m_reusableQueue->get_entry(i);
		if (entry == nullptr || !entry->has_surface_point())
		{
			continue;
		}
		const LPoint3f surfacePoint = entry->get_surface_point(renderRoot);
		const float hitDistance = Distance3(cameraWorldPoint, surfacePoint);
		if (hitDistance <= epsilon || hitDistance >= keyDistance - epsilon)
		{
			continue;
		}
		NodePath intoPath = entry->get_into_node_path();
		const bool selfHit = !intoPath.is_empty() && intoPath.get_tag("AnnotationCollisionKey") == selfKey;
		if (selfHit && !config.occlusion().selfTarget)
		{
			continue;
		}
		if (hitDistance < nearestHitDistance)
		{
			nearestHitDistance = hitDistance;
			nearestTargetID = -1;
			nearestSelfHit = selfHit;
			if (!intoPath.is_empty())
			{
				const std::string targetIDText = intoPath.get_tag("AnnotationTargetID");
				if (!targetIDText.empty())
				{
					nearestTargetID = std::atoi(targetIDText.c_str());
				}
			}
		}
	}

	if (nearestHitDistance < std::numeric_limits<float>::max())
	{
		outHit.hit = true;
		outHit.targetID = nearestTargetID;
		outHit.selfHit = nearestSelfHit;
		outHit.hitDistanceM = nearestHitDistance;
	}
	else
	{
		outHit.hit = false;
		outHit.targetID = -1;
		outHit.hitDistanceM = keyDistance;
	}
	return true;
}

bool AnnotationProjector::ensureCollisionMeshForTarget(
	const TargetPlatformData& targetPlat,
	const AnnotationConfig& config,
	const NodePath& renderRoot,
	NodePath& outCollisionPath,
	MeshCollisionStats& stats)
{
	outCollisionPath = NodePath();
	stats = MeshCollisionStats();
	if (!targetPlat.isExist || targetPlat.nodePath.is_empty() || m_collisionRoot.is_empty() || renderRoot.is_empty())
	{
		return false;
	}

	const std::string key = TargetKeyText(targetPlat);
	std::map<std::string, CollisionMeshCache>::iterator cached = m_collisionCache.find(key);
	if (cached != m_collisionCache.end() && cached->second.sourceNode == targetPlat.nodePath.node() &&
		!cached->second.collisionPath.is_empty())
	{
		outCollisionPath = cached->second.collisionPath;
		PandaNode* node = outCollisionPath.node();
		if (node != nullptr && node->is_of_type(CollisionNode::get_class_type()))
		{
			CollisionNode* collisionNode = DCAST(CollisionNode, node);
			stats.available = collisionNode->get_num_solids() > 0;
			stats.solids = static_cast<int>(collisionNode->get_num_solids());
			stats.triangles = cached->second.triangles;
			outCollisionPath.set_mat(targetPlat.nodePath.get_mat(renderRoot));
			return stats.available;
		}
		return false;
	}
	if (cached != m_collisionCache.end())
	{
		if (!cached->second.collisionPath.is_empty())
		{
			cached->second.collisionPath.remove_node();
		}
		m_collisionCache.erase(cached);
	}

	PT(CollisionNode) collisionNode = new CollisionNode(CollisionNodeName());
	collisionNode->set_from_collide_mask(BitMask32::all_off());
	collisionNode->set_into_collide_mask(AnnotationCollisionMask(config));
	const TargetAnnotationConfig& targetConfig = config.configForPlatform(targetPlat.type);
	int triangleCount = 0;
	collectMeshCollisionPolygons(targetPlat.nodePath, targetPlat.nodePath, false, targetConfig, collisionNode, triangleCount);
	if (triangleCount <= 0 || collisionNode->get_num_solids() == 0)
	{
		return false;
	}

	outCollisionPath = m_collisionRoot.attach_new_node(collisionNode);
	outCollisionPath.set_mat(targetPlat.nodePath.get_mat(renderRoot));
	outCollisionPath.set_tag("AnnotationCollisionKey", key);
	outCollisionPath.set_tag("AnnotationTargetID", std::to_string(targetPlat.targetState.targetID));
	outCollisionPath.set_tag("AnnotationTargetPlatID", std::to_string(targetPlat.targetState.targetPlatID));
	outCollisionPath.set_tag("AnnotationTargetType", std::to_string(targetPlat.targetState.targetType));
	CollisionMeshCache newCache;
	newCache.sourceNodePath = targetPlat.nodePath;
	newCache.sourceNode = targetPlat.nodePath.node();
	newCache.collisionPath = outCollisionPath;
	newCache.targetID = targetPlat.targetState.targetID;
	newCache.targetPlatID = targetPlat.targetState.targetPlatID;
	newCache.targetType = targetPlat.targetState.targetType;
	newCache.triangles = triangleCount;
	newCache.solids = static_cast<int>(collisionNode->get_num_solids());
	m_collisionCache[key] = newCache;
	stats.built = true;
	stats.available = true;
	stats.triangles = triangleCount;
	stats.solids = static_cast<int>(collisionNode->get_num_solids());
	++m_perfStats.collisionBuilds;
	m_perfStats.collisionTriangles += stats.triangles;
	m_perfStats.collisionSolids += stats.solids;

	++m_collisionLogCounter;
	if (m_collisionLogCounter <= 8 || (m_collisionLogCounter % 120) == 0)
	{
		std::cout << "[AnnotationCollision]"
			<< " targetID=" << targetPlat.targetState.targetID
			<< " platform=" << config.labelForTargetType(targetPlat.targetState.targetType)
			<< " triangles=" << stats.triangles
			<< " solids=" << stats.solids
			<< " built=1"
			<< std::endl;
	}
	if (stats.triangles > 10000)
	{
		std::cout << "[AnnotationCollision][WARN]"
			<< " highTriangleCount"
			<< " targetID=" << targetPlat.targetState.targetID
			<< " triangles=" << stats.triangles
			<< std::endl;
	}
	return true;
}

bool AnnotationProjector::collectMeshCollisionPolygons(
	const NodePath& currentNode,
	const NodePath& targetNode,
	bool parentExcluded,
	const TargetAnnotationConfig& targetConfig,
	CollisionNode* collisionNode,
	int& triangleCount) const
{
	if (currentNode.is_empty() || collisionNode == nullptr)
	{
		return false;
	}

	const bool excluded = parentExcluded || isNodeExcluded(currentNode, targetConfig);
	if (excluded)
	{
		return false;
	}

	bool foundTriangle = false;
	PandaNode* pandaNode = currentNode.node();
	if (pandaNode->is_of_type(GeomNode::get_class_type()))
	{
		GeomNode* geomNode = DCAST(GeomNode, pandaNode);
		for (int geomIndex = 0; geomIndex < geomNode->get_num_geoms(); ++geomIndex)
		{
			CPT(Geom) geom = geomNode->get_geom(geomIndex);
			if (geom == nullptr)
			{
				continue;
			}
			CPT(Geom) decomposedGeom = geom->decompose();
			if (decomposedGeom == nullptr)
			{
				continue;
			}
			CPT(GeomVertexData) vertexData = decomposedGeom->get_vertex_data();
			if (vertexData == nullptr)
			{
				continue;
			}
			GeomVertexReader reader(vertexData, InternalName::get_vertex());
			if (!reader.has_column())
			{
				continue;
			}

			for (int primitiveIndex = 0; primitiveIndex < decomposedGeom->get_num_primitives(); ++primitiveIndex)
			{
				CPT(GeomPrimitive) primitive = decomposedGeom->get_primitive(primitiveIndex);
				if (primitive == nullptr)
				{
					continue;
				}
				for (int vertexIndex = 0; vertexIndex + 2 < primitive->get_num_vertices(); vertexIndex += 3)
				{
					reader.set_row(primitive->get_vertex(vertexIndex));
					const LPoint3f p0 = targetNode.get_relative_point(currentNode, reader.get_data3f());
					reader.set_row(primitive->get_vertex(vertexIndex + 1));
					const LPoint3f p1 = targetNode.get_relative_point(currentNode, reader.get_data3f());
					reader.set_row(primitive->get_vertex(vertexIndex + 2));
					const LPoint3f p2 = targetNode.get_relative_point(currentNode, reader.get_data3f());
					if (!CollisionPolygon::verify_points(p0, p1, p2))
					{
						continue;
					}
					PT(CollisionPolygon) polygon = new CollisionPolygon(p0, p1, p2);
					collisionNode->add_solid(polygon);
					++triangleCount;
					foundTriangle = true;
				}
			}
		}
	}

	NodePathCollection children = currentNode.get_children();
	for (int i = 0; i < children.get_num_paths(); ++i)
	{
		if (collectMeshCollisionPolygons(children.get_path(i), targetNode, excluded, targetConfig, collisionNode, triangleCount))
		{
			foundTriangle = true;
		}
	}
	return foundTriangle;
}

bool AnnotationProjector::snapKeyPointToMeshSurface(
	const std::string& name,
	const LPoint3f& seedLocal,
	const AnnotationKeyPointConfig& pointConfig,
	const TargetPlatformData& targetPlat,
	const AnnotationConfig& config,
	SurfaceSnapResult& outResult)
{
	(void)name;
	outResult = SurfaceSnapResult();
	if (!pointConfig.surfaceSearchMode.empty() && pointConfig.surfaceSearchMode != "nearest_mesh_vertex" && pointConfig.surfaceSearchMode != "axis_surface")
	{
		++m_occlusionWarningCounter;
		if (m_occlusionWarningCounter <= 6 || (m_occlusionWarningCounter % 300) == 0)
		{
			std::cout << "[AnnotationSurfacePoint][WARN]"
				<< " targetID=" << targetPlat.targetState.targetID
				<< " mode=" << pointConfig.surfaceSearchMode
				<< " fallback=nearest_mesh_vertex"
				<< std::endl;
		}
	}
	if (targetPlat.nodePath.is_empty())
	{
		return false;
	}
	const std::string cacheKey = TargetKeyText(targetPlat) + "_" + name;
	PandaNode* sourceNode = targetPlat.nodePath.node();
	std::map<std::string, SurfaceSnapResult>::const_iterator cached = m_surfaceSnapCache.find(cacheKey);
	if (cached != m_surfaceSnapCache.end() && cached->second.sourceNode == sourceNode)
	{
		outResult = cached->second;
		return outResult.snapped;
	}

	const TargetAnnotationConfig& targetConfig = config.configForPlatform(targetPlat.type);
	float bestDistanceSq = std::numeric_limits<float>::max();
	const bool found = collectNearestSurfaceVertex(targetPlat.nodePath, targetPlat.nodePath, false, targetConfig, seedLocal, bestDistanceSq, outResult) && outResult.snapped;
	outResult.sourceNode = sourceNode;
	m_surfaceSnapCache[cacheKey] = outResult;
	return found;
}

bool AnnotationProjector::collectNearestSurfaceVertex(
	const NodePath& currentNode,
	const NodePath& targetNode,
	bool parentExcluded,
	const TargetAnnotationConfig& targetConfig,
	const LPoint3f& seedLocal,
	float& bestDistanceSq,
	SurfaceSnapResult& outResult) const
{
	if (currentNode.is_empty())
	{
		return false;
	}

	const bool excluded = parentExcluded || isNodeExcluded(currentNode, targetConfig);
	if (excluded)
	{
		return false;
	}

	bool foundVertex = false;
	PandaNode* pandaNode = currentNode.node();
	if (pandaNode->is_of_type(GeomNode::get_class_type()))
	{
		GeomNode* geomNode = DCAST(GeomNode, pandaNode);
		for (int geomIndex = 0; geomIndex < geomNode->get_num_geoms(); ++geomIndex)
		{
			CPT(Geom) geom = geomNode->get_geom(geomIndex);
			if (geom == nullptr)
			{
				continue;
			}
			CPT(GeomVertexData) vertexData = geom->get_vertex_data();
			if (vertexData == nullptr)
			{
				continue;
			}
			GeomVertexReader reader(vertexData, InternalName::get_vertex());
			if (!reader.has_column())
			{
				continue;
			}
			while (!reader.is_at_end())
			{
				const LPoint3f localVertex = targetNode.get_relative_point(currentNode, reader.get_data3f());
				const float distanceSq = DistanceSquared3(seedLocal, localVertex);
				++outResult.vertexCount;
				if (distanceSq < bestDistanceSq)
				{
					bestDistanceSq = distanceSq;
					outResult.surfaceLocal = localVertex;
					outResult.snapped = true;
				}
				foundVertex = true;
			}
		}
	}

	NodePathCollection children = currentNode.get_children();
	for (int i = 0; i < children.get_num_paths(); ++i)
	{
		if (collectNearestSurfaceVertex(children.get_path(i), targetNode, excluded, targetConfig, seedLocal, bestDistanceSq, outResult))
		{
			foundVertex = true;
		}
	}
	return foundVertex;
}

void AnnotationProjector::setCollisionMeshActive(const NodePath& targetNode, const AnnotationConfig& config, bool active) const
{
	if (targetNode.is_empty())
	{
		return;
	}
	NodePath collisionPath = targetNode.find("**/" + CollisionNodeName());
	if (collisionPath.is_empty())
	{
		return;
	}
	PandaNode* node = collisionPath.node();
	if (node != nullptr && node->is_of_type(CollisionNode::get_class_type()))
	{
		CollisionNode* collisionNode = DCAST(CollisionNode, node);
		collisionNode->set_into_collide_mask(active ? AnnotationCollisionMask(config) : BitMask32::all_off());
	}
}

void AnnotationProjector::setCollisionPathActive(const NodePath& collisionPath, const AnnotationConfig& config, bool active) const
{
	if (collisionPath.is_empty())
	{
		return;
	}
	PandaNode* node = collisionPath.node();
	if (node != nullptr && node->is_of_type(CollisionNode::get_class_type()))
	{
		CollisionNode* collisionNode = DCAST(CollisionNode, node);
		collisionNode->set_into_collide_mask(active ? AnnotationCollisionMask(config) : BitMask32::all_off());
	}
}

bool AnnotationProjector::ensureCollisionRoot(const NodePath& renderRoot)
{
	if (renderRoot.is_empty())
	{
		return false;
	}
	if (m_collisionRoot.is_empty())
	{
		m_collisionRoot = renderRoot.attach_new_node("AnnotationCollisionRoot");
		m_rayPath = NodePath();
		m_reusableRay = nullptr;
		m_reusableRayNode = nullptr;
		m_reusableQueue = nullptr;
		m_reusableTraverser.clear_colliders();
		m_collisionCache.clear();
		m_surfaceSnapCache.clear();
	}
	return !m_collisionRoot.is_empty();
}

bool AnnotationProjector::ensureReusableRayCollider(const AnnotationConfig& config)
{
	if (m_collisionRoot.is_empty())
	{
		return false;
	}
	if (m_reusableRay == nullptr || m_reusableRayNode == nullptr || m_reusableQueue == nullptr || m_rayPath.is_empty())
	{
		m_reusableRay = new CollisionRay();
		m_reusableRayNode = new CollisionNode("AnnotationOcclusionRay");
		m_reusableRayNode->set_from_collide_mask(AnnotationCollisionMask(config));
		m_reusableRayNode->set_into_collide_mask(BitMask32::all_off());
		m_reusableRayNode->add_solid(m_reusableRay);
		m_rayPath = m_collisionRoot.attach_new_node(m_reusableRayNode);
		m_reusableQueue = new CollisionHandlerQueue();
		m_reusableTraverser.clear_colliders();
		m_reusableTraverser.add_collider(m_rayPath, m_reusableQueue);
		return true;
	}
	m_reusableRayNode->set_from_collide_mask(AnnotationCollisionMask(config));
	return true;
}

bool AnnotationProjector::keyPointMayOverlapTargetBBox(const std::string& targetKey, int keyPixelX, int keyPixelY) const
{
	std::map<std::string, AnnotationRect2D>::const_iterator it = m_frameBBoxCache.find(targetKey);
	if (it == m_frameBBoxCache.end() || !it->second.visible)
	{
		return true;
	}
	const AnnotationRect2D& bbox = it->second;
	const int margin = 2;
	return keyPixelX >= bbox.x - margin &&
		keyPixelX <= bbox.x + bbox.width + margin &&
		keyPixelY >= bbox.y - margin &&
		keyPixelY <= bbox.y + bbox.height + margin;
}

void AnnotationProjector::registerFrameBBox(const TargetPlatformData& targetPlat, const AnnotationRect2D& bbox)
{
	m_frameBBoxCache[TargetKeyText(targetPlat)] = bbox;
}
