#pragma once

#include "sdf3d/scene/SceneGraph.h"

namespace sdf3d {

/// Draws graph outliner controls and returns true when the scene graph changes.
class SceneOutliner {
public:
    bool draw(SceneGraph& sceneGraph);
};

} // namespace sdf3d
