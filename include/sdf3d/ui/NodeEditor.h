#pragma once

#include "sdf3d/scene/SceneGraph.h"

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
    SdfGraphNodeId m_inputDragCandidateNode = 0;
    std::string m_inputDragCandidateSocket;
};

} // namespace sdf3d
