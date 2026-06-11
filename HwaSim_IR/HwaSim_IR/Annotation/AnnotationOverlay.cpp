#include "AnnotationOverlay.h"

#include "lineSegs.h"
#include "pandaNode.h"
#include "textNode.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace
{
int ClampPixel(int value, int low, int high)
{
	return std::max(low, std::min(high, value));
}
}

void AnnotationOverlay::initialize(const NodePath& overlayRoot)
{
	m_overlayRoot = overlayRoot;
	clear();
}

void AnnotationOverlay::clear()
{
	if (!m_frameRoot.is_empty())
	{
		m_frameRoot.remove_node();
	}

	if (!m_overlayRoot.is_empty())
	{
		m_frameRoot = m_overlayRoot.attach_new_node("Annotation_Frame");
		m_frameRoot.set_bin("fixed", 100);
		m_frameRoot.set_depth_test(false);
		m_frameRoot.set_depth_write(false);
	}
}

void AnnotationOverlay::drawFrame(const AnnotationFrameRecord& record, const AnnotationDrawOptions& options)
{
	clear();
	if (m_frameRoot.is_empty() || record.width <= 0 || record.height <= 0)
	{
		return;
	}

	// 调试自检文字不依赖目标数据，仅在 DebugOverlay 打开时显示。
	if (options.debugOverlay)
	{
		addText("ANNO_TEST", 8, 20, record.width, record.height, 0.035f);
	}
	++m_drawLogCounter;
	if (m_drawLogCounter <= 3 || (m_drawLogCounter % 120) == 0)
	{
		std::cout << "[AnnotationDraw]"
			<< " test=" << (options.debugOverlay ? "1" : "0")
			<< " targets=" << record.targets.size()
			<< " size=" << record.width << "x" << record.height
			<< std::endl;
	}

	for (size_t i = 0; i < record.targets.size(); ++i)
	{
		const TargetAnnotation& target = record.targets[i];
		if (!target.bbox.visible)
		{
			continue;
		}

		if (options.drawBBox)
		{
			drawRect(target.bbox, record.width, record.height);
		}
		if (options.drawModelLabel)
		{
			addText(target.modelLabel, target.bbox.x, std::max(0, target.bbox.y - 18), record.width, record.height, 0.035f);
		}

		for (size_t k = 0; k < target.keyPoints.size(); ++k)
		{
			if (!options.drawKeyPoints)
			{
				break;
			}
			const AnnotationPoint2D& point = target.keyPoints[k];
			if (!point.visible)
			{
				continue;
			}

			drawCross(point, record.width, record.height);
			std::ostringstream label;
			label << point.name << "(" << point.x << "," << point.y << ")";
			addText(label.str(), point.x + 6, point.y - 6, record.width, record.height, 0.028f);
		}
	}
}

LPoint3f AnnotationOverlay::pixelToOverlayCoord(int x, int y, int width, int height) const
{
	const float safeWidth = static_cast<float>(std::max(1, width));
	const float safeHeight = static_cast<float>(std::max(1, height));
	const float nx = (static_cast<float>(x) / safeWidth) * 2.0f - 1.0f;
	const float nz = 1.0f - (static_cast<float>(y) / safeHeight) * 2.0f;
	return LPoint3f(nx, 0.0f, nz);
}

void AnnotationOverlay::drawRect(const AnnotationRect2D& rect, int width, int height)
{
	const int x0 = rect.x;
	const int y0 = rect.y;
	const int x1 = rect.x + rect.width - 1;
	const int y1 = rect.y + rect.height - 1;
	const LPoint3f p0 = pixelToOverlayCoord(x0, y0, width, height);
	const LPoint3f p1 = pixelToOverlayCoord(x1, y0, width, height);
	const LPoint3f p2 = pixelToOverlayCoord(x1, y1, width, height);
	const LPoint3f p3 = pixelToOverlayCoord(x0, y1, width, height);

	LineSegs lines("Annotation_BBox");
	lines.set_color(0.0f, 1.0f, 0.2f, 1.0f);
	lines.set_thickness(2.0f);
	lines.move_to(p0);
	lines.draw_to(p1);
	lines.draw_to(p2);
	lines.draw_to(p3);
	lines.draw_to(p0);
	NodePath lineNode = m_frameRoot.attach_new_node(lines.create());
	lineNode.set_bin("fixed", 101);
	lineNode.set_depth_test(false);
	lineNode.set_depth_write(false);
}

void AnnotationOverlay::drawCross(const AnnotationPoint2D& point, int width, int height)
{
	const int r = 5;
	const LPoint3f left = pixelToOverlayCoord(point.x - r, point.y, width, height);
	const LPoint3f right = pixelToOverlayCoord(point.x + r, point.y, width, height);
	const LPoint3f top = pixelToOverlayCoord(point.x, point.y - r, width, height);
	const LPoint3f bottom = pixelToOverlayCoord(point.x, point.y + r, width, height);

	LineSegs lines("Annotation_KeyPoint");
	lines.set_color(0.0f, 1.0f, 0.2f, 1.0f);
	lines.set_thickness(2.0f);
	lines.move_to(left);
	lines.draw_to(right);
	lines.move_to(top);
	lines.draw_to(bottom);
	NodePath lineNode = m_frameRoot.attach_new_node(lines.create());
	lineNode.set_bin("fixed", 102);
	lineNode.set_depth_test(false);
	lineNode.set_depth_write(false);
}

void AnnotationOverlay::addText(const std::string& text, int x, int y, int width, int height, float scale)
{
	const int safeX = ClampPixel(x, 0, std::max(0, width - 1));
	const int safeY = ClampPixel(y, 0, std::max(0, height - 1));
	PT(TextNode) textNode = new TextNode("Annotation_Text");
	textNode->set_text(text);
	textNode->set_text_color(0.0f, 1.0f, 0.2f, 1.0f);
	textNode->set_shadow(0.04f, 0.04f);
	textNode->set_shadow_color(0.0f, 0.0f, 0.0f, 0.9f);
	textNode->set_align(TextNode::A_left);

	NodePath textPath = m_frameRoot.attach_new_node(textNode);
	// 标注坐标对应最终图像左上角像素坐标；这里只转换到 Panda3D 2D overlay 坐标。
	textPath.set_pos(pixelToOverlayCoord(safeX, safeY, width, height));
	textPath.set_scale(scale);
	textPath.set_bin("fixed", 103);
	textPath.set_depth_test(false);
	textPath.set_depth_write(false);
}
