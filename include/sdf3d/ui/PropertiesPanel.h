#pragma once

#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/ui/EditorDirtyState.h"

namespace sdf3d {

class GraphGroupRegistry;

/// Draws selected active-graph node parameters and returns dirty state.
class PropertiesPanel {
public:
    EditorDirtyState draw(SdfGraph& graph, GraphGroupRegistry& groups);
};

} // namespace sdf3d
