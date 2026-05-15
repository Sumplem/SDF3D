#include "sdf3d/ui/NodeEditor.h"

#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <vector>

#include <imgui.h>

namespace sdf3d {

bool NodeEditor::draw(SceneGraph& sceneGraph)
{
    bool sceneDirty = false;
    SdfGraph& graph = sceneGraph.graph();

    node_editor::CanvasFrame frame = node_editor::beginCanvas(m_canvasPanX, m_canvasPanY, m_canvasZoom);
    node_editor::updateCanvasView(frame, m_canvasPanX, m_canvasPanY, m_canvasZoom);
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_P)) {
        if (node_editor::previewSelectedNode(graph)) {
            sceneDirty = true;
        }
    }

    node_editor::drawGrid(frame);

    std::vector<node_editor::GraphNodeLayout> layouts;
    std::vector<node_editor::GraphSocketAnchor> anchors;
    node_editor::buildLayoutsAndAnchors(graph, frame, layouts, anchors);

    const bool removedLink = node_editor::drawExistingLinks(graph, frame, anchors);
    if (removedLink) {
        sceneDirty = true;
    }

    SdfGraphNodeId pendingDelete = 0;
    if (layouts.empty()) {
        frame.drawList->AddText({frame.origin.x + 16.0f, frame.origin.y + 16.0f}, IM_COL32(210, 215, 225, 255), "Empty graph");
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
            sceneDirty = true;
        }
        node_editor::drawOutputPins(graph, layout, frame, m_draggingLink, m_dragOutputNode, m_dragOutputSocket, m_dragOutputFromInputDetach);
        if (node_editor::drawNodeActions(graph, layout, pendingDelete)) {
            sceneDirty = true;
        }
        if (node_editor::drawNodeInlineProperties(layout, frame)) {
            sceneDirty = true;
        }
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
                sceneDirty = true;
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
        sceneDirty = true;
    }
    if (!m_draggingLink) {
        m_dragOutputFromInputDetach = false;
        if (suppressOutputPopup) {
            releasedLinkOnEmpty = false;
        }
    }

    bool releasedInputLinkOnEmpty = false;
    if (node_editor::updateActiveInputLinkDrag(graph, frame, anchors, m_draggingInputLink, m_dragInputNode, m_dragInputSocket, releasedInputLinkOnEmpty)) {
        sceneDirty = true;
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

    if (!removedLink
        && !m_draggingLink
        && !m_draggingInputLink
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
        sceneDirty = true;
    }

    if (pendingDelete != 0) {
        graph.deleteNode(pendingDelete);
        sceneDirty = true;
    }

    ImGui::EndChild();
    return sceneDirty;
}

} // namespace sdf3d
