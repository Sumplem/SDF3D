#pragma once

#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/ui/AddMenu.h"
#include "sdf3d/ui/EditorDirtyState.h"

#include <string>

#include <imgui.h>

namespace sdf3d {

class EventBus;

/// Draws the graph canvas and returns true when the scene graph changes.
class NodeEditor {
public:
    void setEventBus(EventBus* eventBus);

    EditorDirtyState draw(SceneGraph& sceneGraph);

private:
    EventBus* m_eventBus = nullptr;
    bool m_draggingLink = false;
    SdfGraphNodeId m_dragOutputNode = 0;
    std::string m_dragOutputSocket;
    bool m_dragOutputFromInputDetach = false;
    SdfGraphNodeId m_inputDragCandidateNode = 0;
    std::string m_inputDragCandidateSocket;
    bool m_draggingInputLink = false;
    SdfGraphNodeId m_dragInputNode = 0;
    std::string m_dragInputSocket;
    AddMenu m_addMenu;
    float m_popupEditorX = 0.0f;
    float m_popupEditorY = 0.0f;
    SdfGraphNodeId m_popupLinkFromNode = 0;
    std::string m_popupLinkFromSocket;
    SdfGraphNodeId m_popupLinkToNode = 0;
    std::string m_popupLinkToSocket;
    float m_canvasPanX = 0.0f;
    float m_canvasPanY = 0.0f;
    float m_canvasZoom = 1.0f;
    bool m_draggingSelectionRect = false;
    ImVec2 m_selectionRectStart = {0.0f, 0.0f};
    ImVec2 m_selectionRectEnd = {0.0f, 0.0f};
};

} // namespace sdf3d
