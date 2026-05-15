#pragma once

#include <string>
#include <unordered_map>

namespace sdf3d {

/// Supported primitive SDF source shapes.
enum class SdfPrimitiveType {
    Sphere,
    Box,
    Cylinder,
    Torus,
    Plane,
    Capsule,
    Cone,
    RoundBox,
};

/// Pure data for one primitive SDF source.
struct SdfPrimitive {
    SdfPrimitiveType type = SdfPrimitiveType::Sphere;
    std::unordered_map<std::string, float> parameters;
};

} // namespace sdf3d
