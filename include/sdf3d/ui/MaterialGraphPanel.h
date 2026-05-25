#pragma once

#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/ui/EditorDirtyState.h"

#include <string>

namespace sdf3d {

/// Draws material assets and their per-material shader graphs.
class MaterialGraphPanel {
public:
    EditorDirtyState draw(SdfGraph& graph);

private:
    MaterialId m_selectedMaterial = 0;
    float m_canvasPanX = 20.0f;
    float m_canvasPanY = 20.0f;
    float m_canvasZoom = 1.0f;
    bool m_draggingLink = false;
    MaterialGraphNodeId m_dragFromNode = 0;
    std::string m_dragFromSocket;
    MaterialGraphNodeId m_dragDetachedToNode = 0;
    std::string m_dragDetachedToSocket;
    bool m_draggingInputLink = false;
    MaterialGraphNodeId m_dragToNode = 0;
    std::string m_dragToSocket;
    float m_addPopupGraphX = 80.0f;
    float m_addPopupGraphY = 80.0f;
    MaterialGraphNodeId m_addPopupLinkToNode = 0;
    std::string m_addPopupLinkToSocket;
};

} // namespace sdf3d
