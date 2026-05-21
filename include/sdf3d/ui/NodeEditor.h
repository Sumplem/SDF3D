#pragma once

#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/ui/AddMenu.h"
#include "sdf3d/ui/EditorDirtyState.h"
#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

#include <vector>

namespace sdf3d {

class EventBus;
class GraphGroupRegistry;

/// Draws the graph canvas and returns true when the scene graph changes.
class NodeEditor {
public:
    void setEventBus(EventBus* eventBus);

    EditorDirtyState draw(SceneGraph& sceneGraph, GraphGroupRegistry& groups);

    /// Returns graph currently visible in node editor: root or entered group subgraph.
    SdfGraph& activeGraph(SceneGraph& sceneGraph, GraphGroupRegistry& groups);

    /// Returns display name for the active editor graph.
    const char* activeGroupName(const GraphGroupRegistry& groups) const;

    /// Clears group navigation state and returns to root graph.
    void resetActiveGraph();

private:
    bool enterSelectedGroup(SdfGraph& graph, GraphGroupRegistry& groups);
    bool enterGroupNode(SdfGraph& graph, GraphGroupRegistry& groups, SdfGraphNodeId nodeId);
    bool exitGroup();
    bool drawBreadcrumb(GraphGroupRegistry& groups);
    bool drawAddPopup(SdfGraph& graph);
    void clearTransientState();

    EventBus* m_eventBus = nullptr;
    std::vector<GroupDefId> m_groupPath;
    node_editor::NodeEditorDragState m_drag;
    AddMenu m_addMenu;
    node_editor::NodeEditorPopupState m_popup;
    float m_canvasPanX = 0.0f;
    float m_canvasPanY = 0.0f;
    float m_canvasZoom = 1.0f;
    node_editor::SelectionRectState m_selectionRect;
};

} // namespace sdf3d
