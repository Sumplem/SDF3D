#pragma once

#include "sdf3d/scene/SceneGraph.h"

namespace sdf3d {

/// Draws the Add menu and returns true when it mutates the scene graph.
class AddMenu {
public:
    bool draw(SceneGraph& sceneGraph);

private:
    void addPrimitive(SceneGraph& sceneGraph, SdfNodePtr node, bool linkToSelection = true);
};

} // namespace sdf3d
