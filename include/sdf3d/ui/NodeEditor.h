#pragma once

#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/ui/AddMenu.h"
#include "sdf3d/ui/EditorDirtyState.h"
#include "sdf3d/ui/node_editor/NodeEditorCanvas.h"

namespace sdf3d {

class EventBus;

/// Draws the graph canvas and returns true when the scene graph changes.
class NodeEditor {
public:
    void setEventBus(EventBus* eventBus);

    EditorDirtyState draw(SceneGraph& sceneGraph);

private:
    bool drawAddPopup(SceneGraph& sceneGraph);

    EventBus* m_eventBus = nullptr;
    node_editor::NodeEditorDragState m_drag;
    AddMenu m_addMenu;
    node_editor::NodeEditorPopupState m_popup;
    float m_canvasPanX = 0.0f;
    float m_canvasPanY = 0.0f;
    float m_canvasZoom = 1.0f;
    node_editor::SelectionRectState m_selectionRect;
};

} // namespace sdf3d
