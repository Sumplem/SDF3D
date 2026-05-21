#pragma once

#include <glm/glm.hpp>

namespace sdf3d {

enum class SdfMaterialType {
    Solid = 0,
    Checker = 1,
    ValueNoise = 2,
};

/// Per-node material parameters used by the raymarch shader.
struct SdfMaterial {
    SdfMaterialType type = SdfMaterialType::Solid;
    glm::vec3 albedo = {0.8f, 0.8f, 0.8f};
    glm::vec3 secondaryAlbedo = {0.08f, 0.08f, 0.08f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float emission = 0.0f;
    float patternScale = 4.0f;
};

} // namespace sdf3d
