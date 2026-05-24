#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <algorithm>

namespace sdf3d::node_editor {
namespace {

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

ImVec2 canvasMouseGraphPosition(const CanvasFrame& frame)
{
    ImVec2 graphPosition = screenToGraph(frame, ImGui::GetIO().MousePos);
    graphPosition.x -= 24.0f;
    graphPosition.y -= 24.0f;
    return graphPosition;
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
