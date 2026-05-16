#include "sdf3d/ui/NodeEditor.h"

#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <algorithm>
#include <vector>

#include <imgui.h>

namespace sdf3d {
namespace {

bool shortcutsEnabled()
{
    const ImGuiIO& io = ImGui::GetIO();
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput;
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
    const std::vector<node_editor::GraphNodeLayout>& layouts,
    const SdfGraph& graph,
    const node_editor::CanvasFrame& frame,
    float& canvasPanX,
    float& canvasPanY)
{
    ImVec2 boundsMin = {0.0f, 0.0f};
    ImVec2 boundsMax = {0.0f, 0.0f};
    bool hasBounds = false;
    for (const node_editor::GraphNodeLayout& layout : layouts) {
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

EditorDirtyState NodeEditor::draw(SceneGraph& sceneGraph)
{
    EditorDirtyState dirty;
    SdfGraph& graph = sceneGraph.graph();

    node_editor::CanvasFrame frame = node_editor::beginCanvas(m_canvasPanX, m_canvasPanY, m_canvasZoom);
    node_editor::updateCanvasView(frame, m_canvasPanX, m_canvasPanY, m_canvasZoom);
    if (shortcutsEnabled() && ImGui::IsKeyPressed(ImGuiKey_P)) {
        if (node_editor::previewSelectedNode(graph)) {
            dirty.scene = true;
        }
    }

    node_editor::drawGrid(frame);

    std::vector<node_editor::GraphNodeLayout> layouts;
    std::vector<node_editor::GraphSocketAnchor> anchors;
    node_editor::buildLayoutsAndAnchors(graph, frame, layouts, anchors);

    const bool removedLink = node_editor::drawExistingLinks(graph, frame, anchors);
    if (removedLink) {
        dirty.scene = true;
    }

    std::vector<SdfGraphNodeId> pendingDelete;
    if (layouts.empty()) {
        frame.drawList->AddText({frame.origin.x + 16.0f, frame.origin.y + 16.0f}, IM_COL32(210, 215, 225, 255), "Empty graph");
    }

    if (shortcutsEnabled()) {
        const SdfGraphNodeId selectedNode = graph.selectedNode();
        const bool hasSelection = selectedNode != 0 && graph.node(selectedNode) != nullptr;
        if (hasSelection && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            const std::vector<SdfGraphNodeId> ids = deletableSelectedNodes(graph);
            pendingDelete.insert(pendingDelete.end(), ids.begin(), ids.end());
        }
        if (hasSelection && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
            std::vector<SdfGraphNodeId> duplicates;
            const std::vector<SdfGraphNodeId> selectedNodes = graph.selectedNodes();
            for (const SdfGraphNodeId id : selectedNodes) {
                if (graph.isOutputNode(id)) {
                    continue;
                }
                const SdfGraphNodeId duplicate = graph.duplicateNode(id);
                if (duplicate != 0) {
                    duplicates.push_back(duplicate);
                }
            }
            if (!duplicates.empty() && graph.setSelectedNodes(duplicates, duplicates.back())) {
                dirty.scene = true;
            }
        }
        if (hasSelection && ImGui::IsKeyPressed(ImGuiKey_F)) {
            frameSelectedNodes(layouts, graph, frame, m_canvasPanX, m_canvasPanY);
            frame.pan = {m_canvasPanX, m_canvasPanY};
        }
    }

    SdfGraphNodeId releasedDraggedNode = 0;
    SdfGraphNodeId activeDraggedNode = 0;
    for (const node_editor::GraphNodeLayout& layout : layouts) {
        node_editor::drawNodeBody(graph, layout, frame);
        node_editor::drawInactiveNodePreview(graph, layout, frame, anchors);
        if (node_editor::handleNodeTitleDrag(graph, layout, activeDraggedNode)) {
            releasedDraggedNode = layout.id;
        }
        if (node_editor::drawInputPins(
                graph,
                layout,
                frame,
                m_draggingLink,
                m_dragOutputNode,
                m_dragOutputSocket,
                m_inputDragCandidateNode,
                m_inputDragCandidateSocket,
                m_draggingInputLink,
                m_dragInputNode,
                m_dragInputSocket,
                m_dragOutputFromInputDetach)) {
            dirty.scene = true;
        }
        node_editor::drawOutputPins(graph, layout, frame, m_draggingLink, m_dragOutputNode, m_dragOutputSocket, m_dragOutputFromInputDetach);
        SdfGraphNodeId actionDelete = 0;
        if (node_editor::drawNodeActions(graph, layout, actionDelete)) {
            dirty.scene = true;
        }
        if (actionDelete != 0) {
            if (graph.isNodeSelected(actionDelete)) {
                const std::vector<SdfGraphNodeId> ids = deletableSelectedNodes(graph);
                pendingDelete.insert(pendingDelete.end(), ids.begin(), ids.end());
            } else {
                pendingDelete.push_back(actionDelete);
            }
        }
        const EditorDirtyState inlineDirty = node_editor::drawNodeInlineProperties(layout, frame);
        dirty.scene = dirty.scene || inlineDirty.scene;
        dirty.material = dirty.material || inlineDirty.material;
    }

    if (activeDraggedNode != 0) {
        for (const node_editor::GraphNodeLayout& layout : layouts) {
            if (layout.id == activeDraggedNode) {
                node_editor::drawLinkInsertionPreview(graph, layout, frame, anchors);
                break;
            }
        }
    }

    if (releasedDraggedNode != 0) {
        for (const node_editor::GraphNodeLayout& layout : layouts) {
            if (layout.id == releasedDraggedNode && node_editor::insertNodeIntoLink(graph, layout, anchors)) {
                dirty.scene = true;
                break;
            }
        }
    }

    const SdfGraphNodeId releasedFromNode = m_dragOutputNode;
    const std::string releasedFromSocket = m_dragOutputSocket;
    const bool suppressOutputPopup = m_dragOutputFromInputDetach;
    const SdfGraphNodeId releasedToNode = m_dragInputNode;
    const std::string releasedToSocket = m_dragInputSocket;
    bool releasedLinkOnEmpty = false;
    if (node_editor::updateActiveLinkDrag(graph, frame, anchors, m_draggingLink, m_dragOutputNode, m_dragOutputSocket, releasedLinkOnEmpty)) {
        dirty.scene = true;
    }
    if (!m_draggingLink) {
        m_dragOutputFromInputDetach = false;
        if (suppressOutputPopup) {
            releasedLinkOnEmpty = false;
        }
    }

    bool releasedInputLinkOnEmpty = false;
    if (node_editor::updateActiveInputLinkDrag(graph, frame, anchors, m_draggingInputLink, m_dragInputNode, m_dragInputSocket, releasedInputLinkOnEmpty)) {
        dirty.scene = true;
    }

    bool mouseInsideNode = false;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    for (const node_editor::GraphNodeLayout& layout : layouts) {
        const ImVec2 nodeEnd = {layout.position.x + layout.size.x, layout.position.y + layout.size.y};
        if (mouse.x >= layout.position.x && mouse.x <= nodeEnd.x && mouse.y >= layout.position.y && mouse.y <= nodeEnd.y) {
            mouseInsideNode = true;
            break;
        }
    }

    if (!m_draggingLink
        && !m_draggingInputLink
        && !mouseInsideNode
        && ImGui::IsWindowHovered()
        && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
        && !ImGui::IsAnyItemHovered()) {
        m_draggingSelectionRect = true;
        m_selectionRectStart = mouse;
        m_selectionRectEnd = mouse;
    }

    if (m_draggingSelectionRect) {
        m_selectionRectEnd = mouse;
        ImVec2 rectMin;
        ImVec2 rectMax;
        normalizeRect(m_selectionRectStart, m_selectionRectEnd, rectMin, rectMax);
        frame.drawList->AddRectFilled(rectMin, rectMax, IM_COL32(90, 140, 255, 35));
        frame.drawList->AddRect(rectMin, rectMax, IM_COL32(110, 170, 255, 210), 0.0f, 0, 1.5f);

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            std::vector<SdfGraphNodeId> selectedIds;
            for (const node_editor::GraphNodeLayout& layout : layouts) {
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
            m_draggingSelectionRect = false;
        }
    }

    if (!removedLink
        && !m_draggingLink
        && !m_draggingInputLink
        && !m_draggingSelectionRect
        && !mouseInsideNode
        && ImGui::IsWindowHovered()
        && ImGui::IsMouseClicked(ImGuiMouseButton_Right)
        && !ImGui::IsAnyItemHovered()) {
        const ImVec2 popupPosition = node_editor::canvasMouseGraphPosition(frame);
        m_popupEditorX = popupPosition.x;
        m_popupEditorY = popupPosition.y;
        m_popupLinkFromNode = 0;
        m_popupLinkFromSocket.clear();
        m_popupLinkToNode = 0;
        m_popupLinkToSocket.clear();
        ImGui::OpenPopup(node_editor::NODE_ADD_POPUP_ID);
    }
    if (releasedLinkOnEmpty || releasedInputLinkOnEmpty) {
        const ImVec2 popupPosition = node_editor::canvasMouseGraphPosition(frame);
        m_popupEditorX = popupPosition.x;
        m_popupEditorY = popupPosition.y;
        if (releasedInputLinkOnEmpty && releasedToNode != 0 && !releasedToSocket.empty()) {
            m_popupLinkFromNode = 0;
            m_popupLinkFromSocket.clear();
            m_popupLinkToNode = releasedToNode;
            m_popupLinkToSocket = releasedToSocket;
        } else {
            m_popupLinkFromNode = releasedFromNode;
            m_popupLinkFromSocket = releasedFromSocket;
            m_popupLinkToNode = 0;
            m_popupLinkToSocket.clear();
        }
        ImGui::OpenPopup(node_editor::NODE_ADD_POPUP_ID);
    }

    bool popupDirty = false;
    if (m_popupLinkFromNode != 0 && !m_popupLinkFromSocket.empty()) {
        popupDirty = m_addMenu.drawPopupFromOutput(sceneGraph, m_popupEditorX, m_popupEditorY, m_popupLinkFromNode, m_popupLinkFromSocket);
    } else if (m_popupLinkToNode != 0 && !m_popupLinkToSocket.empty()) {
        popupDirty = m_addMenu.drawPopupToInput(sceneGraph, m_popupEditorX, m_popupEditorY, m_popupLinkToNode, m_popupLinkToSocket);
    } else {
        popupDirty = m_addMenu.drawPopup(sceneGraph, m_popupEditorX, m_popupEditorY);
    }
    if (popupDirty) {
        dirty.scene = true;
    }

    if (!pendingDelete.empty()) {
        std::sort(pendingDelete.begin(), pendingDelete.end());
        pendingDelete.erase(std::unique(pendingDelete.begin(), pendingDelete.end()), pendingDelete.end());
        for (const SdfGraphNodeId id : pendingDelete) {
            if (graph.deleteNode(id)) {
                dirty.scene = true;
            }
        }
    }

    ImGui::EndChild();
    return dirty;
}

} // namespace sdf3d
