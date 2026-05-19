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
    Material,
    Output,
};

/// Controls whether a parameter is user-editable in generic parameter UIs.
enum class SdfParameterVisibility {
    Visible,
    Hidden,
};

/// Declares one float parameter and its default value.
struct SdfParameterDefinition {
    std::string name;
    float defaultValue = 0.0f;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    float step = 0.01f;
    SdfParameterVisibility visibility = SdfParameterVisibility::Visible;
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

/// Returns metadata for one parameter, or nullptr when custom/unknown.
const SdfParameterDefinition* sdfParameterDefinition(SdfNodeType type, const std::string& name);

/// Returns false only for metadata-declared hidden parameters.
bool isSdfParameterVisible(SdfNodeType type, const std::string& name);

/// Returns node types in menu order for one category.
std::vector<SdfNodeType> sdfNodeTypesForCategory(SdfNodeCategory category);

/// Creates a default data node from metadata.
SdfNodePtr makeSdfNodeFromDefinition(SdfNodeType type);

} // namespace sdf3d
