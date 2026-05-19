#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"
#include "sdf3d/ui/node_editor/NodeEditorLayout.h"

#include "sdf3d/scene/SdfNodeTraits.h"

#include <algorithm>
#include <vector>

namespace sdf3d::node_editor {
namespace {

float layoutZoom(const GraphNodeLayout& layout)
{
    return std::max(0.01f, layout.size.x / NODE_WIDTH);
}

bool wrapInMaterialOverride(SdfGraph& graph, const GraphNodeLayout& layout)
{
    const SdfGraphNodeId materialNode = graph.createNode(SdfNodeType::MaterialOverride);
    SdfGraphNode* material = graph.node(materialNode);
    if (material == nullptr) {
        return false;
    }

    material->editorX = layout.node->editorX + NODE_WIDTH + 40.0f;
    material->editorY = layout.node->editorY;

    const std::vector<SdfGraphLink> oldLinks = graph.links();
    for (const SdfGraphLink& link : oldLinks) {
        if (link.fromNode == layout.id && link.fromSocket == "sdf") {
            graph.unlink(link.fromNode, link.fromSocket, link.toNode, link.toSocket);
            graph.link(materialNode, "sdf", link.toNode, link.toSocket);
        }
    }

    // AGENT: Wrap keeps existing downstream links, then inserts material tag
    // between primitive geometry and all consumers.
    graph.link(layout.id, "sdf", materialNode, "sdf");
    graph.setSelectedNode(materialNode);
    return true;
}

std::vector<SdfGraphNodeId> deletableSelectedNodes(const SdfGraph& graph)
{
    std::vector<SdfGraphNodeId> ids;
    for (const SdfGraphNodeId id : graph.selectedNodes()) {
        if (!graph.isOutputNode(id) && graph.node(id) != nullptr) {
            ids.push_back(id);
        }
    }
    return ids;
}

void frameSelectedNodes(
    const std::vector<GraphNodeLayout>& layouts,
    const SdfGraph& graph,
    const CanvasFrame& frame,
    float& canvasPanX,
    float& canvasPanY)
{
    ImVec2 boundsMin = {0.0f, 0.0f};
    ImVec2 boundsMax = {0.0f, 0.0f};
    bool hasBounds = false;
    for (const GraphNodeLayout& layout : layouts) {
        if (!graph.isNodeSelected(layout.id) || layout.node == nullptr) {
            continue;
        }
        const float graphWidth = layout.size.x / frame.zoom;
        const float graphHeight = layout.size.y / frame.zoom;
        const ImVec2 min = {24.0f + layout.node->editorX, 24.0f + layout.node->editorY};
        const ImVec2 max = {min.x + graphWidth, min.y + graphHeight};
        if (!hasBounds) {
            boundsMin = min;
            boundsMax = max;
            hasBounds = true;
        } else {
            boundsMin.x = std::min(boundsMin.x, min.x);
            boundsMin.y = std::min(boundsMin.y, min.y);
            boundsMax.x = std::max(boundsMax.x, max.x);
            boundsMax.y = std::max(boundsMax.y, max.y);
        }
    }
    if (!hasBounds) {
        return;
    }

    const ImVec2 center = {(boundsMin.x + boundsMax.x) * 0.5f, (boundsMin.y + boundsMax.y) * 0.5f};
    const ImVec2 canvasSize = {frame.end.x - frame.origin.x, frame.end.y - frame.origin.y};
    canvasPanX = canvasSize.x * 0.5f - center.x * frame.zoom;
    canvasPanY = canvasSize.y * 0.5f - center.y * frame.zoom;
}

} // namespace

bool shortcutsEnabled()
{
    const ImGuiIO& io = ImGui::GetIO();
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
}

EditorDirtyState handleNodeEditorShortcuts(
    SdfGraph& graph,
    EventBus* eventBus,
    const CanvasFrame& frame,
    const std::vector<GraphNodeLayout>& layouts,
    float& canvasPanX,
    float& canvasPanY,
    std::vector<SdfGraphNodeId>& pendingDelete)
{
    EditorDirtyState dirty;
    if (!shortcutsEnabled()) {
        return dirty;
    }

    const SdfGraphNodeId selectedNode = graph.selectedNode();
    const bool hasSelection = selectedNode != 0 && graph.node(selectedNode) != nullptr;
    if (ImGui::IsKeyPressed(ImGuiKey_P)) {
        if (previewSelectedNode(graph)) {
            dirty.scene = true;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_L)) {
        if (autoLayoutGraph(graph, frame, ImGui::GetIO().KeyShift)) {
            dirty.scene = true;
        }
    }
    if (hasSelection && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        const std::vector<SdfGraphNodeId> ids = deletableSelectedNodes(graph);
        pendingDelete.insert(pendingDelete.end(), ids.begin(), ids.end());
    }
    if (hasSelection && eventBus != nullptr && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        eventBus->emit(DuplicateSelectionEvent{graph.selectedNodes()});
    }
    if (hasSelection && ImGui::IsKeyPressed(ImGuiKey_F)) {
        frameSelectedNodes(layouts, graph, frame, canvasPanX, canvasPanY);
    }

    return dirty;
}

bool flushPendingDeletes(SdfGraph& graph, std::vector<SdfGraphNodeId>& pendingDelete)
{
    bool sceneDirty = false;
    if (pendingDelete.empty()) {
        return false;
    }

    std::sort(pendingDelete.begin(), pendingDelete.end());
    pendingDelete.erase(std::unique(pendingDelete.begin(), pendingDelete.end()), pendingDelete.end());
    for (const SdfGraphNodeId id : pendingDelete) {
        if (graph.deleteNode(id)) {
            sceneDirty = true;
        }
    }
    return sceneDirty;
}

bool drawNodeActions(SdfGraph& graph, const GraphNodeLayout& layout, SdfGraphNodeId& pendingDelete)
{
    bool sceneDirty = false;
    const float zoom = layoutZoom(layout);
    const float buttonExtent = std::max(16.0f, 18.0f * zoom);
    const ImVec2 buttonSize = {buttonExtent, buttonExtent};
    const float top = layout.position.y + 5.0f * zoom;
    const float deleteX = layout.position.x + layout.size.x - buttonExtent - 5.0f * zoom;
    const float collapseX = deleteX - buttonExtent - 4.0f * zoom;

    ImGui::SetCursorScreenPos({collapseX, top});
    const char* collapseLabel = layout.node->editorPropertiesCollapsed ? "+##node-props-" : "-##node-props-";
    if (ImGui::Button((std::string(collapseLabel) + std::to_string(layout.id)).c_str(), buttonSize)) {
        layout.node->editorPropertiesCollapsed = !layout.node->editorPropertiesCollapsed;
    }

    ImGui::SetCursorScreenPos({deleteX, top});
    if (graph.isOutputNode(layout.id)) {
        ImGui::BeginDisabled();
        ImGui::Button(("X##delete-node-" + std::to_string(layout.id)).c_str(), buttonSize);
        ImGui::EndDisabled();
    } else if (ImGui::Button(("X##delete-node-" + std::to_string(layout.id)).c_str(), buttonSize)) {
        pendingDelete = layout.id;
    }

    const std::string popupId = "node-context-" + std::to_string(layout.id);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};
    const bool mouseInsideNode = mouse.x >= layout.position.x && mouse.x <= nodeEnd.x && mouse.y >= layout.position.y && mouse.y <= nodeEnd.y;
    if (isSdfPrimitiveNode(layout.node->payload.type)
        && mouseInsideNode
        && ImGui::IsWindowHovered()
        && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        graph.setSelectedNode(layout.id);
        ImGui::OpenPopup(popupId.c_str());
    }

    if (ImGui::BeginPopup(popupId.c_str())) {
        if (ImGui::MenuItem("Wrap in Material Override")) {
            sceneDirty = wrapInMaterialOverride(graph, layout);
        }
        ImGui::EndPopup();
    }

    return sceneDirty;
}

} // namespace sdf3d::node_editor
