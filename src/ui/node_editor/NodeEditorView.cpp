#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <algorithm>
#include <cmath>

namespace sdf3d::node_editor {
namespace {

constexpr float MIN_ZOOM = 0.35f;
constexpr float MAX_ZOOM = 2.25f;
constexpr float ZOOM_STEP = 0.12f;
constexpr float GRID_STEP = 32.0f;

float clampZoom(float zoom)
{
    return std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
}

bool rectsOverlap(ImVec2 aMin, ImVec2 aMax, ImVec2 bMin, ImVec2 bMax)
{
    return aMin.x <= bMax.x && aMax.x >= bMin.x && aMin.y <= bMax.y && aMax.y >= bMin.y;
}

void normalizeRect(ImVec2 a, ImVec2 b, ImVec2& min, ImVec2& max)
{
    min = {std::min(a.x, b.x), std::min(a.y, b.y)};
    max = {std::max(a.x, b.x), std::max(a.y, b.y)};
}

} // namespace

float scaleValue(const CanvasFrame& frame, float value)
{
    return value * frame.zoom;
}

ImVec2 graphToScreen(const CanvasFrame& frame, ImVec2 graphPosition)
{
    return {
        frame.origin.x + frame.pan.x + graphPosition.x * frame.zoom,
        frame.origin.y + frame.pan.y + graphPosition.y * frame.zoom,
    };
}

ImVec2 screenToGraph(const CanvasFrame& frame, ImVec2 screenPosition)
{
    return {
        (screenPosition.x - frame.origin.x - frame.pan.x) / frame.zoom,
        (screenPosition.y - frame.origin.y - frame.pan.y) / frame.zoom,
    };
}

ImVec2 canvasMouseGraphPosition(const CanvasFrame& frame)
{
    ImVec2 graphPosition = screenToGraph(frame, ImGui::GetIO().MousePos);
    graphPosition.x -= 24.0f;
    graphPosition.y -= 24.0f;
    return graphPosition;
}

CanvasFrame beginCanvas(float panX, float panY, float zoom)
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 canvasSize = {std::max(available.x, 320.0f), std::max(available.y, 260.0f)};
    ImGui::BeginChild("GraphCanvas", canvasSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    CanvasFrame frame;
    frame.origin = ImGui::GetCursorScreenPos();
    frame.end = {frame.origin.x + canvasSize.x, frame.origin.y + canvasSize.y};
    frame.pan = {panX, panY};
    frame.zoom = clampZoom(zoom);
    frame.drawList = ImGui::GetWindowDrawList();
    return frame;
}

void updateCanvasView(CanvasFrame& frame, float& panX, float& panY, float& zoom)
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
    const float newZoom = clampZoom(oldZoom + io.MouseWheel * ZOOM_STEP);
    if (newZoom == oldZoom) {
        return;
    }

    // AGENT: Zoom anchors under cursor so users can inspect dense graphs without losing context.
    const ImVec2 mouseGraph = screenToGraph(frame, io.MousePos);
    zoom = newZoom;
    panX = io.MousePos.x - frame.origin.x - mouseGraph.x * newZoom;
    panY = io.MousePos.y - frame.origin.y - mouseGraph.y * newZoom;
    frame.zoom = newZoom;
    frame.pan = {panX, panY};
}

void drawGrid(const CanvasFrame& frame)
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

bool updateSelectionRectangle(
    SdfGraph& graph,
    const CanvasFrame& frame,
    const std::vector<GraphNodeLayout>& layouts,
    const NodeEditorDragState& drag,
    bool mouseInsideNode,
    SelectionRectState& selection)
{
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if (!drag.draggingLink
        && !drag.draggingInputLink
        && !mouseInsideNode
        && ImGui::IsWindowHovered()
        && !ImGui::GetIO().KeyShift
        && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && !ImGui::IsAnyItemHovered()) {
        selection.dragging = true;
        selection.start = mouse;
        selection.end = mouse;
    }

    if (!selection.dragging) {
        return false;
    }

    selection.end = mouse;
    ImVec2 rectMin;
    ImVec2 rectMax;
    normalizeRect(selection.start, selection.end, rectMin, rectMax);
    frame.drawList->AddRectFilled(rectMin, rectMax, IM_COL32(90, 140, 255, 35));
    frame.drawList->AddRect(rectMin, rectMax, IM_COL32(110, 170, 255, 210), 0.0f, 0, 1.5f);

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        return false;
    }

    std::vector<SdfGraphNodeId> selectedIds;
    for (const GraphNodeLayout& layout : layouts) {
        const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};
        if (rectsOverlap(rectMin, rectMax, layout.position, nodeEnd)) {
            selectedIds.push_back(layout.id);
        }
    }
    if (selectedIds.empty()) {
        graph.clearSelection();
    } else {
        graph.setSelectedNodes(selectedIds, selectedIds.back());
    }
    selection.dragging = false;
    return false;
}

} // namespace sdf3d::node_editor
