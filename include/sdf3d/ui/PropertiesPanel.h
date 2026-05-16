#pragma once

#include "sdf3d/scene/SceneGraph.h"
#include "sdf3d/ui/EditorDirtyState.h"

namespace sdf3d {

/// Draws selected node parameters and returns true when edits change the scene.
class PropertiesPanel {
public:
    EditorDirtyState draw(SceneGraph& sceneGraph);
};

} // namespace sdf3d
