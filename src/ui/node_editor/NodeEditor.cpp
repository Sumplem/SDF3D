#include "sdf3d/ui/NodeEditor.h"

#include "sdf3d/core/EventBus.h"
#include "sdf3d/scene/GraphGroupRegistry.h"
#include "sdf3d/systems/GraphSystem.h"
#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <string>
#include <vector>

#include <imgui.h>

namespace sdf3d {

void NodeEditor::setEventBus(EventBus* eventBus)
{
    m_eventBus = eventBus;
}

SdfGraph& NodeEditor::activeGraph(SceneGraph& sceneGraph, GraphGroupRegistry& groups)
{
    while (!m_groupPath.empty()) {
        GraphGroupDefinition* definition = groups.definition(m_groupPath.back());
        if (definition != nullptr) {
            return definition->subgraph;
        }
        m_groupPath.pop_back();
    }

    return sceneGraph.graph();
}

const char* NodeEditor::activeGroupName(const GraphGroupRegistry& groups) const
{
    if (m_groupPath.empty()) {
        return "root";
    }

    const GraphGroupDefinition* definition = groups.definition(m_groupPath.back());
    return definition == nullptr ? "missing" : definition->name.c_str();
}

bool NodeEditor::enterSelectedGroup(SdfGraph& graph, GraphGroupRegistry& groups)
{
    const SdfGraphNode* selected = graph.node(graph.selectedNode());
    if (selected == nullptr || selected->payload.type != SdfNodeType::Group || selected->payload.groupDefinitionId == 0) {
        return false;
    }
    if (groups.definition(selected->payload.groupDefinitionId) == nullptr) {
        return false;
    }

    m_groupPath.push_back(selected->payload.groupDefinitionId);
    m_drag = {};
    m_popup = {};
    m_selectionRect = {};
    return true;
}

bool NodeEditor::exitGroup()
{
    if (m_groupPath.empty()) {
        return false;
    }

    m_groupPath.pop_back();
    m_drag = {};
    m_popup = {};
    m_selectionRect = {};
    return true;
}

bool NodeEditor::drawBreadcrumb(GraphGroupRegistry& groups)
{
    bool changed = false;
    if (ImGui::SmallButton("root")) {
        changed = !m_groupPath.empty();
        m_groupPath.clear();
    }
    for (std::size_t i = 0; i < m_groupPath.size(); ++i) {
        ImGui::SameLine();
        ImGui::TextUnformatted(">");
        ImGui::SameLine();
        const GraphGroupDefinition* definition = groups.definition(m_groupPath[i]);
        const std::string label = std::string(definition == nullptr ? "missing" : definition->name) + "##group-breadcrumb-" + std::to_string(i);
        if (ImGui::SmallButton(label.c_str())) {
            m_groupPath.resize(i + 1);
            changed = true;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("[%s]", activeGroupName(groups));
    return changed;
}

bool NodeEditor::drawAddPopup(SdfGraph& graph)
{
    if (m_popup.linkFromNode != 0 && !m_popup.linkFromSocket.empty() && m_popup.linkToNode != 0 && !m_popup.linkToSocket.empty()) {
        return m_addMenu.drawPopupBetween(
            graph,
            m_popup.editorX,
            m_popup.editorY,
            m_popup.linkFromNode,
            m_popup.linkFromSocket,
            m_popup.linkToNode,
            m_popup.linkToSocket);
    }
    if (m_popup.linkFromNode != 0 && !m_popup.linkFromSocket.empty()) {
        return m_addMenu.drawPopupFromOutput(graph, m_popup.editorX, m_popup.editorY, m_popup.linkFromNode, m_popup.linkFromSocket);
    }
    if (m_popup.linkToNode != 0 && !m_popup.linkToSocket.empty()) {
        return m_addMenu.drawPopupToInput(graph, m_popup.editorX, m_popup.editorY, m_popup.linkToNode, m_popup.linkToSocket);
    }
    return m_addMenu.drawPopup(graph, m_popup.editorX, m_popup.editorY);
}

EditorDirtyState NodeEditor::draw(SceneGraph& sceneGraph, GraphGroupRegistry& groups)
{
    EditorDirtyState dirty;
    SdfGraph& graph = activeGraph(sceneGraph, groups);
    (void)drawBreadcrumb(groups);

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

    if (drawAddPopup(graph)) {
        dirty.scene = true;
    }

    dirty.scene = node_editor::flushPendingDeletes(graph, pendingDelete) || dirty.scene;
    if (node_editor::shortcutsEnabled()
        && graph.selectedNode() != 0
        && graph.node(graph.selectedNode()) != nullptr
        && ImGui::GetIO().KeyCtrl
        && ImGui::IsKeyPressed(ImGuiKey_G)) {
        // AGENT: Grouping deletes/replaces graph nodes, so it runs after all
        // same-frame layout/node drawing has finished using old node pointers.
        dirty.scene = GraphSystem::groupSelection(graph, groups, graph.selectedNodes(), graph.selectedNode(), "Group") != 0 || dirty.scene;
    }
    if (node_editor::shortcutsEnabled() && ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        if (ImGui::GetIO().KeyShift) {
            dirty.scene = exitGroup() || dirty.scene;
        } else {
            dirty.scene = enterSelectedGroup(graph, groups) || dirty.scene;
        }
    }

    ImGui::EndChild();
    return dirty;
}

} // namespace sdf3d
