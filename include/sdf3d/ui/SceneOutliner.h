#pragma once

#include "sdf3d/scene/SdfGraph.h"

namespace sdf3d {

class GraphGroupRegistry;

/// Draws graph outliner controls and returns true when the active graph changes.
class SceneOutliner {
public:
    bool draw(SdfGraph& graph, const GraphGroupRegistry& groups);
};

} // namespace sdf3d
