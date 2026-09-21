#include "AnnotationOverlay.h"

#include "lineSegs.h"
#ifdef _MSC_VER
#pragma warning(disable:4996) // Read-only diagnostic environment switches.
#endif
#include "pandaNode.h"
#include "textNode.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>

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
	m_layout.reset();
	clear();
}

void AnnotationOverlay::clear()
{
	m_requests.clear();m_textNodes.clear();m_textScales.clear();m_placements.clear();
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
		m_labelIdentity=std::to_string(target.targetType)+":"+std::to_string(target.targetPlatID)+":"+std::to_string(target.targetID)+":";
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
			if (point.displayIndex <= 0)
			{
				std::cerr << "[AnnotationDisplayIndex][ERROR] target=" << m_labelIdentity
					<< " name=" << point.name << " reason=missing_stable_definition_index" << std::endl;
				continue;
			}
			std::ostringstream label;
			label << point.displayIndex << "(" << point.x << "," << point.y << ")";
			addText(label.str(), point.x + 6, point.y - 6,
				record.width, record.height, 0.028f, true);
		}
	}
	flushText(record);
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

void AnnotationOverlay::addText(const std::string& text, int x, int y, int width, int height, float scale, bool keyPoint)
{
	const int safeX = ClampPixel(x, 0, std::max(0, width - 1));
	const int safeY = ClampPixel(y, 0, std::max(0, height - 1));
	PT(TextNode) textNode = new TextNode("Annotation_Text");
	textNode->set_text(text);
	textNode->set_text_color(0.0f, 1.0f, 0.2f, 1.0f);
	textNode->set_shadow(0.04f, 0.04f);
	textNode->set_shadow_color(0.0f, 0.0f, 0.0f, 0.9f);
	textNode->set_align(TextNode::A_left);

	const char* legacy=std::getenv("P10LabelLegacy");
	if(!legacy || std::string(legacy)!="1"){
		AnnotationLabelRequest r;r.text=text;r.keyPoint=keyPoint;
		r.key=m_labelIdentity+text.substr(0,text.find('('));r.anchorX=float(x);r.anchorY=float(y);
		r.w=std::ceil((textNode->get_right()-textNode->get_left())*scale*width*.5f)+3;
		r.h=std::ceil((textNode->get_top()-textNode->get_bottom())*scale*height*.5f)+3;
		m_requests.push_back(r);m_textNodes.push_back(textNode);m_textScales.push_back(scale);return;
	}
	NodePath textPath = m_frameRoot.attach_new_node(textNode);
	// 标注坐标对应最终图像左上角像素坐标；这里只转换到 Panda3D 2D overlay 坐标。
	textPath.set_pos(pixelToOverlayCoord(safeX, safeY, width, height));
	textPath.set_scale(scale);
	textPath.set_bin("fixed", 103);
	textPath.set_depth_test(false);
	textPath.set_depth_write(false);
}

void AnnotationOverlay::flushText(const AnnotationFrameRecord& record){
    m_placements=m_layout.place(m_requests,record.width,record.height);
    for(size_t i=0;i<m_requests.size();++i){
        const auto& r=m_requests[i];const auto& b=m_placements[i].box;auto t=m_textNodes[i];const float s=m_textScales[i];
        const int x=int(std::lround(b.x+1-t->get_left()*s*record.width*.5f));
        const int y=int(std::lround(b.y+1+t->get_top()*s*record.height*.5f));
        NodePath p=m_frameRoot.attach_new_node(t);p.set_pos(pixelToOverlayCoord(x,y,record.width,record.height));p.set_scale(s);
        p.set_bin("fixed",103);p.set_depth_test(false);p.set_depth_write(false);
		// Preserve the existing keypoint leader anchor even though the displayed
		// text is now the stable numeric presentation index only.
		if(r.keyPoint){
            const float ax=r.anchorX-6,ay=r.anchorY+6;
            const float ex=std::max(b.x,std::min(b.x+b.w,ax)),ey=std::max(b.y,std::min(b.y+b.h,ay));
            LineSegs l("Annotation_TextLeader");l.set_color(0,1,.2f,.85f);l.set_thickness(1);
            l.move_to(pixelToOverlayCoord(int(ax),int(ay),record.width,record.height));
            l.draw_to(pixelToOverlayCoord(int(ex),int(ey),record.width,record.height));
            auto n=m_frameRoot.attach_new_node(l.create());n.set_bin("fixed",102);n.set_depth_test(false);n.set_depth_write(false);
        }
        if(m_placements[i].overflow)std::cerr<<"[AnnotationLayout][ERROR] insufficient_area key="<<r.key<<" allLabelsRetained=1"<<std::endl;
    }
    const char* path=std::getenv("P10LabelDumpPrefix"),*seq=std::getenv("P10LabelDumpSeq");
    if(path&&seq&&record.frameIndex==std::strtoull(seq,nullptr,10)){
        std::ofstream out(std::string(path)+"_labels.csv");out<<"key,text,anchorX,anchorY,labelX,labelY,width,height,candidate,overflow\n";
        for(size_t i=0;i<m_requests.size();++i){const auto& r=m_requests[i];const auto& p=m_placements[i];
            out<<r.key<<",\""<<r.text<<"\","<<r.anchorX<<','<<r.anchorY<<','<<p.box.x<<','<<p.box.y<<','<<p.box.w<<','<<p.box.h<<','<<p.candidate<<','<<p.overflow<<'\n';}
    }
}
