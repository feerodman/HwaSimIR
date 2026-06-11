#pragma once

#include "AnnotationTypes.h"
#include "nodePath.h"

class AnnotationOverlay
{
public:
	void initialize(const NodePath& overlayRoot);
	void clear();
	void drawFrame(const AnnotationFrameRecord& record, const AnnotationDrawOptions& options);

private:
	LPoint3f pixelToOverlayCoord(int x, int y, int width, int height) const;
	void drawRect(const AnnotationRect2D& rect, int width, int height);
	void drawCross(const AnnotationPoint2D& point, int width, int height);
	void addText(const std::string& text, int x, int y, int width, int height, float scale);

	NodePath m_overlayRoot;
	NodePath m_frameRoot;
	int m_drawLogCounter = 0;
};
