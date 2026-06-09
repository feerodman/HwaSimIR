#include "AnnotationProjector.h"

#include "geom.h"
#include "geomNode.h"
#include "geomVertexData.h"
#include "geomVertexReader.h"
#include "internalName.h"
#include "nodePathCollection.h"
#include "pandaNode.h"

#include <algorithm>
#include <cmath>
#include <iostream>
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
}

bool AnnotationProjector::buildTargetAnnotation(
	const TargetPlatformData& targetPlat,
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
	if (!buildBoundingBox(targetPlat.nodePath, renderRoot, cameraNode, cameraLens, width, height, targetConfig, localMin, localMax, bbox, bboxStats))
	{
		return false;
	}

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
		buildKeyPoint("head", headLocal, targetPlat.nodePath, renderRoot, cameraNode, cameraLens, width, height, headPoint))
	{
		outAnnotation.keyPoints.push_back(headPoint);
	}

	if (targetConfig.keyPointsEnabled && targetConfig.middlePoint.visible &&
		buildKeyPoint("middle", middleLocal, targetPlat.nodePath, renderRoot, cameraNode, cameraLens, width, height, middlePoint))
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
	const NodePath& targetNode,
	const NodePath& renderRoot,
	const NodePath& cameraNode,
	Lens* cameraLens,
	int width,
	int height,
	AnnotationPoint2D& outPoint)
{
	const LPoint3f worldPoint = renderRoot.get_relative_point(targetNode, localPoint);
	int x = 0;
	int y = 0;
	bool inViewport = false;
	if (!projectWorldPointToPixel(renderRoot, cameraNode, cameraLens, worldPoint, width, height, x, y, inViewport) || !inViewport)
	{
		return false;
	}

	const LPoint3f cameraWorldPoint = renderRoot.get_relative_point(cameraNode, LPoint3f(0.0f, 0.0f, 0.0f));
	if (!isKeyPointVisibleByRay(renderRoot, cameraWorldPoint, worldPoint))
	{
		return false;
	}

	outPoint.name = name;
	outPoint.x = x;
	outPoint.y = y;
	outPoint.visible = true;
	return true;
}

bool AnnotationProjector::isKeyPointVisibleByRay(const NodePath& renderRoot, const LPoint3f& cameraWorldPoint, const LPoint3f& keyWorldPoint)
{
	(void)renderRoot;
	(void)cameraWorldPoint;
	(void)keyWorldPoint;

	// Stage 1.2 保留射线遮挡接口；当前模型未统一提供 collision solids，因此降级为仅投影可见。
	++m_occlusionWarningCounter;
	if (m_occlusionWarningCounter == 1 || (m_occlusionWarningCounter % 300) == 0)
	{
		std::cout << "[Annotation][WARN] occlusion geometry unavailable"
			<< " fallback=projection_only"
			<< " checks=" << m_occlusionWarningCounter
			<< std::endl;
	}
	return true;
}
