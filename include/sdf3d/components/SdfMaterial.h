#pragma once

#include <glm/glm.hpp>

namespace sdf3d {

/// Per-node material parameters used by the raymarch shader.
struct SdfMaterial {
    glm::vec3 albedo = {0.8f, 0.8f, 0.8f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float emission = 0.0f;
};

} // namespace sdf3d
