#pragma once

#include <string>
#include <unordered_map>

namespace sdf3d {

/// Supported SDF graph operation nodes.
enum class SdfOperationType {
    Union,
    SmoothUnion,
    Subtract,
    SmoothSubtract,
    Intersect,
    SmoothIntersect,
    MaterialOverride,
    Output,
};

/// Pure data for one SDF boolean/material/output operation.
struct SdfOperation {
    SdfOperationType type = SdfOperationType::Union;
    std::unordered_map<std::string, float> parameters;
};

} // namespace sdf3d
