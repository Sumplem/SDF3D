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

/// Returns shared selected/hover node shell colors for graph editors.
GraphNodeStyle graphNodeInteractionStyle(bool selected, bool hovered, ImU32 titleColor);

/// Returns shared socket color/radius for normal and hovered graph pins.
ImU32 graphSocketInteractionColor(ImU32 normalColor, ImU32 hoverColor, bool hovered);

/// Returns shared socket radius for normal and hovered graph pins.
float graphSocketInteractionRadius(bool hovered);

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

/// Draws an active link drag from a fixed socket to the current mouse position.
void drawGraphLinkDrag(const GraphCanvasFrame& frame, ImVec2 fixedSocket, bool fixedSocketIsOutput, ImVec2 mouse, ImU32 color);

/// Returns true when point is inside a zoom-scaled socket hit circle.
bool graphSocketHit(const GraphCanvasFrame& frame, ImVec2 point, ImVec2 socketPosition, float radius = 12.0f);

/// Draws a zoom-scaled socket drop feedback ring.
void drawGraphSocketDropFeedback(const GraphCanvasFrame& frame, ImVec2 position, ImU32 color);

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
