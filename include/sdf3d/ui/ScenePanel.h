#pragma once

#include "sdf3d/scene/SceneGraph.h"

namespace sdf3d {

/// Draws the scene outliner and updates selected node state.
class ScenePanel {
public:
    /// Renders the outliner tree content for the given scene.
    void draw(SceneGraph& sceneGraph);

private:
    void drawNode(const SdfNodePtr& node, SceneGraph& sceneGraph);
};

} // namespace sdf3d
