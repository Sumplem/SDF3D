#pragma once

#include "sdf3d/scene/SdfNodeDefinition.h"

#include <cstddef>
#include <string>

namespace sdf3d::node_editor {

/// Returns true when a node parameter should be shown in inline node properties.
inline bool isInlinePropertyParameterVisible(const SdfNode& node, const std::string& key)
{
    return isSdfParameterVisible(node.type, key);
}

/// Counts only parameters rendered by inline node properties.
inline size_t visibleInlinePropertyParameterCount(const SdfNode& node)
{
    size_t count = 0;
    for (const auto& parameter : node.parameters) {
        if (isInlinePropertyParameterVisible(node, parameter.first)) {
            ++count;
        }
    }
    return count;
}

} // namespace sdf3d::node_editor
