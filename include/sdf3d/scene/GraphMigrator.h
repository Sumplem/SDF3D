#pragma once

#include "sdf3d/scene/SdfGraph.h"

namespace sdf3d {

/// Temporary helpers for one-time editable graph schema migrations.
class GraphMigrator {
public:
    /// Moves legacy primitive material payloads into explicit MaterialOverride nodes.
    static bool injectMaterialOverrides(SdfGraph& graph);
};

} // namespace sdf3d
