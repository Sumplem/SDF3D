#include "sdf3d/ui/GraphCanvas.h"

#include <algorithm>
#include <cmath>

namespace sdf3d::ui {
namespace {

constexpr float ZOOM_STEP = 0.12f;
constexpr float GRID_STEP = 32.0f;

float clampZoom(float zoom, float minZoom, float maxZoom)
{
    return std::clamp(zoom, minZoom, maxZoom);
}

} // namespace

float scaleValue(const GraphCanvasFrame& frame, float value)
{
    return value * frame.zoom;
}

ImVec2 graphToScreen(const GraphCanvasFrame& frame, ImVec2 graphPosition)
{
    return {
        frame.origin.x + frame.pan.x + graphPosition.x * frame.zoom,
        frame.origin.y + frame.pan.y + graphPosition.y * frame.zoom,
    };
}

ImVec2 screenToGraph(const GraphCanvasFrame& frame, ImVec2 screenPosition)
{
    return {
        (screenPosition.x - frame.origin.x - frame.pan.x) / frame.zoom,
        (screenPosition.y - frame.origin.y - frame.pan.y) / frame.zoom,
    };
}

GraphCanvasFrame beginGraphCanvas(
    const char* id,
    float panX,
    float panY,
    float zoom,
    ImVec2 minSize,
    float minZoom,
    float maxZoom)
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 canvasSize = {std::max(available.x, minSize.x), std::max(available.y, minSize.y)};
    ImGui::BeginChild(id, canvasSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    GraphCanvasFrame frame;
    frame.origin = ImGui::GetCursorScreenPos();
    frame.end = {frame.origin.x + canvasSize.x, frame.origin.y + canvasSize.y};
    frame.pan = {panX, panY};
    frame.zoom = clampZoom(zoom, minZoom, maxZoom);
    frame.drawList = ImGui::GetWindowDrawList();
    return frame;
}

void updateGraphCanvasView(GraphCanvasFrame& frame, float& panX, float& panY, float& zoom, float minZoom, float maxZoom)
{
    if (!ImGui::IsWindowHovered()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyShift && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        panX += io.MouseDelta.x;
        panY += io.MouseDelta.y;
        frame.pan = {panX, panY};
    }

    if (io.MouseWheel == 0.0f) {
        return;
    }

    const float oldZoom = frame.zoom;
    const float newZoom = clampZoom(oldZoom + io.MouseWheel * ZOOM_STEP, minZoom, maxZoom);
    if (newZoom == oldZoom) {
        return;
    }

    // AGENT: Zoom anchors under cursor so graph inspection does not lose context.
    const ImVec2 mouseGraph = screenToGraph(frame, io.MousePos);
    zoom = newZoom;
    panX = io.MousePos.x - frame.origin.x - mouseGraph.x * newZoom;
    panY = io.MousePos.y - frame.origin.y - mouseGraph.y * newZoom;
    frame.zoom = newZoom;
    frame.pan = {panX, panY};
}

void drawGraphCanvasGrid(const GraphCanvasFrame& frame)
{
    frame.drawList->AddRectFilled(frame.origin, frame.end, IM_COL32(30, 32, 36, 255));

    const float gridStep = GRID_STEP * frame.zoom;
    const float offsetX = std::fmod(frame.pan.x, gridStep);
    const float offsetY = std::fmod(frame.pan.y, gridStep);

    for (float x = frame.origin.x + offsetX; x < frame.end.x; x += gridStep) {
        frame.drawList->AddLine({x, frame.origin.y}, {x, frame.end.y}, IM_COL32(48, 50, 56, 255));
    }
    for (float y = frame.origin.y + offsetY; y < frame.end.y; y += gridStep) {
        frame.drawList->AddLine({frame.origin.x, y}, {frame.end.x, y}, IM_COL32(48, 50, 56, 255));
    }
}

} // namespace sdf3d::ui
