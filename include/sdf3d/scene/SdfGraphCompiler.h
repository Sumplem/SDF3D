#pragma once

#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/scene/SdfNode.h"

#include <string>
#include <vector>

namespace sdf3d {

/// Result of lowering an editable graph into the compiler's tree model.
struct SdfGraphLowerResult {
    SdfNodePtr root;
    std::vector<std::string> errors;
};

/// Lowers the graph output path into an SDF expression tree.
SdfGraphLowerResult lowerSdfGraphToTree(const SdfGraph& graph);

} // namespace sdf3d
