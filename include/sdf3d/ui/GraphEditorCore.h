#pragma once

#include "sdf3d/ui/GraphCanvas.h"

#include <string>

namespace sdf3d::ui {

/// Visual colors for a graph-editor node shell.
struct GraphNodeStyle {
    ImU32 bodyColor = IM_COL32(42, 45, 52, 255);
    ImU32 titleColor = IM_COL32(54, 58, 68, 255);
    ImU32 borderColor = IM_COL32(78, 82, 92, 255);
    ImU32 titleTextColor = IM_COL32(235, 238, 242, 255);
    float borderThickness = 1.0f;
};

/// Draws a shared graph node body, title bar, border, and title text.
void drawGraphNodeShell(const GraphCanvasFrame& frame, ImVec2 position, ImVec2 size, const std::string& title, const GraphNodeStyle& style);

/// Draws scaled graph-editor text.
void drawGraphText(const GraphCanvasFrame& frame, ImVec2 position, ImU32 color, const std::string& text);

/// Returns squared distance between two screen points.
float distanceSquared(ImVec2 a, ImVec2 b);

/// Returns true when a point is inside a screen-space rectangle.
bool pointInsideRect(ImVec2 point, ImVec2 rectMin, ImVec2 rectMax);

/// Draws a horizontal graph wire Bezier.
void drawGraphBezier(const GraphCanvasFrame& frame, ImVec2 from, ImVec2 to, ImU32 color, float thickness = 3.0f, float handleLength = 70.0f);

/// Draws a zoom-scaled socket circle.
void drawGraphSocket(const GraphCanvasFrame& frame, ImVec2 position, ImU32 color, float radius = 6.5f);

/// Returns the on-title action button extent at the current canvas zoom.
float graphNodeActionButtonExtent(const GraphCanvasFrame& frame);

/// Draws a right-aligned title-bar action button. Index 0 is rightmost.
bool drawGraphNodeTitleActionButton(
    const GraphCanvasFrame& frame,
    ImVec2 nodePosition,
    ImVec2 nodeSize,
    int rightIndex,
    const std::string& label,
    bool enabled);

/// Width reserved on the title bar for right-side action buttons.
float graphNodeTitleActionWidth(const GraphCanvasFrame& frame, int buttonCount);

/// Draws the invisible title drag hit region, leaving action space on the right.
void drawGraphNodeTitleDragRegion(
    const GraphCanvasFrame& frame,
    ImVec2 nodePosition,
    ImVec2 nodeSize,
    float titleHeight,
    float reservedActionWidth,
    const std::string& id);

} // namespace sdf3d::ui
