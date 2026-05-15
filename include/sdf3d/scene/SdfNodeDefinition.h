#pragma once

#include "sdf3d/scene/SdfGraph.h"

#include <string>
#include <vector>

namespace sdf3d {

/// High-level grouping used by Add menu and future node browser UI.
enum class SdfNodeCategory {
    Primitive,
    Boolean,
    Transform,
    Output,
};

/// Declares one float parameter and its default value.
struct SdfParameterDefinition {
    std::string name;
    float defaultValue = 0.0f;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float step = 0.01f;
};

/// Static metadata shared by graph sockets, Add menu, and default nodes.
struct SdfNodeDefinition {
    SdfNodeType type = SdfNodeType::Sphere;
    SdfNodeCategory category = SdfNodeCategory::Primitive;
    std::string displayName;
    std::vector<SdfParameterDefinition> parameters;
    std::vector<SdfGraphSocket> inputs;
    std::vector<SdfGraphSocket> outputs;
};

/// Returns metadata for a node type, or nullptr when not defined for Phase 1.
const SdfNodeDefinition* sdfNodeDefinition(SdfNodeType type);

/// Returns node types in menu order for one category.
std::vector<SdfNodeType> sdfNodeTypesForCategory(SdfNodeCategory category);

/// Creates a default data node from metadata.
SdfNodePtr makeSdfNodeFromDefinition(SdfNodeType type);

} // namespace sdf3d
