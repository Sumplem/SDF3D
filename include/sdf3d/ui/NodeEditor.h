#pragma once

#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/ui/AddMenu.h"

#include <string>

namespace sdf3d {

/// Draws the graph canvas and returns true when the scene graph changes.
class NodeEditor {
public:
    bool draw(SceneGraph& sceneGraph);

private:
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
};

} // namespace sdf3d
