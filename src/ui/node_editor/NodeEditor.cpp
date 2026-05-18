#include "sdf3d/ui/NodeEditor.h"

#include "sdf3d/core/EventBus.h"
#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <vector>

#include <imgui.h>

namespace sdf3d {

void NodeEditor::setEventBus(EventBus* eventBus)
{
    m_eventBus = eventBus;
}

bool NodeEditor::drawAddPopup(SceneGraph& sceneGraph)
{
    if (m_popup.linkFromNode != 0 && !m_popup.linkFromSocket.empty() && m_popup.linkToNode != 0 && !m_popup.linkToSocket.empty()) {
        return m_addMenu.drawPopupBetween(
            sceneGraph,
            m_popup.editorX,
            m_popup.editorY,
            m_popup.linkFromNode,
            m_popup.linkFromSocket,
            m_popup.linkToNode,
            m_popup.linkToSocket);
    }
    if (m_popup.linkFromNode != 0 && !m_popup.linkFromSocket.empty()) {
        return m_addMenu.drawPopupFromOutput(sceneGraph, m_popup.editorX, m_popup.editorY, m_popup.linkFromNode, m_popup.linkFromSocket);
    }
    if (m_popup.linkToNode != 0 && !m_popup.linkToSocket.empty()) {
        return m_addMenu.drawPopupToInput(sceneGraph, m_popup.editorX, m_popup.editorY, m_popup.linkToNode, m_popup.linkToSocket);
    }
    return m_addMenu.drawPopup(sceneGraph, m_popup.editorX, m_popup.editorY);
}

EditorDirtyState NodeEditor::draw(SceneGraph& sceneGraph)
{
    EditorDirtyState dirty;
    SdfGraph& graph = sceneGraph.graph();

    node_editor::CanvasFrame frame = node_editor::beginCanvas(m_canvasPanX, m_canvasPanY, m_canvasZoom);
    node_editor::updateCanvasView(frame, m_canvasPanX, m_canvasPanY, m_canvasZoom);

    node_editor::drawGrid(frame);

    std::vector<node_editor::GraphNodeLayout> layouts;
    std::vector<node_editor::GraphSocketAnchor> anchors;
    node_editor::buildLayoutsAndAnchors(graph, frame, layouts, anchors);

    const bool removedLink = node_editor::drawExistingLinks(graph, frame, anchors);
    if (removedLink) {
        dirty.scene = true;
    }

    std::vector<SdfGraphNodeId> pendingDelete;
    node_editor::drawEmptyGraphMessage(frame, layouts);

    const EditorDirtyState shortcutDirty = node_editor::handleNodeEditorShortcuts(graph, m_eventBus, frame, layouts, m_canvasPanX, m_canvasPanY, pendingDelete);
    dirty.scene = dirty.scene || shortcutDirty.scene;
    frame.pan = {m_canvasPanX, m_canvasPanY};

    const node_editor::NodeDrawResult nodeDraw = node_editor::drawGraphNodes(
        graph,
        frame,
        layouts,
        anchors,
        m_drag,
        pendingDelete);
    dirty.scene = dirty.scene || nodeDraw.dirty.scene;
    dirty.material = dirty.material || nodeDraw.dirty.material;

    if (node_editor::drawNodeDragInsertion(graph, frame, layouts, anchors, nodeDraw.activeDraggedNode, nodeDraw.releasedDraggedNode)) {
        dirty.scene = true;
    }

    node_editor::LinkDragResult linkDrag;
    const EditorDirtyState linkDragDirty = node_editor::updateNodeEditorLinkDrags(
        graph,
        frame,
        anchors,
        m_drag,
        linkDrag);
    dirty.scene = dirty.scene || linkDragDirty.scene;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool mouseInsideNode = node_editor::mouseInsideAnyNode(layouts, mouse);

    (void)node_editor::updateSelectionRectangle(
        graph,
        frame,
        layouts,
        m_drag,
        mouseInsideNode,
        m_selectionRect);

    node_editor::openNodeEditorContextPopup(
        frame,
        removedLink,
        m_drag,
        m_selectionRect,
        mouseInsideNode,
        linkDrag,
        m_popup);

    if (drawAddPopup(sceneGraph)) {
        dirty.scene = true;
    }

    dirty.scene = node_editor::flushPendingDeletes(graph, pendingDelete) || dirty.scene;

    ImGui::EndChild();
    return dirty;
}

} // namespace sdf3d
