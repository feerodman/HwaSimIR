#pragma once

#include "AnnotationTypes.h"
#include "nodePath.h"
#include "textNode.h"
#include "AnnotationLabelLayout.h"

class AnnotationOverlay
{
public:
	void initialize(const NodePath& overlayRoot);
	void clear();
	void drawFrame(const AnnotationFrameRecord& record, const AnnotationDrawOptions& options);
	const std::vector<AnnotationLabelPlacement>& labelPlacements() const {return m_placements;}
	const std::vector<AnnotationLabelRequest>& labelRequests() const {return m_requests;}

private:
	LPoint3f pixelToOverlayCoord(int x, int y, int width, int height) const;
	void drawRect(const AnnotationRect2D& rect, int width, int height);
	void drawCross(const AnnotationPoint2D& point, int width, int height);
	void addText(const std::string& text, int x, int y, int width, int height, float scale, bool keyPoint = false);

	NodePath m_overlayRoot;
	NodePath m_frameRoot;
	int m_drawLogCounter = 0;
	AnnotationLabelLayout m_layout;
	std::vector<AnnotationLabelRequest> m_requests;
	std::vector<AnnotationLabelPlacement> m_placements;
	std::vector<PT(TextNode)> m_textNodes;
	std::vector<float> m_textScales;
	std::string m_labelIdentity;
	void flushText(const AnnotationFrameRecord& record);
};
