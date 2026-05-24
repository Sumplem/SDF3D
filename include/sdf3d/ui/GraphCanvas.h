#pragma once

#include <imgui.h>

namespace sdf3d::ui {

/// Runtime frame data for a pan/zoom node canvas.
struct GraphCanvasFrame {
    ImVec2 origin = {0.0f, 0.0f};
    ImVec2 end = {0.0f, 0.0f};
    ImVec2 pan = {0.0f, 0.0f};
    float zoom = 1.0f;
    ImDrawList* drawList = nullptr;
};

/// Scales a canvas-space value by the current zoom.
float scaleValue(const GraphCanvasFrame& frame, float value);

/// Converts graph coordinates to screen coordinates.
ImVec2 graphToScreen(const GraphCanvasFrame& frame, ImVec2 graphPosition);

/// Converts screen coordinates to graph coordinates.
ImVec2 screenToGraph(const GraphCanvasFrame& frame, ImVec2 screenPosition);

/// Begins a clipped graph canvas child and returns its frame.
GraphCanvasFrame beginGraphCanvas(
    const char* id,
    float panX,
    float panY,
    float zoom,
    ImVec2 minSize,
    float minZoom,
    float maxZoom);

/// Updates shift-drag pan and mouse-wheel zoom, anchored under the cursor.
void updateGraphCanvasView(GraphCanvasFrame& frame, float& panX, float& panY, float& zoom, float minZoom, float maxZoom);

/// Draws the shared graph canvas background grid.
void drawGraphCanvasGrid(const GraphCanvasFrame& frame);

} // namespace sdf3d::ui
