#pragma once

#include <string>
#include <unordered_map>

namespace sdf3d {

/// Supported SDF domain transform operations.
enum class SdfTransformType {
    Translate,
    Rotate,
    Scale,
    Repeat,
    Mirror,
    Twist,
    Bend,
};

/// Pure data for one SDF domain transform.
struct SdfTransform {
    SdfTransformType type = SdfTransformType::Translate;
    std::unordered_map<std::string, float> parameters;
};

} // namespace sdf3d
