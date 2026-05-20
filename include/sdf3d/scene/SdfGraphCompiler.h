#pragma once

#include "sdf3d/scene/SdfGraph.h"
#include "sdf3d/scene/SdfNode.h"

#include <string>
#include <vector>

namespace sdf3d {

class GraphGroupRegistry;

/// Result of lowering an editable graph into the compiler's tree model.
struct SdfGraphLowerResult {
    SdfNodePtr root;
    std::vector<std::string> errors;
};

/// Lowers the graph output path into an SDF expression tree.
SdfGraphLowerResult lowerSdfGraphToTree(const SdfGraph& graph);

/// Lowers the graph output path and resolves Group nodes through a registry.
SdfGraphLowerResult lowerSdfGraphToTree(const SdfGraph& graph, const GraphGroupRegistry& groups);

} // namespace sdf3d
